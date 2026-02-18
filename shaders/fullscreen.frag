#line 1

out vec4 FragColor;

in vec2 fragCoord; // [0,1]

const int MAX_MODELS = 64;
const int MAX_TEXTURES = 64;
const int MAX_STACK_SIZE = 48;
const int MAX_REFRACTIONS = 16;
const float PI = 3.14159265359;

layout(std430, binding = 0) buffer ssboTriangles { ivec4 triangles[]; };
layout(std430, binding = 1) buffer ssboVertices { vec4 vertices[]; };
layout(std430, binding = 2) buffer ssboTexCoords { vec2 texCoords[]; };
layout(std430, binding = 3) buffer ssboNormals { vec4 normals[]; };
layout(std430, binding = 4) buffer ssboMaterials { Material materials[]; };
layout(std430, binding = 5) buffer ssboBoundingBoxMin { BVHnode BVHnodes[]; };
layout(std430, binding = 6) buffer ssboModelOffsets { ModelOffset modelOffsets[]; };
layout(std430, binding = 7) buffer ssboModelTransformations { mat4 modelTransformations[]; };
layout(std430, binding = 8) buffer ssboModelInvTransformations { mat4 modelInvTransformations[]; };

uniform int   numModels;

uniform vec3  cameraPos;
uniform vec3  camForward;
uniform vec3  camUp;
uniform vec3  camRight;
uniform float fovDeg;
uniform float aperture;
uniform float focusDistance;
uniform bool  focusDistancePlane;

uniform uvec2 resolution;
uniform int   frameCount;
uniform float timeSinceStart;
uniform int   sampleCount;

uniform int       numNodes;
uniform int       samples;
uniform int       aa;
uniform int       bounceLim;
uniform sampler2D previousFrame;

uniform vec3  skyColor;
uniform vec3  sunDir;
uniform vec3  sunColor;

uniform bool  floorActive;
uniform vec4  floorDiffuseColor;
uniform vec4  floorSpecularColor;

uniform bool      skyActive;
uniform sampler2D skyTex;
uniform float     uEnvYaw;

uniform bool  debugView;
uniform int   debugMode;
uniform int   triTh;
uniform int   aabbTh;
uniform float depthScale;

uniform sampler2D textures[MAX_TEXTURES];
uniform float     textureScales[MAX_TEXTURES];


int   stack[MAX_STACK_SIZE];
float iorStack[MAX_REFRACTIONS];
int   iorSize = 1;

Material floorMaterial;

Triangle createTri(int triIndex, int modelIdx){
    Triangle tri;

    ivec4 idx1 = triangles[triIndex*3+0];
    ivec4 idx2 = triangles[triIndex*3+1];
    ivec4 idx3 = triangles[triIndex*3+2];

    tri.material = materials[int(idx1.w) + modelOffsets[modelIdx].material];
    tri.textureID = modelOffsets[modelIdx].textureID;
    tri.useTexture = tri.textureID != -1.0;
    tri.useNormals = idx1.z != -1.0 && idx2.z != -1.0 && idx3.z != -1.0;

    tri.v1 = vertices[idx1.x].xyz;
    tri.v2 = vertices[idx2.x].xyz;
    tri.v3 = vertices[idx3.x].xyz;

    if (tri.useTexture){
        tri.t1 = texCoords[idx1.y];
        tri.t2 = texCoords[idx2.y];
        tri.t3 = texCoords[idx3.y];
    }

    if (tri.useNormals){
        tri.n1 = normals[idx1.z].xyz;
        tri.n2 = normals[idx2.z].xyz;
        tri.n3 = normals[idx3.z].xyz;
    }

    return tri;
}

// RNG
float randomValue(inout uint state){
    state = state * 747796405u + 2891336453u;
    uint result = ((state >> ((state >> 28) + 4u)) ^ state) * 277803737u;
    result = (result >> 22) ^ result;
    return float(result) * (1.0/4294967295.0);
}
vec2 randPointDisk(inout uint state){
    float angle = randomValue(state) * 2 * PI;
    vec2 pointOnCircle = vec2(cos(angle), sin(angle));
    return pointOnCircle * sqrt(randomValue(state));
}
float randomValueNormalDistribution(inout uint state) {
    // Thanks to https://stackoverflow.com/a/6178290
    float theta = 2 * PI * randomValue(state);
    float rho = sqrt(-2 * log(randomValue(state)));
    return rho * cos(theta);
}
vec3 randPointSphere(inout uint state) {
    // Thanks to https://math.stackexchange.com/a/1585996
    float x = randomValueNormalDistribution(state);
    float y = randomValueNormalDistribution(state);
    float z = randomValueNormalDistribution(state);
    return normalize(vec3(x, y, z));
}
uint hash_u32(uint v) {
    v ^= v >> 16;
    v *= 0x7feb352du;
    v ^= v >> 15;
    v *= 0x846ca68bu;
    v ^= v >> 16;
    return v;
}
uint pixelFrameSeed(uvec2 pix) {
    uint v =
        pix.x * 0x1f123bb5u ^
        pix.y * 0x05491333u ^
        frameCount * 0x9e3779b9u;

    v = hash_u32(v);
    v ^= uint(timeSinceStart * 1000000.0);
    return hash_u32(v);
}

// Helpers
float schlick(float cos_theta, float n1, float n2) {
    if (abs(n1-n2) < 1e-3) return 0.0;
    float r0 = (n1-n2)/(n1+n2); r0 *= r0;
    return r0 + (1.0-r0)*pow(1.0-cos_theta, 5.0);
}
mat3 rotY(float a){
    float c = cos(a), s = sin(a);
    return mat3(vec3(c,0,-s), vec3(0,1,0), vec3(s,0,c));
}
vec2 dirToLatLongUV(vec3 d){
    d = normalize(d);
    float phi = atan(d.z, d.x);
    float u   = phi * (1.0/(2.0*PI)) + 0.5;
    float v   = acos(clamp(d.y,-1.0,1.0)) / PI;
    return vec2(fract(u), clamp(v,0.0,1.0));
}
vec2 seamSafeUV(vec2 uv){
    uv.x -= floor(uv.x);
    vec2 texel = 1.0/vec2(textureSize(skyTex,0));
    uv = clamp(uv, texel, 1.0-texel);
    return uv;
}

void getTriangle(int triIndex, out ivec4 t1, out ivec4 t2, out ivec4 t3){
    t1 = triangles[3*triIndex+0];
    t2 = triangles[3*triIndex+1];
    t3 = triangles[3*triIndex+2];
}

// Camera / Path
Ray calculateInitialRay(int aaCycle, vec2 screenCoord, inout uint state){
    float xi = float(aaCycle % aa);
    float yi = float(aaCycle) / float(aa);

    float ox = (xi + 0.5)/float(aa) - 0.5;
    float oy = (yi + 0.5)/float(aa) - 0.5;

    vec2 jitter = vec2(ox, oy) * vec2(2.0/resolution.x, 2.0/resolution.y);

    vec2 coord = screenCoord + jitter;

    float fovRadX = radians(fovDeg);
    coord *= tan(0.5*fovRadX);

    vec3 lensPoint = vec3(randPointDisk(state)*aperture*focusDistance, 0);
    vec3 focusPoint = focusDistance*vec3(coord, 1);

    vec3 dir = normalize(focusPoint-lensPoint);

    Ray ray;
    ray.pos = cameraPos+lensPoint;
    ray.dir = dir.x * camRight + dir.y * camUp + dir.z * camForward;
    ray.invDir = 1/ray.dir;

    return ray;
}

// Intersections
Hit rayTriangleIntersect(Ray ray, vec3 v1, vec3 v2, vec3 v3){
    Hit h;
    h.hit = false;

    const float EPS = 1e-10;

    vec3 e1 = v2 - v1;
    vec3 e2 = v3 - v1;

    vec3 p  = cross(ray.dir, e2);

    float det = dot(e1, p);

    if (abs(det) < EPS) return h;

    float invDet = 1.0/det;
    vec3 tv = ray.pos - v1;

    float u = dot(tv, p) * invDet; if (u < 0.0 || u > 1.0) return h;

    vec3 q = cross(tv, e1);

    float v = dot(ray.dir, q) * invDet; if (v < 0.0 || (u+v) > 1.0) return h;

    float t = dot(e2, q) * invDet;
    if (t < 0.0005) return h;

    h.hit = true;
    h.t = t;
    h.u = u;
    h.v = v;
    h.w = 1-u-v;
    return h;

}
float intersectAABB(Ray ray, vec3 bmin, vec3 bmax){
    vec3 t0 = (bmin - ray.pos) * ray.invDir;
    vec3 t1 = (bmax - ray.pos) * ray.invDir;
    vec3 tn = min(t0,t1);
    vec3 tf = max(t0,t1);
    float tmin = max(max(tn.x, tn.y), tn.z);
    float tmax = min(min(tf.x, tf.y), tf.z);
    return (tmax >= max(tmin,0.0)) ? tmin : 3.4e38;
}

// NORMALS (LOCAL -> WORLD)
vec3 calculateNormalLocal(Hit hit){
    Triangle tri = hit.tri;
    if (tri.useNormals){
        return normalize(tri.n1*hit.w + tri.n2*hit.u + tri.n3*hit.v);
    }
    return normalize(cross(tri.v2-tri.v1, tri.v3-tri.v1));
}
vec3 toWorldNormal(vec3 nLocal, mat4 invMat) {
    mat3 normalMat = transpose(mat3(invMat));
    return normalize(normalMat * nLocal);
}

// Environment
vec3 sampleSky(vec3 dir){
    dir = rotY(uEnvYaw) * normalize(dir);
    vec2 uv = seamSafeUV(dirToLatLongUV(dir));
    if (uv.x < 0) return vec3(1, 0, 0);
    if (uv.x > 1) return vec3(0, 1, 0);
    if (uv.y < 0) return vec3(0, 0, 1);
    if (uv.y > 1) return vec3(1, 0, 1);
    return textureLod(skyTex, uv, 0).rgb;
}
vec3 getEnviormentLight(vec3 dir){

    if (!skyActive) return vec3(0);

    vec3 sky = sampleSky(dir);
    float s  = pow(max(dot(normalize(dir), normalize(sunDir)), 0.0), 2048.0);
    return sky + sunColor * s;
}

// BRDF / Directions
vec3 calculateRandDir(vec3 normal, inout uint state){ return normalize(randPointSphere(state) + normal); }
vec3 calculateReflectDir(vec3 normal, vec3 dir){ return dir - normal*2.0*dot(dir,normal); }
vec3 calculateOpaqueDir(vec3 normal, vec3 dir, float roughness, inout uint state){
    vec3 random = calculateRandDir(normal, state);
    vec3 reflect = calculateReflectDir(normal, dir);
    return normalize(mix(reflect, random, roughness));
}
vec3 calculateRefractionDir(vec3 normal, vec3 dir, float diffuseRoughness, float specularRoughness, float ior, inout uint state, inout vec3 color){
    bool entering;
    float m1, m2;
    vec3 n = normal;

    if (dot(dir, normal) > 0.0){
        entering = false;
        m1 = iorStack[iorSize-1];
        m2 = (iorSize >= 2) ? iorStack[iorSize-2] : 1.0;
        n  = -normal;
    } else {
        entering = true;
        m1 = (iorSize >= 1) ? iorStack[iorSize-1] : 1.0;
        m2 = ior;
        n  = normal;
    }

    float eta = m1/m2;
    float cos_i = clamp(-dot(dir, n), 0.0, 1.0);
    float r0 = (m1-m2)/(m1+m2); r0*=r0;
    float reflect_prob = r0 + (1.0-r0)*pow(1.0-cos_i,5.0);

    if (randomValue(state) < reflect_prob)
    return calculateOpaqueDir(normal, dir, specularRoughness, state);

    float disc = 1.0 - eta*eta*(1.0 - cos_i*cos_i);
    if (disc < 0.0)
    return calculateOpaqueDir(normal, dir, specularRoughness, state);

    float cos_t = sqrt(disc);
    vec3 refr = eta*dir + (eta*cos_i - cos_t)*n;

    if (entering) iorStack[iorSize++] = ior; else iorSize--;

    vec3 jitter = randPointSphere(state);
    vec3 dir2   = normalize(refr + diffuseRoughness * 0.5 * jitter);
    return dir2;
}
vec3 calculateNewDirection(vec3 normal, vec3 dir, Material material, bool isSpcular, inout uint state, inout vec3 color){

    bool isTransparent = material.type == 2 && randomValue(state) <= material.transparency;

    if (isTransparent){
        return calculateRefractionDir(normal, dir, material.diffuseRoughness, material.specularRoughness, material.indexOfRefraction, state, color);
    }

    float roughness = isSpcular ? material.specularRoughness : material.diffuseRoughness;
    return calculateOpaqueDir(normal, dir, roughness, state);
}

// Material / Hit

vec2 getTexutreUV(Hit hitInfo, mat4 mat){
    Triangle tri = hitInfo.tri;

    float eps = 1e-6;
    bool tileMoreWhenScaledUp = true;

    vec2 uv = tri.t1*hitInfo.w + tri.t2*hitInfo.u + tri.t3*hitInfo.v;

    // 2) Build triangle tangent/bitangent from geometry + UVs (object/local space)
    vec3 e1 = tri.v2 - tri.v1;
    vec3 e2 = tri.v3 - tri.v1;

    vec2 duv1 = tri.t2 - tri.t1;
    vec2 duv2 = tri.t3 - tri.t1;

    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    if (abs(det) < eps) return uv;

    float r = 1.0f / det;
    vec3 T = (e1 * duv2.y - e2 * duv1.y) * r; // +U direction
    vec3 B = (e2 * duv1.x - e1 * duv2.x) * r; // +V direction

    float tLen = length(T);
    float bLen = length(B);
    if (tLen < eps || bLen < eps) return uv;

    vec3 Tn = T / tLen;
    vec3 Bn = B / bLen;

    // 3) Measure stretch along U/V directions using the linear part of the model matrix
    mat3 M = mat3(mat); // drops translation

    float sU = length(M * Tn);
    float sV = length(M * Bn);

    // Guard against numerical issues
    sU = (sU < eps) ? 1.0f : sU;
    sV = (sV < eps) ? 1.0f : sV;

    // 4) Apply scaling to UVs
    vec2 scaleUV = vec2(sU, sV);

    if (tileMoreWhenScaledUp) {
        // scale object up => UV grows => more tiling
        return uv * scaleUV;
    } else {
        // scale object up => UV shrinks => texture stretches with object
        return uv / scaleUV;
    }
}
vec3 getTextureColor(Hit hitInfo, mat4 mat){
    float scale = textureScales[hitInfo.tri.textureID];

    vec2 texCoord;
    if (scale == 0){
        texCoord = hitInfo.tri.t1*hitInfo.w + hitInfo.tri.t2*hitInfo.u + hitInfo.tri.t3*hitInfo.v;
        texCoord.y = 1.0 - texCoord.y;
    } else {
        texCoord = getTexutreUV(hitInfo, mat);

        texCoord *= scale;
    }

    return texture(textures[hitInfo.tri.textureID], texCoord).xyz;
}

bool updateColor(Hit hit, inout vec3 color, bool isSpecular, mat4 mat){
    Material material = hit.tri.material;
    bool isTransparent = material.type == 2;
    bool isEmissive = material.type == 3;

    if (isEmissive){
        color *= material.diffuseColor.rgb * material.emissionStrength;
        return true;
    }

    vec3 diffuseColor = hit.tri.useTexture ? getTextureColor(hit, mat) : material.diffuseColor.rgb;

    color *= (isSpecular || isTransparent) ? material.specularColor.rgb : diffuseColor;


    return false;
}

// Transform world ray (origin,dir) by inverse(M) to local; compute invdir.
Ray worldToLocalRay(Ray ray, mat4 invM){
    Ray rayLocal;
    rayLocal.pos    = (invM * vec4(ray.pos, 1.0)).xyz;
    rayLocal.dir    = normalize((invM * vec4(ray.dir, 0.0)).xyz); // linear part only
    rayLocal.invDir = 1.0 / rayLocal.dir;
    return rayLocal;
}

// Traverse one model’s BVH entirely in LOCAL space. Returns the best WORLD distance and indices via out params.
Hit traverseBVH(int nodeOffset, Ray ray, mat4 M, mat4 invM, float bestTW, inout int triTest, inout int aabbTest){

    Hit hit;
    hit.hit = false;

    Ray rayLocal = worldToLocalRay(ray, invM);

    hit.t = 1e30;
    if (bestTW < 1e30) {
        vec3 bestHitPosW = ray.pos + bestTW*ray.dir;
        vec3 bestHitPosL = (invM * vec4(bestHitPosW, 1)).xyz;
        hit.t = length(bestHitPosL - rayLocal.pos);
    }

    int sp = 0;
    stack[sp++] = nodeOffset;

    while (sp > 0){
        BVHnode node = BVHnodes[stack[--sp]];

        if (leaf(node)){
            int start = triStart(node);
            int num = numTri(node);
            for (int j = start; j < start+num; ++j){
                triTest++;
                ivec4 tri1, tri2, tri3;
                getTriangle(j, tri1, tri2, tri3);
                vec3 v0 = vertices[tri1.x].xyz;
                vec3 v1 = vertices[tri2.x].xyz;
                vec3 v2 = vertices[tri3.x].xyz;
                Hit h = rayTriangleIntersect(rayLocal, v0, v1, v2);

                if (h.hit && h.t < hit.t){
                    hit = h;
                    hit.triID = j;
                }
            }
        } else {
            int A = childA(node);
            int B = childB(node);
            BVHnode childA = BVHnodes[A];
            BVHnode childB = BVHnodes[B];

            aabbTest += 2;
            float dA = intersectAABB(rayLocal, getMin(childA), getMax(childA));
            float dB = intersectAABB(rayLocal, getMin(childB), getMax(childB));

            bool nearA = (dA <= dB);
            float dNear = nearA ? dA : dB;
            float dFar  = nearA ? dB : dA;
            int   iNear = nearA ? A  : B;
            int   iFar  = nearA ? B  : A;

            if (dFar < hit.t  && dFar < 1e38)  stack[sp++] = iFar;
            if (dNear < hit.t && dNear < 1e38) stack[sp++] = iNear;

            if (sp > MAX_STACK_SIZE) break;
        }
    }

    vec3 hitL = rayLocal.pos + hit.t*rayLocal.dir;
    vec3 hitW = (M * vec4(hitL, 1.0)).xyz;
    hit.t = length(hitW - ray.pos);

    return hit;
}

// Find best triangle across all models; all traversal is in LOCAL; returns Hit.
Hit findBestTri(Ray ray, out int triTest, out int aabbTest){

    Hit hit;
    hit.hit = false;
    hit.t = 1e30;

    triTest = 0; aabbTest = 0;

    // Build sorted list of models by distance
    ModelDistance modelDists[MAX_MODELS];

    for (int i = 0; i < numModels; i++){
        mat4 M    = modelTransformations[i];
        mat4 invM = modelInvTransformations[i];

        Ray rayLocal = worldToLocalRay(ray, invM);

        // Get root AABB bounds in local space
        BVHnode root = BVHnodes[modelOffsets[i].BVHnodes];

        aabbTest++;
        float tLocal = intersectAABB(rayLocal, getMin(root), getMax(root));

        if (tLocal > 1e30){
            modelDists[i].index = i;
            modelDists[i].dist2 = 3.4e38;
            continue;
        }

        vec3 hitLocal = rayLocal.dir*tLocal + rayLocal.pos;
        vec3 hitWorld = (M * vec4(hitLocal, 1)).xyz;

        vec3 diff = hitWorld-ray.pos;

        modelDists[i].index = i;
        modelDists[i].dist2 = dot(diff, diff);
    }

    // Simple insertion sort (good for small numModels)
    for (int i = 1; i < numModels; i++){
        ModelDistance key = modelDists[i];
        int j = i - 1;
        while (j >= 0 && modelDists[j].dist2 > key.dist2){
            modelDists[j + 1] = modelDists[j];
            j--;
        }
        modelDists[j + 1] = key;
    }

    // Traverse models in sorted order
    for (int i = 0; i < numModels; i++){
        if (modelDists[i].dist2 > 1e30) continue;
        int modelIdx = modelDists[i].index;

        mat4 M    = modelTransformations[modelIdx];
        mat4 invM = modelInvTransformations[modelIdx];

        Hit h = traverseBVH(
            modelOffsets[modelIdx].BVHnodes, ray, M, invM, hit.t,
            triTest, aabbTest
        );

        if (h.hit && h.t < hit.t){
            hit = h;
            hit.modelID = modelIdx;
        }
    }

    hit.tri = createTri(hit.triID, hit.modelID);

    return hit;
}

// Russian Roulette
bool russianRoulet(inout vec3 color, inout uint state){
    float p = min(max(max(color.r,color.g),color.b), 1.0);
    if (randomValue(state) >= p) return true;
    color *= 1.0/p;
    return false;
}

// Trace / Shading
bool hitTriangleUpdate(Hit hit, inout Ray ray, inout vec3 color, inout uint state){

    float u = hit.u;
    float v = hit.v;
    float w = hit.w;

    mat4 mat     = modelTransformations[hit.modelID];
    mat4 invMat  = modelInvTransformations[hit.modelID];

    Triangle tri = hit.tri;

    Material material = tri.material;

    // advance to world hit
    ray.pos += ray.dir * hit.t;

    // local normal -> world normal
    vec3 normalLocal = calculateNormalLocal(hit);
    vec3 normalWorld = toWorldNormal(normalLocal, invMat);
    if (dot(normalWorld, ray.dir) > 0.0) {
        //normalWorld = -normalWorld;
        if (material.type == 2) color *= exp(-hit.t*material.absorption * (1-material.diffuseColor.rgb));
    }

    if (debugView){
        if (debugMode == 0) color = normalWorld*0.5+0.5;
        else if (debugMode == 2)color = hit.t < depthScale ? vec3(hit.t/depthScale) : vec3(1);
        return true;
    }

    bool isSpecular = randomValue(state) <= material.specularProbability;
    isSpecular = isSpecular && (material.type != 0);

    if (updateColor(hit, color, isSpecular, mat)) return true;

    float eps = 1e-3;
    ray.dir   = calculateNewDirection(normalWorld, ray.dir, material, isSpecular, state, color);
    ray.pos += normalWorld * eps * sign(dot(ray.dir, normalWorld));
    ray.invDir = 1.0/ray.dir;
    return false;
}

bool hitFloorUpdate(inout Ray ray, inout vec3 color, inout uint state){
    float floorY = -1000.0;
    float t = (floorY - ray.pos.y)/ray.dir.y;
    if (t > 0.01 && t < 1e30){
        ray.pos += ray.dir*t;

        vec3 n = vec3(0,1,0);

        float specularProbability = floorMaterial.specularProbability;
        bool isSpecular = randomValue(state) <= specularProbability;
        isSpecular = isSpecular && (floorMaterial.type != 0);

        color *= isSpecular ? floorMaterial.specularColor.rgb : floorMaterial.diffuseColor.rgb;

        ray.dir = calculateNewDirection(n, ray.dir, floorMaterial, isSpecular, state, color);
        ray.invDir = 1.0/ray.dir;

        if (debugView){
            if (debugMode == 0) color = n*0.5+0.5;
            else if (debugMode == 2)color = t < depthScale ? vec3(t/depthScale) : vec3(1);
            return true;
        }

    } else {
        if (debugView){
            color = vec3(1);
            return true;
        }

        color *= getEnviormentLight(ray.dir);
        return true;
    }
    return false;
}

vec3 trace(Ray ray, inout uint state){
    iorStack[0] = 1.0; iorSize = 1;
    vec3 color  = vec3(1.0);

    for (int bounce = 0; bounce <= bounceLim; bounce++){
        int triTest = 0, aabbTest = 0;

        Hit hit = findBestTri(ray, triTest, aabbTest);

        if (debugView && debugMode == 1) {
            vec3 heatmap = triTest > triTh || aabbTest > aabbTh ? vec3(1) : vec3(float(triTest)/float(triTh), 0.0, float(aabbTest)/float(aabbTh));

            return heatmap;
        }

        if (hit.hit){
            if (focusDistancePlane && hit.t > focusDistance && bounce == 0) color *= vec3(0.75, 1, 0.75);

            if (hitTriangleUpdate(hit, ray, color, state)) break;
        } else if (floorActive && ray.dir.y < 0.0){
            if (hitFloorUpdate(ray, color, state)) break;
        } else {
            if (debugView){
                return vec3(1);
            }
            color *= getEnviormentLight(ray.dir);
            break;
        }

        if (russianRoulet(color, state) && bounce >= 1) return vec3(0.0);
        if (bounce == bounceLim) return vec3(0.0);
    }

    return color;
}

// Main
void main(){
    floorMaterial.diffuseColor = vec4(floorDiffuseColor.rgb, 0);
    floorMaterial.diffuseRoughness = 1.0;
    floorMaterial.specularColor = vec4(floorSpecularColor.rgb, 0);
    floorMaterial.specularRoughness = 1-floorDiffuseColor.w;
    floorMaterial.specularProbability = floorSpecularColor.w;
    floorMaterial.transparency = 0.0;
    floorMaterial.indexOfRefraction = 1.0;
    floorMaterial.absorption = 0.0;
    floorMaterial.emissionStrength = 0.0;
    floorMaterial.type = floorSpecularColor.w == -1 ? 0 : 1;

    float targetAspect = 16./9.;

    float aspect = float(resolution.x)/float(resolution.y);
    vec2  screen = vec2((2.0*fragCoord.x-1.0)*aspect, 2.0*fragCoord.y-1.0);
    if (aspect < targetAspect){
        screen *= targetAspect/vec2(aspect);
    }

    uvec2 pix = uvec2(fragCoord.x*resolution.x, fragCoord.y*resolution.y);
    uint  state = pixelFrameSeed(pix);

    vec3 total = vec3(0.0);
    int aaCycle = frameCount % (aa*aa);

    vec3 prev = texture(previousFrame, fragCoord).rgb;
    prev *= prev;

    for (int s=0; s<samples; ++s) {
        Ray ray = calculateInitialRay(aaCycle, screen, state);

        vec3 color = min(trace(ray, state), vec3(25.));

        total += color;

        aaCycle = (aaCycle+1) % (aa*aa);
    }

    total /= float(samples);

    vec3 accum = mix(prev, total, float(samples)/(float(sampleCount+samples)));
    accum = sqrt(accum);

    FragColor = vec4(accum, 1.0);
}

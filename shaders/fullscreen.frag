
out vec4 FragColor;

in vec2 fragCoord; // [0,1]

struct Triangle{
    vec3 v1, v2, v3;
    vec2 t1, t2, t3;
    vec3 n1, n2, n3;
    int matID;
    bool useTexture;
    int textureID;
    bool useNormals;
};

struct HitInfo{
    bool hit;
    Triangle tri;
    int triID;
    int modelID;
    float t;
    float u, v, w;
};

struct Ray{
    vec3 pos;
    vec3 dir;
    vec3 invDir;
};

const int MAX_MODELS = 64;
const int MAX_TEXTURES = 64;
const int MAX_STACK_SIZE = 48;
const int MAX_REFRACTIONS = 16;
const float PI = 3.14159265359;

layout(std430, binding = 0) buffer ssboTriangles { ivec4 triangles[]; };
layout(std430, binding = 1) buffer ssboVertices { vec4 vertices[]; };
layout(std430, binding = 2) buffer ssboTexCoords { vec2 texCoords[]; };
layout(std430, binding = 3) buffer ssboNormals { vec4 normals[]; };
layout(std430, binding = 4) buffer ssboColors { vec4 colors[]; };
layout(std430, binding = 5) buffer ssboSpecularColors { vec4 specularColors[]; };
layout(std430, binding = 6) buffer ssboGlassLightSettings { vec4 glassLightSettings[]; };
layout(std430, binding = 7) buffer ssboBoundingBoxMin { vec4 boundingBoxMin[]; };
layout(std430, binding = 8) buffer ssboBoundingBoxMax { vec4 boundingBoxMax[]; };
layout(std430, binding = 9) buffer ssboChildA { int vChildA[]; };
layout(std430, binding = 10) buffer ssboChildB { int vChildB[]; };
layout(std430, binding = 11) buffer ssboModels { int models[]; };
layout(std430, binding = 12) buffer ssboModelTransformations { mat4 modelTransformations[]; };
layout(std430, binding = 13) buffer ssboModelInvTransformations { mat4 modelInvTransformations[]; };

uniform int   numModels;

uniform vec3  cameraPos;
uniform vec3  camForward;
uniform vec3  camUp;
uniform vec3  camRight;
uniform float fovDeg;

uniform uvec2 resolution;
uniform int   frameCount;
uniform float timeSinceStart;

uniform int       numNodes;
uniform int       samples;
uniform int       aa;
uniform int       bounceLim;
uniform sampler2D previousFrame;

uniform vec3  skyColor;
uniform vec3  sunDir;
uniform vec3  sunColor;

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


Triangle createTri(int triIndex){
    Triangle tri;

    ivec4 idx1 = triangles[triIndex*3+0];
    ivec4 idx2 = triangles[triIndex*3+1];
    ivec4 idx3 = triangles[triIndex*3+2];

    tri.matID = int(idx1.w);
    tri.useTexture = idx2.w == 1.0;
    tri.textureID = int(idx3.w);
    tri.useNormals = idx1.z != -1 && idx2.z != -1 && idx3.z != -1;

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
uint randomValueU(inout uint state){
    state = state * 747796405u + 2891336453u;
    uint result = ((state >> ((state >> 28) + 4u)) ^ state) * 277803737u;
    result = (result >> 22) ^ result;
    return result;
}
float randomValueNormalDistribution(inout uint state){
    float theta = 2.0*PI * randomValue(state);
    float rho   = sqrt(-3.0 * log(max(1e-7, randomValue(state))));
    return rho * cos(theta);
}
vec3 randPointSphere(inout uint state){
    for (int i=0;i<10;i++){
        vec3 p = vec3(2.0*randomValue(state)-1.0,
        2.0*randomValue(state)-1.0,
        2.0*randomValue(state)-1.0);
        float m = dot(p,p);
        if (m < 1.0 && m > 0.0) return p * inversesqrt(m);
    }
    return vec3(1,0,0);
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

// Intersections
bool rayTriangleIntersect(Ray ray, vec3 v0, vec3 v1, vec3 v2, out float t, out float u, out float v){
    const float EPS = 1e-10;
    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;
    vec3 p  = cross(ray.dir, e2);
    float det = dot(e1, p);
    if (abs(det) < EPS) return false;
    float invDet = 1.0/det;
    vec3 tv = ray.pos - v0;
    u = dot(tv, p) * invDet; if (u < 0.0 || u > 1.0) return false;
    vec3 q = cross(tv, e1);
    v = dot(ray.dir, q) * invDet; if (v < 0.0 || (u+v) > 1.0) return false;
    t = dot(e2, q) * invDet;
    return t > 0.0005;
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
vec3 calculateNormalLocal(Triangle tri, float u, float v, float w){
    if (tri.useNormals){
        return normalize(tri.n1*w + tri.n2*u + tri.n3*v);
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
    return 1.3 * texture(skyTex, uv).rgb;
}
vec3 getEnviormentLight(vec3 dir){
    vec3 sky = sampleSky(dir);
    float s  = pow(max(dot(normalize(dir), normalize(sunDir)), 0.0), 2048.0);
    return sky + sunColor * s;
}

// BRDF / Directions
vec3 calculateRandDir(vec3 n, inout uint st){ return normalize(randPointSphere(st) + n); }
vec3 calculateReflectDir(vec3 n, vec3 d){ return d - n*2.0*dot(d,n); }
vec3 calculateOpaqueDir(vec3 n, vec3 d, float sm, inout uint st){
    vec3 r = calculateRandDir(n, st);
    vec3 m = calculateReflectDir(n, d);
    return normalize(mix(r, m, sm));
}
vec3 calculateRefractionDir(vec3 normal, vec3 dir, float smoothness, float ior, float specularProb, inout uint state, inout vec3 color){
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
    return calculateOpaqueDir(normal, dir, smoothness, state);

    float disc = 1.0 - eta*eta*(1.0 - cos_i*cos_i);
    if (disc < 0.0)
    return calculateOpaqueDir(normal, dir, smoothness, state);

    float cos_t = sqrt(disc);
    vec3 refr = eta*dir + (eta*cos_i - cos_t)*n;

    if (entering) iorStack[iorSize++] = ior; else iorSize--;

    vec3 rand = calculateRandDir(entering ? normal : -normal, state);
    refr = mix(rand, normalize(refr), specularProb);
    return normalize(refr);
}
vec3 calculateNewDirection(vec3 normal, vec3 dir, float smoothness, float specularProb, float transparency, float ior, inout uint state, inout vec3 color){
    if (randomValue(state) <= transparency)
    return calculateRefractionDir(normal, dir, smoothness, ior, specularProb, state, color);
    return calculateOpaqueDir(normal, dir, smoothness, state);
}

// Material / Hit

vec2 getTexutreUV(HitInfo hitInfo, mat4 mat){
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
vec3 getTextureColor(HitInfo hitInfo, mat4 mat){
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

bool updateColor(HitInfo hitInfo, inout vec3 color, bool isSpecular, bool diffuseOnly, mat4 mat){
    int matID = hitInfo.tri.matID;

    vec3 diffuseColor = hitInfo.tri.useTexture ? getTextureColor(hitInfo, mat) : colors[matID].xyz;

    vec3 selected = (isSpecular && !diffuseOnly) ? specularColors[matID].xyz : diffuseColor;

    color *= selected;

    if (glassLightSettings[matID].z > 0.0) { color *= glassLightSettings[matID].z; return true; }

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
HitInfo traverseBVH(int nodeOffset, Ray ray, mat4 M, mat4 invM, float bestTW, inout int triTest, inout int aabbTest){

    HitInfo hitInfo;
    hitInfo.hit = false;

    Ray rayLocal = worldToLocalRay(ray, invM);

    vec3 bestHitPosW = ray.pos + bestTW*ray.dir;
    vec3 bestHitPosL = (invM * vec4(bestHitPosW, 1)).xyz;

    float bestTL = dot(bestHitPosL - rayLocal.pos, rayLocal.dir);

    int sp = 0;
    stack[sp++] = nodeOffset;

    while (sp > 0){
        int node = stack[--sp];
        int A = vChildA[node];
        int B = vChildB[node];

        if (A <= 0){
            int triStart = -A;
            int triCount = -B;
            for (int j = triStart; j < triStart+triCount; ++j){
                triTest++;
                ivec4 tri1, tri2, tri3;
                getTriangle(j, tri1, tri2, tri3);
                vec3 v0 = vertices[tri1.x].xyz;
                vec3 v1 = vertices[tri2.x].xyz;
                vec3 v2 = vertices[tri3.x].xyz;
                float tL, u, v;
                if (!rayTriangleIntersect(rayLocal, v0, v1, v2, tL, u, v)) continue;

                // local hit -> world hit distance
                vec3 hitL = rayLocal.pos + tL*rayLocal.dir;
                vec3 hitW = (M * vec4(hitL, 1.0)).xyz;
                float tW = length(hitW - ray.pos);

                if (tL < bestTL){
                    hitInfo.hit = true;
                    hitInfo.triID = j;
                    hitInfo.t = tW;
                    hitInfo.u = u;
                    hitInfo.v = v;
                    hitInfo.w = 1-u-v;

                    bestTL = tL;
                }
            }
        } else {
            vec3 Amin = boundingBoxMin[A].xyz;
            vec3 Amax = boundingBoxMax[A].xyz;
            vec3 Bmin = boundingBoxMin[B].xyz;
            vec3 Bmax = boundingBoxMax[B].xyz;

            aabbTest += 2;
            float dA = intersectAABB(rayLocal, Amin, Amax);
            float dB = intersectAABB(rayLocal, Bmin, Bmax);

            bool nearA = (dA <= dB);
            float dNear = nearA ? dA : dB;
            float dFar  = nearA ? dB : dA;
            int   iNear = nearA ? A  : B;
            int   iFar  = nearA ? B  : A;

            if (dFar < bestTL  && dFar < 1e38)  stack[sp++] = iFar;
            if (dNear < bestTL && dNear < 1e38) stack[sp++] = iNear;

            if (sp > MAX_STACK_SIZE) break;
        }
    }

    return hitInfo;
}

// Find best triangle across all models; all traversal is in LOCAL; returns HitInfo.
HitInfo findBestTri_world(Ray ray, out int triTest, out int aabbTest){

    HitInfo hitInfo;
    hitInfo.hit = false;
    hitInfo.t = 1e30;

    triTest = 0; aabbTest = 0;

    // Build sorted list of models by distance
    struct ModelDistance {
        int index;
        float dist2;
    };
    ModelDistance modelDists[MAX_MODELS];

    for (int i = 0; i < numModels; i++){
        mat4 M    = modelTransformations[i];
        mat4 invM = modelInvTransformations[i];

        Ray rayLocal = worldToLocalRay(ray, invM);

        // Get root AABB bounds in local space
        int rootNode = models[i];
        vec3 min = boundingBoxMin[rootNode].xyz;
        vec3 max = boundingBoxMax[rootNode].xyz;

        float tLocal = intersectAABB(rayLocal, min, max);

        if (tLocal > 1e30){
            modelDists[i].index = i;
            modelDists[i].dist2 = 3.4e38;
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
        int modelIdx = modelDists[i].index;

        mat4 M    = modelTransformations[modelIdx];
        mat4 invM = modelInvTransformations[modelIdx];

        HitInfo hit = traverseBVH(
        models[modelIdx], ray, M, invM, hitInfo.t,
        triTest, aabbTest
        );

        if (hit.hit && hit.t < hitInfo.t){
            hitInfo = hit;
            hitInfo.modelID = modelIdx;
        }
    }

    hitInfo.tri = createTri(hitInfo.triID);

    return hitInfo;
}

// Camera / Path
vec3 calculateInitialDir(int aaCycle, vec2 screenCoord){
    float xi = float(aaCycle % aa);
    float yi = float(aaCycle) / float(aa);
    float ox = (xi + 0.5)/float(aa) - 0.5;
    float oy = (yi + 0.5)/float(aa) - 0.5;
    ox /= float(resolution.x)/2.0;
    oy /= float(resolution.y)/2.0;
    vec2 coord = screenCoord + vec2(ox,oy);
    float fovRadX = radians(fovDeg);
    coord *= tan(0.5*fovRadX);
    vec3 d = camForward + camRight*coord.x + camUp*coord.y;
    return normalize(d);
}

// Russian Roulette
bool russianRoulet(inout vec3 color, inout uint state){
    float p = min(max(max(color.r,color.g),color.b)*5.0, 1.0);
    if (randomValue(state) >= p) return true;
    color *= 1.0/p;
    return false;
}

// Trace / Shading
bool hitTriangleUpdate(HitInfo hitInfo, inout Ray ray, inout vec3 color, inout uint state){

    float u = hitInfo.u;
    float v = hitInfo.v;
    float w = hitInfo.w;

    mat4 mat     = modelTransformations[hitInfo.modelID];
    mat4 invMat  = modelInvTransformations[hitInfo.modelID];

    Triangle tri = hitInfo.tri;

    // advance to world hit
    ray.pos += ray.dir * hitInfo.t;

    // local normal -> world normal
    vec3 nL = calculateNormalLocal(tri, u, v, w);
    vec3 nW = toWorldNormal(nL, invMat);
    //if (dot(nW, ray.dir) > 0.0) nW = -nW;

    if (debugView){
        if (debugMode == 0) color = nW*0.5+0.5;
        else if (debugMode == 2)color = hitInfo.t < depthScale ? vec3(hitInfo.t/depthScale) : vec3(1);
        return true;
    }

    float specP   = specularColors[tri.matID].w;
    bool  diffOnly = (specP == -1.0);
    bool  isSpec   = randomValue(state) <= specP;

    if (updateColor(hitInfo, color, isSpec, diffOnly, mat)) return true;

    float sm    = (isSpec || diffOnly) ? colors[tri.matID].w : 0.0;
    float trans = glassLightSettings[tri.matID].x;
    float ior   = glassLightSettings[tri.matID].y;

    ray.dir   = calculateNewDirection(nW, ray.dir, sm, specP, trans, ior, state, color);
    ray.invDir = 1.0/ray.dir;
    return false;
}

bool hitFloorUpdate(inout Ray ray, inout vec3 color, inout uint state){
    float floorY = -1000.0;
    float t = (floorY - ray.pos.y)/ray.dir.y;
    if (t > 0.01 && t < 1e30){
        ray.pos += ray.dir*t;
        vec3 n = vec3(0,1,0);
        color *= vec3(0.9);
        ray.dir = calculateNewDirection(n, ray.dir, 0.0, 0.0, 0.0, 1.0, state, color);
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

    for (int i=0;i<=bounceLim;i++){
        int triTest = 0, aabbTest = 0;

        HitInfo hitInfo = findBestTri_world(ray, triTest, aabbTest);

        if (debugView && debugMode == 1) {
            vec3 heatmap = triTest > triTh || aabbTest > aabbTh ? vec3(1) : vec3(float(triTest)/float(triTh), 0.0, float(aabbTest)/float(aabbTh));

            return heatmap;
        }

        if (hitInfo.hit){
            if (hitTriangleUpdate(hitInfo, ray, color, state)) break;
        } else if (ray.dir.y < 0.0){
            if (hitFloorUpdate(ray, color, state)) break;
        } else {
            if (debugView){
                return vec3(1);
            }
            color *= getEnviormentLight(ray.dir);
            break;
        }

        if (russianRoulet(color, state)) return vec3(0.0);
        if (i == bounceLim) return vec3(0.0);
    }

    return color;
}

// Main
void main(){
    float aspect = float(resolution.x)/float(resolution.y);
    vec2  screen = vec2((2.0*fragCoord.x-1.0)*aspect, 2.0*fragCoord.y-1.0);

    uvec2 pix = uvec2(fragCoord.x*resolution.x, fragCoord.y*resolution.y*aspect);
    uint  state = pixelFrameSeed(pix);

    vec3 total = vec3(0.0);
    int aaCycle = frameCount % (aa*aa);

    for (int s=0; s<samples; ++s){
        Ray ray;
        ray.pos = cameraPos;
        ray.dir = calculateInitialDir(aaCycle, screen);
        ray.invDir = 1/ray.dir;

        total += trace(ray, state);
        aaCycle = (aaCycle+1) % (aa*aa);
    }

    total /= float(samples);
    total = sqrt(total); // gamma approx

    vec3 prev = texture(previousFrame, fragCoord).rgb;
    vec3 accum = mix(prev, total, 1.0/(float(frameCount)+1.0));

    FragColor = vec4(accum, 1.0);
}

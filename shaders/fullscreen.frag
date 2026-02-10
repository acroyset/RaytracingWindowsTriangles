#version 430 core
out vec4 FragColor;

in vec2 fragCoord; // [0,1]

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
uniform uvec2 resolution;
uniform int   frameCount;
uniform int   numNodes;
uniform int   samples;
uniform int   aa;
uniform int   bounceLim;
uniform sampler2D previousFrame;

uniform vec3  skyColor;
uniform vec3  sunDir;
uniform vec3  sunColor;

uniform sampler2D uEnvLatLong;
uniform float     uEnvYaw;

uniform bool debugView;
uniform int triTh;
uniform int aabbTh;

const float PI = 3.14159265359;

const int MAX_STACK_SIZE = 48;
int   stack[MAX_STACK_SIZE];
float iorStack[16];
int   iorSize = 1;

/* ========================= RNG ========================= */
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
uint pixelFrameSeed(uvec2 pix, uint frame) {
    uint v = pix.x * 0x1f123bb5u ^ pix.y * 0x05491333u ^ frame * 0x9e3779b9u;
    return hash_u32(v);
}

/* ========================= Helpers ========================= */
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
    vec2 texel = 1.0/vec2(textureSize(uEnvLatLong,0));
    uv = clamp(uv, texel, 1.0-texel);
    return uv;
}

/* ========================= Intersections ========================= */
bool rayTriangleIntersect(vec3 rayOrigin, vec3 rayDir, vec3 v0, vec3 v1, vec3 v2, out float t, out float u, out float v){
    const float EPS = 1e-10;
    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;
    vec3 p  = cross(rayDir, e2);
    float det = dot(e1, p);
    if (abs(det) < EPS) return false;
    float invDet = 1.0/det;
    vec3 tv = rayOrigin - v0;
    u = dot(tv, p) * invDet; if (u < 0.0 || u > 1.0) return false;
    vec3 q = cross(tv, e1);
    v = dot(rayDir, q) * invDet; if (v < 0.0 || (u+v) > 1.0) return false;
    t = dot(e2, q) * invDet;
    return t > 0.0005;
}
float intersectAABB(vec3 rayOrigin, vec3 invRayDir, vec3 bmin, vec3 bmax){
    vec3 t0 = (bmin - rayOrigin) * invRayDir;
    vec3 t1 = (bmax - rayOrigin) * invRayDir;
    vec3 tn = min(t0,t1);
    vec3 tf = max(t0,t1);
    float tmin = max(max(tn.x, tn.y), tn.z);
    float tmax = min(min(tf.x, tf.y), tf.z);
    return (tmax >= max(tmin,0.0)) ? tmin : 1e35;
}

/* ======= NORMALS (LOCAL -> WORLD) ======= */
vec3 calculateNormalLocal(int triIndex, float u, float v, float w){
    ivec4 tri1 = triangles[3*triIndex+0];
    ivec4 tri2 = triangles[3*triIndex+1];
    ivec4 tri3 = triangles[3*triIndex+2];
    if (tri1.z != -1 && tri2.z != -1 && tri3.z != -1){
        vec3 na = normals[tri1.z].xyz;
        vec3 nb = normals[tri2.z].xyz;
        vec3 nc = normals[tri3.z].xyz;
        return normalize(na*w + nb*u + nc*v);
    }
    vec3 v0 = vertices[tri1.x].xyz;
    vec3 v1 = vertices[tri2.x].xyz;
    vec3 v2 = vertices[tri3.x].xyz;
    return normalize(cross(v1-v0, v2-v0));
}
vec3 toWorldNormal(vec3 nLocal, mat4 M){
    mat3 N = transpose(inverse(mat3(M)));
    return normalize(N * nLocal);
}

/* ========================= Environment ========================= */
vec3 sampleSky(vec3 dir){
    dir = rotY(uEnvYaw) * normalize(dir);
    vec2 uv = seamSafeUV(dirToLatLongUV(dir));
    return 1.3 * texture(uEnvLatLong, uv).rgb;
}
vec3 getEnviormentLight(vec3 dir){
    vec3 sky = sampleSky(dir);
    float s  = pow(max(dot(normalize(dir), normalize(sunDir)), 0.0), 2048.0);
    return sky + sunColor * s;
}

/* ========================= BRDF / Directions ========================= */
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

/* ========================= Material / Hit ========================= */
bool updateColor(inout vec3 color, int mat_i, bool isSpecular, bool diffuseOnly){
    vec3 selected = (isSpecular && !diffuseOnly) ? specularColors[mat_i].xyz : colors[mat_i].xyz;
    color *= selected;
    if (glassLightSettings[mat_i].z > 0.0) { color *= glassLightSettings[mat_i].z; return true; }
    return false;
}

/* ========================= BVH TRAVERSAL (RAY -> LOCAL) ========================= */

/* Transform world ray (origin,dir) by inverse(M) to local; compute invdir. */
void worldToLocalRay(vec3 rayOriginWorld, vec3 rayDirWorld, mat4 invM, out vec3 rayOriginLocal, out vec3 rayDirLocal, out vec3 invRayDirLocal){
    rayOriginLocal = (invM * vec4(rayOriginWorld, 1.0)).xyz;
    rayDirLocal    = (invM * vec4(rayDirWorld, 0.0)).xyz; // linear part only
    invRayDirLocal = 1.0 / rayDirLocal;
}

/* Traverse one model’s BVH entirely in LOCAL space.
   Returns the best WORLD distance and indices via out params. */
void traverseBVH_local(
    int nodeOffset,
    vec3 rayOriginWorld, vec3 rayDirWorld,
    mat4 M, mat4 invM,
    inout float best_t_world,
    inout float best_u, inout float best_v,
    inout int triTest, inout int aabbTest,
    inout int best_tri_i, inout int best_model_i,
    int modelIndex){

    vec3 rayOriginLocal, rayDirLocal, invRayDirLocal;
    worldToLocalRay(rayOriginWorld, rayDirWorld, invM, rayOriginLocal, rayDirLocal, invRayDirLocal);

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
                ivec4 tri1 = triangles[j*3+0];
                ivec4 tri2 = triangles[j*3+1];
                ivec4 tri3 = triangles[j*3+2];
                vec3 v0 = vertices[tri1.x].xyz;
                vec3 v1 = vertices[tri2.x].xyz;
                vec3 v2 = vertices[tri3.x].xyz;
                float tL, u, v;
                if (!rayTriangleIntersect(rayOriginLocal, rayDirLocal, v0, v1, v2, tL, u, v)) continue;

                // local hit -> world hit distance
                vec3 hitL = rayOriginLocal + tL*rayDirLocal;
                vec3 hitW = (M * vec4(hitL, 1.0)).xyz;
                float tW = length(hitW - rayOriginWorld);

                if (tW < best_t_world){
                    best_t_world = tW;
                    best_tri_i   = j;
                    best_model_i = modelIndex;
                    best_u = u; best_v = v;
                }
            }
        } else {
            vec3 Amin = boundingBoxMin[A].xyz;
            vec3 Amax = boundingBoxMax[A].xyz;
            vec3 Bmin = boundingBoxMin[B].xyz;
            vec3 Bmax = boundingBoxMax[B].xyz;

            aabbTest += 2;
            float dA = intersectAABB(rayOriginLocal, invRayDirLocal, Amin, Amax);
            float dB = intersectAABB(rayOriginLocal, invRayDirLocal, Bmin, Bmax);

            bool nearA = (dA <= dB);
            float dNear = nearA ? dA : dB;
            float dFar  = nearA ? dB : dA;
            int   iNear = nearA ? A  : B;
            int   iFar  = nearA ? B  : A;

            // prune by converting local distance approx to world scale
            float scaleApprox = length((invM * vec4(rayDirWorld,0.0)).xyz);
            float worldNear   = dNear * scaleApprox;
            float worldFar    = dFar  * scaleApprox;

            if (worldNear < best_t_world) stack[sp++] = iNear;
            if (worldFar < best_t_world && dFar < 1e29) stack[sp++] = iFar;

            if (sp > MAX_STACK_SIZE) break;
        }
    }
}

/* Find best triangle across all models; all traversal is in LOCAL; returns WORLD t. */
float findBestTri_world(
    vec3 rayOriginWorld, vec3 rayDirWorld,
    out int best_tri_i, out int best_model_i,
    out int triTest, out int aabbTest,
    out float best_u, out float best_v, out float best_w){
    best_tri_i = -1;
    best_model_i = -1;
    triTest = 0; aabbTest = 0;
    float best_tW = 1e30;

    for (int i=0;i<numModels;i++){
        mat4 M    = modelTransformations[i];
        mat4 invM = modelInvTransformations[i];
        traverseBVH_local(models[i], rayOriginWorld, rayDirWorld, M, invM,
        best_tW, best_u, best_v,
        triTest, aabbTest,
        best_tri_i, best_model_i,
        i);
    }
    best_w = 1.0 - best_u - best_v;
    return best_tW;
}

/* ========================= Camera / Path ========================= */
vec3 calculateInitialDir(int aaCycle, vec2 screenCoord){
    float xi = float(aaCycle % aa);
    float yi = float(aaCycle) / float(aa);
    float ox = (xi + 0.5)/float(aa) - 0.5;
    float oy = (yi + 0.5)/float(aa) - 0.5;
    ox /= float(resolution.x)/2.0;
    oy /= float(resolution.y)/2.0;
    vec2 coord = screenCoord + vec2(ox,oy);
    float fovDegX = 60.0;
    float fovRadX = radians(fovDegX);
    coord *= tan(0.5*fovRadX);
    vec3 d = camForward + camRight*coord.x + camUp*coord.y;
    return normalize(d);
}

/* ========================= Russian Roulette ========================= */
bool russianRoulet(inout vec3 color, inout uint state){
    float p = min(max(max(color.r,color.g),color.b)*5.0, 1.0);
    if (randomValue(state) >= p) return true;
    color *= 1.0/p;
    return false;
}

/* ========================= Trace / Shading ========================= */

bool hitTriangleUpdateWorld(
    int triIndex, int modelIndex, float tWorld,
    float u, float v, float w,
    inout vec3 posW, inout vec3 dirWorld, inout vec3 invDirWorld,
    inout vec3 color, inout uint state){
    // advance to world hit
    posW += dirWorld * tWorld;

    int material_i = triangles[triIndex*3].w;

    // local normal -> world normal
    vec3 nL = calculateNormalLocal(triIndex, u, v, w);
    mat4 M  = modelTransformations[modelIndex];
    vec3 nW = toWorldNormal(nL, M);
    if (dot(nW, dirWorld) > 0.0) nW = -nW;

    float specP   = specularColors[material_i].w;
    bool  diffOnly = (specP == -1.0);
    bool  isSpec   = randomValue(state) <= specP;

    if (updateColor(color, material_i, isSpec, diffOnly)) return true;

    float sm   = (isSpec || diffOnly) ? colors[material_i].w : 0.0;
    float trans    = glassLightSettings[material_i].x;
    float ior      = glassLightSettings[material_i].y;

    dirWorld   = calculateNewDirection(nW, dirWorld, sm, specP, trans, ior, state, color);
    invDirWorld = 1.0/dirWorld;
    return false;
}

bool hitFloorUpdate(inout vec3 pos, inout vec3 dir, inout vec3 invDir, inout vec3 color, inout uint state){
    float floorY = -1000.0;
    float t = (floorY - pos.y)/dir.y;
    if (t > 0.01 && t < 1e7){
        pos += dir*t;
        vec3 n = vec3(0,1,0);
        color *= vec3(0.9);
        dir = calculateNewDirection(n, dir, 0.0, 0.0, 0.0, 1.0, state, color);
        invDir = 1.0/dir;
    } else {
        color *= getEnviormentLight(dir);
        return true;
    }
    return false;
}

vec3 trace(vec3 pos, vec3 dir, inout uint state){
    iorStack[0] = 1.0; iorSize = 1;
    vec3 invDir = 1.0/dir;
    vec3 color  = vec3(1.0);

    for (int i=0;i<bounceLim;i++){
        int triTest, aabbTest, tri_i, model_i;
        float bu, bv, bw;

        float tW = findBestTri_world(pos, dir, tri_i, model_i, triTest, aabbTest, bu, bv, bw);

        if (debugView) {

            vec3 heatmap = triTest > triTh || aabbTest > aabbTh ? vec3(1) : vec3(float(triTest)/float(triTh), 0.0, float(aabbTest)/float(aabbTh));

            return heatmap;
        }

        if (tri_i != -1){
            if (hitTriangleUpdateWorld(tri_i, model_i, tW, bu, bv, bw, pos, dir, invDir, color, state)) break;
        } else if (dir.y < 0.0){
            if (hitFloorUpdate(pos, dir, invDir, color, state)) break;
        } else {
            color *= getEnviormentLight(dir);
            break;
        }

        if (russianRoulet(color, state)) return vec3(0.0);
        if (i == bounceLim-1) return vec3(0.0);
    }
    return color;
}

/* ========================= Main ========================= */
void main(){
    float aspect = float(resolution.x)/float(resolution.y);
    vec2  screen = vec2((2.0*fragCoord.x-1.0)*aspect, 2.0*fragCoord.y-1.0);

    uvec2 pix = uvec2(fragCoord.x*resolution.x, fragCoord.y*resolution.y*aspect);
    uint  state = pixelFrameSeed(pix, frameCount);

    vec3 total = vec3(0.0);
    int aaCycle = frameCount % (aa*aa);

    for (int s=0; s<samples; ++s){
        vec3 ro = cameraPos;
        vec3 rd = calculateInitialDir(aaCycle, screen);
        total += trace(ro, rd, state);
        aaCycle = (aaCycle+1) % (aa*aa);
    }

    total /= float(samples);
    total = sqrt(total); // gamma approx

    vec3 prev = texture(previousFrame, fragCoord).rgb;
    vec3 accum = mix(prev, total, 1.0/(float(frameCount)+1.0));

    FragColor = vec4(accum, 1.0);
}

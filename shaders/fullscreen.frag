#version 430 core

out vec4 FragColor;

in vec2 fragCoord; // From vertex shader, in range [0,1]

layout(std430, binding = 0) buffer ssboVertices {
    vec4 vertices[];
};
layout(std430, binding = 1) buffer ssboTriangles {
    ivec4 triangles[];
};
layout(std430, binding = 2) buffer ssboColors {
    vec4 colors[];
};
layout(std430, binding = 3) buffer ssboNormals {
    vec4 normals[];
};
layout(std430, binding = 4) buffer ssboEmission {
    float emission[];
};
layout(std430, binding = 5) buffer ssboBoundingBoxMin {
    vec4 boundingBoxMin[];
};
layout(std430, binding = 6) buffer ssboBoundingBoxMax {
    vec4 boundingBoxMax[];
};
layout(std430, binding = 7) buffer ssboModels {
    int models[];
};

uniform int numModels;
uniform vec3 cameraPos;
uniform vec3 camForward;
uniform vec3 camUp;
uniform vec3 camRight;
uniform vec2 resolution;
uniform int frameCount;
uniform int numNodes;
uniform int samples;
uniform int aa;
uniform int bounceLim;
uniform sampler2D uPrevFrame;   // Previous accumulated result
uniform uint time;

const int MAX_STACK_SIZE = 33;
int stack[MAX_STACK_SIZE];

float randomValue(inout uint state){
    state = state * 747796405u + 2891336453u;
    uint result = ((state >> ((state >> 28) + 4u)) ^ state) * 277803737u;
    result = (result >> 22) ^ result;
    return float(result) * (1/4294967295.0);
}
uint randomValueU(inout uint state){
    state = state * 747796405u + 2891336453u;
    uint result = ((state >> ((state >> 28) + 4u)) ^ state) * 277803737u;
    result = (result >> 22) ^ result;
    return result;
}
float randomValueNormalDistribution(inout uint state){
    float theta = 2 * 3.1415926 * randomValue(state);
    float rho = sqrt(-3 * log(randomValue(state)));
    return rho * cos(theta);
}
vec3 randPointSphere(inout uint state){
    vec3 pos;
    for (int i = 0; i < 10; i++){
        pos.x = 2*randomValue(state)-1;
        pos.y = 2*randomValue(state)-1;
        pos.z = 2*randomValue(state)-1;
        float mag = dot(pos,pos);
        if (mag < 1 && mag != 0){
            return pos / sqrt(mag);
        }
    }
    return vec3(1,0,0);
}
vec3 randPointSphereN(inout uint state){
    float x = randomValueNormalDistribution(state);
    float y = randomValueNormalDistribution(state);
    float z = randomValueNormalDistribution(state);
    return vec3(x, y, z);
}

bool rayTriangleIntersect(vec3 rayOrig, vec3 rayDir, vec3 v0, vec3 v1, vec3 v2, out float t, out float u, out float v){
    const float EPSILON = 0.01;
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;

    vec3 pvec = cross(rayDir, edge2);
    float det = dot(edge1, pvec);

    if (abs(det) < EPSILON)
    return false; // Ray parallel to triangle

    float invDet = 1.0 / det;
    vec3 tvec = rayOrig - v0;

    u = dot(tvec, pvec) * invDet;
    if (u < 0.0 || u > 1.0)
    return false;

    vec3 qvec = cross(tvec, edge1);

    v = dot(rayDir, qvec) * invDet;
    if (v < 0.0 || (u + v) > 1.0)
    return false;

    t = dot(edge2, qvec) * invDet;

    if (t < 0.1)
    return false;

    return true;
}
float sphereRayCollision(vec3 rayPos, vec3 rayDir, vec3 spherePos, float sphereRadius){
    float epsilon = 0.1;
    vec3 ray_pos = rayPos - spherePos;
    float d = dot(ray_pos, rayDir);

    float discriminant = d*d - dot(ray_pos, ray_pos) + sphereRadius*sphereRadius;

    if (discriminant == 0){
        float t = -d;
        if (t > epsilon){
            return t;
        }
    }
    if (discriminant > 0){
        float sq = sqrt(discriminant);
        float t = -d - sq;
        if (t > epsilon){
            return t;
        }
        t = -d + sq;
        if (t > epsilon){
            return t;
        }
    }
    return -1;
}
float intersectAABB(vec3 rayOrigin, vec3 rayInvDir, vec3 boxMin, vec3 boxMax) {
    vec3 t0 = (boxMin - rayOrigin) * rayInvDir;
    vec3 t1 = (boxMax - rayOrigin) * rayInvDir;

    vec3 tNear = min(t0, t1);
    vec3 tFar  = max(t0, t1);

    float tMin = max(max(tNear.x, tNear.y), tNear.z);
    float tMax = min(min(tFar.x,  tFar.y),  tFar.z);

    bool didHit = tMax >= max(tMin, 0.0);
    return didHit? tMin : 1000000000;
}

void traverseBVH(int nodeOffset, vec3 rayPos, vec3 rayDir, vec3 invRayDir, inout float best_t, inout float best_u, inout float best_v, inout int triTest, inout int aabbTest, inout int best_tri_i) {
    int stackPtr = 0;
    stack[stackPtr++] = nodeOffset;  // start from root node  index=nodeOffset

    while (stackPtr > 0) {
        int nodeIndex = stack[--stackPtr];
        vec4 bboxMin_childA = boundingBoxMin[nodeIndex];
        vec4 bboxMax_childB = boundingBoxMax[nodeIndex];

        if (bboxMin_childA.w <= 0) {
            // Intersect ray with all triangles in the leaf node
            int triStart = -int(bboxMin_childA.w);
            int numTris = -int(bboxMax_childB.w);
            for (int j = triStart; j < triStart+numTris; j++){
                float t = -1;
                triTest++;
                ivec4 tri = triangles[j];
                vec4 v1 = vertices[tri.x];
                vec4 v2 = vertices[tri.y];
                vec4 v3 = vertices[tri.z];
                float u, v;
                if (!rayTriangleIntersect(rayPos, rayDir, v1.xyz, v2.xyz, v3.xyz, t, u, v)) continue;
                if (t < best_t) {
                    best_t = t;
                    best_tri_i = j;
                    best_u = u;
                    best_v = v;
                }
            }
        }
        else {
            // Push children onto the stack

            int childIndexA = int(bboxMin_childA.w);
            int childIndexB = int(bboxMax_childB.w);
            vec4 AbboxMin_childA = boundingBoxMin[childIndexA];
            vec4 AbboxMax_childB = boundingBoxMax[childIndexA];
            vec4 BbboxMin_childA = boundingBoxMin[childIndexB];
            vec4 BbboxMax_childB = boundingBoxMax[childIndexB];

            aabbTest += 2;
            float disA = intersectAABB(rayPos, invRayDir, AbboxMin_childA.xyz, AbboxMax_childB.xyz);
            float disB = intersectAABB(rayPos, invRayDir, BbboxMin_childA.xyz, BbboxMax_childB.xyz);

            bool isNearestA = disA <= disB;
            float disNear = isNearestA ? disA : disB;
            float disFar = isNearestA ? disB : disA;
            int childIndexNear = isNearestA ? childIndexA : childIndexB;
            int childIndexFar = isNearestA ? childIndexB : childIndexA;

            if (disFar < best_t) stack[stackPtr++] = childIndexFar;
            if (disNear < best_t) stack[stackPtr++] = childIndexNear;

            // Prevent overflow (optional: clamp or discard)
            if (stackPtr > MAX_STACK_SIZE) break;
        }
    }
}

vec3 trace(vec3 pos, vec3 dir, inout uint state){
    vec3 invDir = 1/dir;
    vec3 color = vec3(1, 1, 1);

    for (int i = 0; i < bounceLim; i++) {

        float best_t = 1000000000;
        int triTest, aabbTest;
        int best_tri_i = -1;
        float best_u, best_v, best_w;
        for (int i = 0; i < numModels; i++){
            traverseBVH(models[i], pos, dir, invDir, best_t, best_u, best_v, triTest, aabbTest, best_tri_i);
        }

        if (false){
            int triThreshold = 50;
            int aabbThreshold = 500;
            color = vec3(float(triTest)/triThreshold, 0, float(aabbTest)/aabbThreshold);
            if (triTest > triThreshold || aabbTest > aabbThreshold){
                color = vec3(1);
                return color;
            }
            return color;
        }
        best_w = 1-best_u-best_v;

        if (best_tri_i != -1) {
            pos += dir * best_t;
            ivec4 tri = triangles[best_tri_i];
            int material_i = tri.w;
            vec3 v1 = vertices[tri.x].xyz;
            vec3 v2 = vertices[tri.y].xyz;
            vec3 v3 = vertices[tri.z].xyz;
            vec3 normal = normalize(cross(v2 - v1, v3 - v1));

            color *= colors[material_i].xyz;
            if (emission[material_i] > 0.0) {
                color *= emission[material_i];
                break;
            }

            vec3 random = randPointSphere(state)+normal;
            random = normalize(random);
            vec3 reflect = dir-normal*2*dot(dir, normal);
            dir = normalize(mix(random, reflect, colors[material_i].w));
            invDir = 1/dir;
        }
        else if (dir.y < 0){
            float t = ((-1000)-pos.y)/dir.y;
            if (t > 0.01 && t < 10000000){
                pos += dir*t;

                vec3 normal = vec3(0, 1, 0);

                color *= vec3(0.9,0.9,0.9);

                vec3 random = normalize(randPointSphere(state)+normal);
                vec3 reflect = dir-normal*2*dot(dir, normal);
                dir = normalize(mix(random, reflect, 0));
                invDir = 1/dir;
            } else {
                vec3 sky = vec3(0.5,0.7,0.9);
                vec3 sunDir = vec3(-0.5, 1, 1);
                vec3 sunColor = vec3(100, 70, 30);
                float sunStrength = pow(max(dot(dir, sunDir)-0.5,0), 128);
                sky += sunColor*sunStrength;
                color *= sky;
                break;
            }
        }
        else {
            vec3 sky = vec3(0.5,0.7,0.9);
            vec3 sunDir = vec3(-0.5, 1, 1);
            vec3 sunColor = vec3(100, 70, 30);
            float sunStrength = pow(max(dot(dir, sunDir)-0.5,0), 128);
            sky += sunColor*sunStrength;
            color *= sky;
            break;
        }

        if (i == bounceLim-1){
            color = vec3(0, 0, 0);
        }
    }

    return color;
}

void main() {
    float aspectRatio = 16./10.;
    vec2 sceenCoord = vec2((2*fragCoord.x-1) * aspectRatio, 2*fragCoord.y-1);

    uvec2 pixel = uvec2(fragCoord.x * resolution.x, fragCoord.y * resolution.y * aspectRatio);
    int pixels = int(resolution.x*resolution.y);
    uint state = pixel.x + pixel.y * uint(resolution.x) + uint(time);

    vec3 totalColor = vec3(0,0,0);

    int aaCycle = 0;
    for (int s = 0; s < samples; s++) {
        float xi = float(aaCycle % aa);
        float yi = float(aaCycle) / float(aa);

        float ox = (xi + 0.5) / float(aa) - 0.5f; // Center of each subpixel grid cell
        float oy = (yi + 0.5) / float(aa) - 0.5f;

        ox /= resolution.x/2;
        oy /= resolution.y/2;

        vec2 coord = sceenCoord + vec2(ox, oy);

        vec3 pos = cameraPos;
        vec3 dir = camForward + camRight * coord.x + camUp * coord.y;
        dir = normalize(dir);

        totalColor += trace(pos, dir, state);
        aaCycle++;
        if (aaCycle >= aa*aa) aaCycle = 0;
    }

    totalColor /= samples;
    totalColor = sqrt(totalColor);

    // Read previous accumulated color
    vec3 prev = texture(uPrevFrame, fragCoord).rgb;

    // Exponential moving average accumulation
    float frame = float(frameCount);
    vec3 accum = mix(prev, totalColor, 1.0 / (frame + 1.0));

    FragColor = vec4(accum, 1.0);
}

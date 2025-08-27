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
layout(std430, binding = 3) buffer ssboSpecularColors {
    vec4 specularColors[];
};
layout(std430, binding = 4) buffer ssboGlassLightSettings {
    vec4 glassLightSettings[];
};
layout(std430, binding = 5) buffer ssboBoundingBoxMin {
    vec4 boundingBoxMin[];
};
layout(std430, binding = 6) buffer ssboBoundingBoxMax {
    vec4 boundingBoxMax[];
};
layout(std430, binding = 7) buffer ssboChildA {
    int vChildA[];
};
layout(std430, binding = 8) buffer ssboChildB {
    int vChildB[];
};
layout(std430, binding = 9) buffer ssboModels {
    int models[];
};
layout(std430, binding = 10) buffer ssboNormalsList {
    vec4 normalsList[];
};
layout(std430, binding = 11) buffer ssboNormals {
    ivec4 normals[];
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

uniform vec3 skyColor;
uniform vec3 sunDir;
uniform vec3 sunColor;

const int MAX_STACK_SIZE = 48;
int stack[MAX_STACK_SIZE];
float iorStack[16];
int iorSize = 1;

//random functions
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

float schlick(float cos_theta, float n1, float n2) {
    if (abs(n1 - n2) < 0.001) return 0.0f;

    float r0 = pow((n1 - n2) / (n1 + n2), 2.0f);
    return r0 + (1.0f - r0) * pow(1.0f - cos_theta, 5.0f);
}

//intersection functions
bool rayTriangleIntersect(vec3 rayOrig, vec3 rayDir, vec3 v0, vec3 v1, vec3 v2, out float t, out float u, out float v){
    const float EPSILON = 0.0000001;
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

    if (t < 0.05)
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
        vec3 bboxMin = boundingBoxMin[nodeIndex].xyz;
        vec3 bboxMax = boundingBoxMax[nodeIndex].xyz;
        int childA = vChildA[nodeIndex];
        int childB = vChildB[nodeIndex];

        if (childA <= 0) {
            // Intersect ray with all triangles in the leaf node
            int triStart = -childA;
            int numTris = -childB;
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

            vec4 AbboxMin_childA = boundingBoxMin[childA];
            vec4 AbboxMax_childB = boundingBoxMax[childA];
            vec4 BbboxMin_childA = boundingBoxMin[childB];
            vec4 BbboxMax_childB = boundingBoxMax[childB];

            aabbTest += 2;
            float disA = intersectAABB(rayPos, invRayDir, AbboxMin_childA.xyz, AbboxMax_childB.xyz);
            float disB = intersectAABB(rayPos, invRayDir, BbboxMin_childA.xyz, BbboxMax_childB.xyz);

            bool isNearestA = disA <= disB;
            float disNear = isNearestA ? disA : disB;
            float disFar = isNearestA ? disB : disA;
            int childIndexNear = isNearestA ? childA : childB;
            int childIndexFar = isNearestA ? childB : childA;

            if (disFar < best_t) stack[stackPtr++] = childIndexFar;
            if (disNear < best_t) stack[stackPtr++] = childIndexNear;

            // Prevent overflow (optional: clamp or discard)
            if (stackPtr > MAX_STACK_SIZE) break;
        }
    }
}
float findBestTri(vec3 pos, vec3 dir, vec3 invDir, out int best_tri_i, out int triTest, out int aabbTest, out float best_u, out float best_v, out float best_w){
    best_tri_i = -1;
    float best_t = 1000000000;
    for (int i = 0; i < numModels; i++){
        traverseBVH(models[i], pos, dir, invDir, best_t, best_u, best_v, triTest, aabbTest, best_tri_i);
    }
    best_w = 1-best_u-best_v;
    return best_t;
}

//color functions
vec3 debugView(int triTest, int aabbTest){
    int triThreshold = 50;
    int aabbThreshold = 500;
    vec3 color = vec3(float(triTest)/triThreshold, 0, float(aabbTest)/aabbThreshold);
    if (triTest > triThreshold || aabbTest > aabbThreshold){
        color = vec3(1);
    }
    return color;
}
vec3 getEnviormentLight(vec3 dir){
    float sunStrength = pow(max(dot(dir, sunDir),0), 1024);
    return skyColor + sunColor*sunStrength;
}
bool updateColor(inout vec3 color, int material_i, bool isSpecular){
    //TERRAIN COLORS
    //float v = dot(normal, vec3(0, 1, 0));
    //v = max(v, 0);
    //v = 6*v*v*v*v*v-15*v*v*v*v+10*v*v*v;
    //v = v*v*v;

    //vec4 rock =  vec4(0.8,  0.7,  0.3,  0.0);
    //vec4 grass = vec4(0.1,  0.3,  0.1,  0.1);
    //vec4 snow =  vec4(0.9, 0.9, 0.9, 0.4);
    //vec4 hColor = mix(grass, snow, smoothstep(1000.0, 1200.0, pos.y));
    //vec4 c = mix(rock, hColor, v);

    vec3 selectedColor = isSpecular ? specularColors[material_i].xyz : colors[material_i].xyz;

    color *= selectedColor;
    if (glassLightSettings[material_i].z > 0.0) {
        color *= glassLightSettings[material_i].z;
        return true;
    }
    return false;
}

//calculate functions
vec3 calculateNormal(int triIndex, float u, float v, float w) {
    ivec4 normal = normals[triIndex];

    if (
        normal.x != -1 &&
        normal.y != -1 &&
        normal.z != -1)
    {
        vec3 normA = normalsList[normal.x].xyz;
        vec3 normB = normalsList[normal.y].xyz;
        vec3 normC = normalsList[normal.z].xyz;

        vec3 interpolatedNormal = normA * w + normB * u + normC * v;

        // Normalize the result
        return normalize(interpolatedNormal);
    }

    // Fallback to face normal if vertex normals are not available
    ivec4 tri = triangles[triIndex];
    vec3 v1 = vertices[tri.x].xyz;
    vec3 v2 = vertices[tri.y].xyz;
    vec3 v3 = vertices[tri.z].xyz;
    return normalize(cross(v2 - v1, v3 - v1));
}
vec3 calculateInitialDir(int aaCycle, vec2 screenCoord){
    float xi = float(aaCycle % aa);
    float yi = float(aaCycle) / float(aa);

    float ox = (xi + 0.5) / float(aa) - 0.5f; // Center of each subpixel grid cell
    float oy = (yi + 0.5) / float(aa) - 0.5f;

    ox /= resolution.x/2;
    oy /= resolution.y/2;

    vec2 coord = screenCoord + vec2(ox, oy);

    vec3 dir = camForward + camRight * coord.x + camUp * coord.y;
    dir = normalize(dir);

    return dir;
}

vec3 calculateRandDir(vec3 normal, inout uint state){
    return normalize(randPointSphere(state)+normal);
}
vec3 calculateReflectDir(vec3 normal, vec3 dir){
    return dir-normal*2*dot(dir, normal);
}

vec3 calculateOpaqueDir(vec3 normal, vec3 dir, float smoothness, inout uint state) {
    vec3 random = calculateRandDir(normal, state);
    vec3 reflect = calculateReflectDir(normal, dir);
    return normalize(mix(random, reflect, smoothness));
}
vec3 calculateRefractionDir(vec3 normal, vec3 dir, float smoothness, float ior, float specularProb, inout uint state, inout vec3 color){
    bool entering;
    float m1, m2;
    vec3 n = normal;

    // Determine if we're entering or exiting the material
    if (dot(dir, normal) > 0.0f) {
        // Ray and normal point in same direction = EXITING
        entering = false;
        m1 = iorStack[iorSize-1];    // Current medium (where ray is coming from)
        m2 = (iorSize >= 2) ? iorStack[iorSize-2] : 1.0;  // Previous medium (where ray is going)
        n = -normal; // Flip normal for refraction math
    } else {
        // Ray and normal point opposite directions = ENTERING
        entering = true;
        m1 = (iorSize >= 1) ? iorStack[iorSize-1] : 1.0;  // Current medium (air/previous)
        m2 = ior;                    // New medium we're entering
        n = normal;  // Keep normal as-is
    }

    float eta = m1 / m2;
    float cos_theta_i = clamp(-dot(dir, n), 0.0f, 1.0f);

    // Fresnel reflectance using Schlick's approximation
    float r0 = (m1 - m2) / (m1 + m2);
    r0 *= r0;
    float reflect_prob = r0 + (1.0 - r0) * pow(1.0 - cos_theta_i, 5.0);

    // Randomly choose reflection vs refraction based on Fresnel
    if (randomValue(state) < reflect_prob) {
        // Use original normal for reflection, not the potentially flipped one
        return calculateOpaqueDir(normal, dir, smoothness, state);
    }

    // Calculate discriminant for Total Internal Reflection check
    float discriminant = 1.0f - eta * eta * (1.0f - cos_theta_i * cos_theta_i);



    // Check for Total Internal Reflection
    if (discriminant < 0.0f) {
        float angle_degrees = acos(cos_theta_i) * 180.0 / 3.14159;
        return calculateOpaqueDir(normal, dir, smoothness, state);
    }

    // Calculate refracted direction using Snell's law
    float cos_theta_t = sqrt(discriminant);
    vec3 refracted = eta * dir + (eta * cos_theta_i - cos_theta_t) * n;

    // Update IOR stack
    if (entering) {
        iorStack[iorSize++] = ior;
    } else {
        iorSize--;
    }

    // For roughness: mix with random direction
    // Use the normal on the refracted side for the random direction
    vec3 refract_normal = entering ? normal : -normal;
    vec3 random = calculateRandDir(refract_normal, state);
    refracted = mix(random, normalize(refracted), specularProb);

    return normalize(refracted);
}

vec3 calculateNewDirection(vec3 normal, vec3 dir, float smoothness, float specularProb, float transparency, float ior, inout uint state, inout vec3 color){
    if (randomValue(state) <= transparency){
        return calculateRefractionDir(normal, dir, smoothness, ior, specularProb, state, color);
    }
    return calculateOpaqueDir(normal, dir, smoothness, state);
}

//hit updaters
bool hitTriangleUpdate(int triIndex, float t, float u, float v, float w, inout vec3 pos, inout vec3 dir, inout vec3 invDir, inout vec3 color, inout uint state){
    pos += dir * t;
    int material_i = triangles[triIndex].w;

    //calculate normal
    vec3 normal = calculateNormal(triIndex, u ,v, w);

    float specularProb = specularColors[material_i].w;
    bool isSpecular = randomValue(state) <= specularProb;

    //update color
    if (updateColor(color, material_i, isSpecular)) return true;

    //update direction
    float smoothness = isSpecular ? colors[material_i].w : 0;
    float transparancy = glassLightSettings[material_i].x;
    float ior = glassLightSettings[material_i].y;
    dir = calculateNewDirection(normal, dir, smoothness, specularProb, transparancy, ior, state, color);
    invDir = 1/dir;

    return false;
}
bool hitFloorUpdate(inout vec3 pos, inout vec3 dir, inout vec3 invDir, inout vec3 color, inout uint state){
    float floorHeight = -1000;
    float t = ((floorHeight)-pos.y)/dir.y;
    if (t > 0.01 && t < 10000000){
        pos += dir*t;

        vec3 normal = vec3(0, 1, 0);

        color *= vec3(0.9,0.9,0.9);

        dir = calculateNewDirection(normal, dir, 0, 0, 0, 1, state, color);
        invDir = 1/dir;
    } else {
        color *= getEnviormentLight(dir);
        return true;
    }
    return false;
}
bool russianRoulet(inout vec3 color, inout uint state){
    float p = min(max(color.r, max(color.g, color.b))*5, 1);
    if (randomValue(state) >= p) {
        return true;
    }
    color *= 1.0f / p;
    return false;
}

vec3 trace(vec3 pos, vec3 dir, inout uint state){
    iorStack[0] = 1;
    iorSize = 1;
    vec3 invDir = 1/dir;
    vec3 color = vec3(1);

    for (int i = 0; i < bounceLim; i++) {

        //find closest triangle
        int triTest, aabbTest, best_tri_i;
        float best_u, best_v, best_w;
        float t = findBestTri(pos, dir, invDir, best_tri_i, triTest, aabbTest, best_u, best_v, best_w);

        //debug view
        if (false){
            return debugView(triTest, aabbTest);
        }

        if (best_tri_i != -1) {if (hitTriangleUpdate(best_tri_i, t, best_u, best_v, best_w, pos, dir, invDir, color, state)) break;} //hit tri
        else if (dir.y < 0) {if (hitFloorUpdate(pos, dir, invDir, color, state)) break;} //hit floor
        else {
            color *= getEnviormentLight(dir);
            break;
        } //hit sky

        //russian roulet (low light rays get deleted)
        if (russianRoulet(color, state)) return vec3(0);

        //max bounce lim clears color
        if (i == bounceLim-1){
            return vec3(0);
        }
    }

    return color;
}

void main() {
    //setup
    float aspectRatio = 16./9.;
    vec2 screenCoord = vec2((2*fragCoord.x-1) * aspectRatio, 2*fragCoord.y-1); // centered at 0,0

    uvec2 pixel = uvec2(fragCoord.x * resolution.x, fragCoord.y * resolution.y * aspectRatio);
    int pixels = int(resolution.x*resolution.y);
    uint state = pixel.x + pixel.y * uint(resolution.x) + uint((frameCount*int(resolution.x)*time)%4000000000);

    vec3 totalColor = vec3(0,0,0);

    //sample loop
    int aaCycle = frameCount%(aa*aa);
    for (int s = 0; s < samples; s++) {
        vec3 pos = cameraPos;
        vec3 dir = calculateInitialDir(aaCycle, screenCoord);

        totalColor += trace(pos, dir, state);
        aaCycle++;
        if (aaCycle >= aa*aa) aaCycle = 0;
    }
    //gamma correction
    totalColor /= samples;
    totalColor = sqrt(totalColor);

    //read previous accumulated color
    vec3 prev = texture(uPrevFrame, fragCoord).rgb;

    //exponential moving average accumulation
    vec3 accum = mix(prev, totalColor, 1.0 / (float(frameCount) + 1.0));

    //retrn result
    FragColor = vec4(accum, 1.0);
}

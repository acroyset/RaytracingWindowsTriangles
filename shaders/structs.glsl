
struct Material {
    vec4 diffuseColor; // diffuse color, smoothness
    vec4 specularColor; // specular color, specular probability
    vec4 glassLightSettings; // transparency, ior, emission strength, transparent smoothness
};

struct Triangle{
    vec3 v1, v2, v3;
    vec2 t1, t2, t3;
    vec3 n1, n2, n3;
    Material material;
    bool useTexture;
    int textureID;
    bool useNormals;
};

struct Hit{
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

struct BVHnode {
    float minX, minY, minZ;// xyz: bbox min
    int childA_TriStart; // childA, -TriStart
    float maxX, maxY, maxZ;// xyz: bbox max
    int childB_NumTri; // childB, -NumTri
};

vec3 diffuseColor(Material m){
    return m.diffuseColor.rgb;
}
float smoothness(Material m){
    return m.diffuseColor.w;
}
vec3 specularColor(Material m){
    return m.specularColor.rgb;
}
float specularProbability(Material m){
    return m.specularColor.w;
}
float transparency(Material m){
    return m.glassLightSettings.x;
}
float ior(Material m){
    return m.glassLightSettings.y;
}
float emissionStrength(Material m){
    return m.glassLightSettings.z;
}
float transparentSmoothness(Material m){
    return m.glassLightSettings.w;
}

vec3 getMin(BVHnode node){
    return vec3(node.minX, node.minY, node.minZ);
}
vec3 getMax(BVHnode node){
    return vec3(node.maxX, node.maxY, node.maxZ);
}
int childA(BVHnode node){
    int w = node.childA_TriStart;
    if (w <= 0) return -1;
    return w;
}
int childB(BVHnode node){
    int w = node.childB_NumTri;
    if (w <= 0) return -1;
    return w;
}
int triStart(BVHnode node){
    int w = node.childA_TriStart;
    if (w > 0) return -1;
    return -w;
}
int numTri(BVHnode node){
    int w = node.childB_NumTri;
    if (w > 0) return -1;
    return -w;
}
bool leaf(BVHnode node){
    return childA(node) == -1;
}
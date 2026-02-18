

struct Material {

    // Opaque

    vec4 diffuseColor;
    vec4 specularColor;

    float diffuseRoughness;
    float specularRoughness;
    float specularProbability;

    // Transparent

    float transparency;
    float indexOfRefraction;
    float absorption;

    // absorb color = diffuse color
    // roughness = diffuse roughness

    // specular color = same
    // specular roughness = same
    // specular prob = same

    // Emissive

    float emissionStrength;

    int type;
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

struct ModelOffset {
    int BVHnodes;
    int material;
    int textureID;

    int padding;
};

struct ModelDistance {
    int index;
    float dist2;
};

struct BVHnode {
    float minX, minY, minZ;// xyz: bbox min
    int childA_TriStart; // childA, -TriStart
    float maxX, maxY, maxZ;// xyz: bbox max
    int childB_NumTri; // childB, -NumTri
};

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
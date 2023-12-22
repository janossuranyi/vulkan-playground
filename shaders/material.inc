#ifndef MATERIAL_INC_
#define MATERIAL_INC_

#define MATERIAL_TYPE_DIFFUSE 0
#define MATERIAL_TYPE_PHONG 1
#define MATERIAL_TYPE_METALLIC_ROUGHNESS 2

#define ALPHA_MODE_OPAQUE 0
#define ALPHA_MODE_BLEND 1
#define ALPHA_MODE_MASK 2

int unpackAlphaMode(int flags) {
    return (flags >> 1) & 3;
}

bool doubleSided(int flags) {
    return bool(flags & 1);
}

struct S_MATERIAL {
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    vec4 normalScale;
    float occlusionStrenght;
    float metallicFactor;
    float roughnessFactor;
    int type;
    int flags;
    int baseColorTexture;
    int normalTexture;
    int specularTexture;
    int occlusionTexture;
    int emissiveTexture;
    int dummy0;
    int dummy1;
};

#endif
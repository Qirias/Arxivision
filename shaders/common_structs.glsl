// Lighting
struct ClusterBounds {
    vec4 minPoint;
    vec4 maxPoint;
};

struct ClusterLights {
    uint count;
    uint lightIndices[711];
};

struct PointLight {
    vec3 position;
    uint visibilityMask;
    vec4 color;
};

struct EditorData
{
    // Camera parameters
    uint frustumCulling;
    uint occlusionCulling;
    uint enableCulling;
    uint padding0;
    float zNear;
    float zFar;
    float speed;
    float padding1;
    vec3 position;
    float padding2;

    // Lighting parameters
    uint ssaoEnabled;
    uint ssaoOnly;
    uint ssaoBlur;
    uint deferred;
    float directLightColor;
    float directLightIntensity;
    float maxDistance;
    float padding3;
};


// Frustum / Occlusion Culling
struct IndirectDrawCommand
{
    uint    indexCount;
    uint    instanceCount;
    uint    firstIndex;
    int     vertexOffset;
    uint    firstInstance;
    uint    drawID;
    uint    _padding[2];
};

struct ObjectData {
    vec4 aabbMin;
    vec4 aabbMax; // w component stores the instanceCount
};

const float DIFFUSE_MULTIPLIER = 2.5f;
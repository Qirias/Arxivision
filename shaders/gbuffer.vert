#version 460
#extension GL_ARB_shader_draw_parameters : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec2 inUV;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec3 outPosView;
layout (location = 2) out vec3 outNormalView;
layout (location = 3) out vec2 outUV;

struct InstanceData {
    vec4 translation;
    vec4 color;
    uint visibilityMask;
    uint _padding[3];
};

layout (set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 invView;
} ubo;

layout (set = 0, binding = 1) readonly buffer InstanceDataBuffer {
    InstanceData instances[];
};

//layout(set = 0, binding = 2) readonly buffer FaceVisibilityBuffer {
//    uint faceVisibility[];
//};

// Rotation matrix for the 90-degree flip about the X-axis for the .vox models
const mat4 rotationX90 = mat4(
    1.0, 0.0, 0.0, 0.0,
    0.0, 0.0, -1.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 1.0
);

layout (push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

int getFaceIndex(vec3 normal) {
    vec3 absNormal = abs(normal);
    float maxComponent = max(max(absNormal.x, absNormal.y), absNormal.z);
    
    return maxComponent == absNormal.x ? (normal.x > 0 ? 1 : 0) : // Right, Left
           maxComponent == absNormal.y ? (normal.y > 0 ? 2 : 3) : // Bottom, Top
                                         (normal.z > 0 ? 4 : 5); // Front, Back
}

void main() {
    // gl_InstanceIndex bounds are [firstInstance, firstInstance + baseInstance]
    // where these are set in the drawCommand
    InstanceData instance = instances[gl_InstanceIndex];
    
    // Left most bit enabled for .vox scenes
    mat4 rotation = (instance.visibilityMask & 0x80000000) != 0 ? rotationX90 : mat4(1.0);
    
    // Transform the normal to world space
    mat3 normalWorldMatrix = transpose(inverse(mat3(push.modelMatrix * rotation)));
    vec3 worldNormal = normalize(normalWorldMatrix * inNormal);

    // Determine the face index based on the world normal
    int faceIndex = getFaceIndex(worldNormal);

    // Check visibility
    if ((instance.visibilityMask & (1u << faceIndex)) == 0) {
        // Face is occluded, move the vertex outside of clip space
        gl_Position = vec4(1, 1, 1, 0);
        return;
    }

    vec4 positionWorld = push.modelMatrix * vec4(inPos, 1.0);
    positionWorld.xyz += instance.translation.xyz;
    gl_Position = ubo.projection * ubo.view * positionWorld;

    mat3 normalViewMatrix = transpose(inverse(mat3(ubo.view * push.modelMatrix)));
    
    outNormalView = normalViewMatrix * inNormal;
    outPosView = vec3(ubo.view * positionWorld);
    outColor = instance.color.xyz;
    outUV = inUV;
}

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
};

layout (set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 invView;
} ubo;

layout (set = 0, binding = 1) readonly buffer InstanceDataBuffer {
    InstanceData instances[];
};

layout (push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

void main() {
    // gl_InstanceIndex bounds are [firstInstance, firstInstance + baseInstance]
    // where these are set in the drawCommand
    InstanceData instance = instances[gl_InstanceIndex];
    vec4 positionWorld = push.modelMatrix * vec4(inPos, 1.0);
    positionWorld.xyz += instance.translation.xyz;
    gl_Position = ubo.projection * ubo.view * positionWorld;

    mat3 normalViewMatrix = transpose(inverse(mat3(ubo.view * push.modelMatrix)));
    
    outNormalView = normalViewMatrix * inNormal;
    outPosView = vec3(ubo.view * positionWorld);
    outColor = instance.color.xyz;
    outUV = inUV;
}

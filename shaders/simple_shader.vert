#version 460
#extension GL_ARB_shader_draw_parameters : enable

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 uv;

layout (location = 0) out vec3 fragColor;
layout (location = 1) out vec3 fragPosWorld;
layout (location = 2) out vec3 fragNormalWorld;

struct InstanceData {
    vec4 translation;
    vec4 color;
};

layout (set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 invView;
    float nearPlane;
    float farPlane;
} ubo;

layout (set = 0, binding = 1) buffer InstanceDataBuffer {
    InstanceData instances[];
};

layout (push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

void main() {
    InstanceData instance = instances[gl_InstanceIndex + gl_BaseInstance];
    vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
    positionWorld.xyz += instance.translation.xyz;
    gl_Position = ubo.projection * ubo.view * positionWorld;

    fragNormalWorld = normalize(mat3(push.normalMatrix) * normal);
    fragPosWorld = positionWorld.xyz;
    fragColor = instance.color.xyz;
}

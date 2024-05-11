#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec2 inUV;

// Instance data
layout (location = 4) in vec3 inTranslation;
layout (location = 5) in vec3 inInstanceColor;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec3 outPosWorld;
layout (location = 2) out vec3 outNormalWorld;
layout (location = 3) out vec2 outUV;

layout (set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 invView;
} ubo;

layout (push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

void main() {
    vec4 positionWorld = push.modelMatrix * vec4(inPos, 1.0);
    positionWorld.xyz += inTranslation;
    gl_Position = ubo.projection * ubo.view * positionWorld;

    outNormalWorld = normalize(mat3(push.normalMatrix) * inNormal);
    outPosWorld = positionWorld.xyz;
    outColor = inInstanceColor * inColor;
    outUV = inUV;
}

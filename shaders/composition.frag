#version 450

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerSSAO;
layout (binding = 4) uniform sampler2D samplerSSAOBlur;
layout (binding = 5) uniform sampler2D samplerDeferred;
layout (binding = 6) uniform UBO
{
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    int ssao;
    int ssaoOnly;
    int ssaoBlur;
    int _padding;
} uboParams;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main()
{
    vec3 fragPos = texture(samplerPosition, inUV).rgb;
    vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);
    vec4 albedo = texture(samplerAlbedo, inUV);
    
    float ssao = (uboParams.ssaoBlur == 1) ? texture(samplerSSAOBlur, inUV).r : texture(samplerSSAO, inUV).r;
    vec3 deferredColor = texture(samplerDeferred, inUV).rgb;

    vec3 ambient = vec3(0.0) * albedo.rgb;

    if (uboParams.ssao == 1)
    {
        ambient *= ssao;
    }
    
    if (uboParams.ssaoOnly == 1)
    {
        outFragColor.rgb = ssao.rrr;
    }
    else
    {
        vec3 finalColor = ambient + deferredColor;
        finalColor = clamp(finalColor, 0.0, 1.0);
        outFragColor = vec4(finalColor, 1.0);
    }
}

#version 450

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerSSAO;
layout (binding = 4) uniform sampler2D samplerSSAOBlur;
layout (binding = 5) uniform UBO
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
//    vec3 fragWorldPos = (uboParams.inverseView * vec4(fragPos, 1.0)).xyz;
    vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);
    vec4 albedo = texture(samplerAlbedo, inUV);
    
    float ssao = (uboParams.ssaoBlur == 1) ? texture(samplerSSAOBlur, inUV).r : texture(samplerSSAO, inUV).r;

    vec3 lightPos = vec3(70, -178, 80);
//    vec3 L = normalize(lightPos - fragPos);
    vec3 L = normalize((uboParams.view * vec4(lightPos, 1.0)).xyz - fragPos);
    float NdotL = max(0.5, dot(normal, L));

    if (uboParams.ssaoOnly == 1)
    {
        outFragColor.rgb = ssao.rrr;
    }
    else
    {
        vec3 baseColor = albedo.rgb * NdotL;

        if (uboParams.ssao == 1)
        {
            outFragColor.rgb = ssao.rrr;

            if (uboParams.ssaoOnly != 1)
                outFragColor.rgb *= baseColor;
        }
        else
        {
//            outFragColor.rgb = vec3(texture(samplerPosition, inUV).w / 1024.0, 0, 0); // depth
//            outFragColor.rgb = (uboParams.view * vec4(fragWorldPos, 1.0)).xyz; // fragment position
//            outFragColor.rgb = normal * 0.5 + 0.5; // normal
            outFragColor.rgb = baseColor;
        }
    }
}

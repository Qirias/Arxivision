#version 450

layout (binding = 0) uniform sampler2D samplerPositionDepth;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D ssaoNoise;

layout (constant_id = 0) const int SSAO_KERNEL_SIZE = 32;
layout (constant_id = 1) const float SSAO_RADIUS = 0.2;

layout (binding = 3) uniform UBOSSAOKernel
{
    vec4 samples[SSAO_KERNEL_SIZE];
} uboSSAOKernel;

layout (binding = 4) uniform UBO
{
    mat4 projection;
    mat4 view;
    mat4 inverseView;
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out float outFragColor;

void main()
{
    // Get G-Buffer values
    vec3 fragPos = texture(samplerPositionDepth, inUV).rgb;
    vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);

    // Get a random vector using a noise lookup
    ivec2 texDim = textureSize(samplerPositionDepth, 0);
    ivec2 noiseDim = textureSize(ssaoNoise, 0);
    const vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/(noiseDim.y)) * inUV;
    vec3 randomVec = normalize(texture(ssaoNoise, noiseUV).xyz * 2.0 - 1.0);
    
    // Create TBN matrix
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(tangent, normal);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0f;
    // remove banding
    const float bias = 0.01f;
    for(int i = 0; i < SSAO_KERNEL_SIZE; i++)
    {
        vec3 samplePos = TBN * uboSSAOKernel.samples[i].xyz;
        samplePos = fragPos + samplePos * SSAO_RADIUS;
        
        // project
        vec4 offset = vec4(samplePos, 1.0f);
        offset = ubo.projection * offset;
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5f + 0.5f;
        
        float sampleDepth = texture(samplerPositionDepth, offset.xy).w;
        vec3 sampleNormal = normalize(texture(samplerNormal, offset.xy).rgb * 2.0 - 1.0);
        
        // If sampledNormal is very similar to the current fragment normal, skip the sample
        if(dot(sampleNormal, normal) > 0.99)
            continue;

        float rangeCheck = smoothstep(1.0f, 0.0f, abs(fragPos.z - sampleDepth) / SSAO_RADIUS);
        occlusion += (sampleDepth <= samplePos.z - bias ? 1.0f*rangeCheck : 0.0f);
    }
    
    occlusion = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
    occlusion = pow(occlusion, 1.2f);
    
    outFragColor = occlusion;
}

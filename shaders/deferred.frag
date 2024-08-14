#version 450

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;

layout (binding = 3) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 invView;
} ubo;

struct PointLight {
    vec3 position;
    uint chunkID;
    vec4 color;
};

layout (binding = 4) readonly buffer pointLightsBuffer {
    PointLight pointLights[];
};

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;


vec3 calculatePointLight(PointLight light, vec3 fragPos, vec3 normal, vec3 albedo) {
    vec3 lightPosViewSpace = vec3(ubo.view * vec4(light.position, 1.0));
    
    vec3 lightDir = normalize(lightPosViewSpace - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    
    float distance = length(lightPosViewSpace - fragPos);
    float attenuation = 1.0 / (distance * distance);
    
    vec3 diffuse = diff * light.color.rgb * light.color.a * albedo;
    return diffuse * attenuation;
}

void main() {
    vec3 fragPos = texture(samplerPosition, inUV).rgb;
    vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);
    vec3 albedo = texture(samplerAlbedo, inUV).rgb;

    vec3 finalColor = vec3(0.0);

    // Temporary length of point lights
    // Storage Buffer don't have length()
    for (uint i = 0; i < 262; ++i) {
        finalColor += calculatePointLight(pointLights[i], fragPos, normal, albedo);
    }

    // Output the final color
    outFragColor = vec4(finalColor, 1.0);
}

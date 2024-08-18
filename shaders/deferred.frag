#version 450

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerLTC1;
layout (binding = 4) uniform sampler2D samplerLTC2;

layout (binding = 5) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 invView;
} ubo;

struct PointLight {
    vec3 position;
    uint visibilityMask;
    vec4 color;
};

layout (binding = 6) readonly buffer pointLightsBuffer {
    PointLight pointLights[];
};

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

// Need 4 points for each face to create area lights
// Light's position is in the center of each voxel
const vec3 FACE_OFFSET[6][4] = vec3[6][4] (
    vec3[4](
        vec3(-0.5,  0.5, -0.5),
        vec3(-0.5, -0.5, -0.5),
        vec3(-0.5, -0.5,  0.5),
        vec3(-0.5,  0.5,  0.5)
    ),
    
    vec3[4](
        vec3(0.5,  0.5, -0.5),
        vec3(0.5, -0.5, -0.5),
        vec3(0.5, -0.5,  0.5),
        vec3(0.5,  0.5,  0.5)
    ),
    
    vec3[4](
        vec3(-0.5, 0.5, -0.5),
        vec3( 0.5, 0.5, -0.5),
        vec3( 0.5, 0.5,  0.5),
        vec3(-0.5, 0.5,  0.5)
    ),
    
    vec3[4](
        vec3(-0.5, -0.5, -0.5),
        vec3( 0.5, -0.5, -0.5),
        vec3( 0.5, -0.5,  0.5),
        vec3(-0.5, -0.5,  0.5)
    ),
    
    vec3[4](
        vec3(-0.5, -0.5, 0.5),
        vec3( 0.5, -0.5, 0.5),
        vec3( 0.5,  0.5, 0.5),
        vec3(-0.5,  0.5, 0.5)
    ),
    
    vec3[4](
        vec3(-0.5, -0.5, -0.5),
        vec3( 0.5, -0.5, -0.5),
        vec3( 0.5,  0.5, -0.5),
        vec3(-0.5,  0.5, -0.5)
    )
);

// FaceIndex0 Left (negative X)
// FaceIndex1 Right (positive X)
// FaceIndex2 Bottom (positive Y)
// FaceIndex3 Top (negative Y)
// FaceIndex4 Back (positive Z)
// FaceIndex5 Front (negative Z)

vec3 calculatePointLight(PointLight light, vec3 fragPos, vec3 normal, vec3 albedo, vec3 offset) {
    vec3 lightPosViewSpace = vec3(ubo.view * vec4(light.position + offset, 1.0));
    
    vec3 lightDir = normalize(lightPosViewSpace - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    
    float distance = length(lightPosViewSpace - fragPos);
    float attenuation = 1.0 / (distance * distance * distance);
    vec3 diffuse = diff * light.color.rgb * light.color.a * albedo;
    return diffuse * attenuation;
}

void main() {
    vec3 fragPos = texture(samplerPosition, inUV).rgb;
    vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);
    vec3 albedo = texture(samplerAlbedo, inUV).rgb;

    vec3 finalColor = vec3(0.0);
    vec3 offset = vec3(0);

    // Temporary length of point lights
    // Storage Buffer don't have length()
    for (uint i = 0; i < 262; i++) {
        if (pointLights[i].visibilityMask == 0) continue;
         for (int faceIndex = 0; faceIndex < 1; ++faceIndex) {
             if ((pointLights[i].visibilityMask & (1u << faceIndex)) != 0) {
                 for (int j = 0; j < 4; j++) {
                     finalColor += calculatePointLight(pointLights[i], fragPos, normal, albedo, FACE_OFFSET[faceIndex][j]);
                 }
             }
         }
     }

    // Output the final color
    outFragColor = vec4(finalColor, 1.0);
}

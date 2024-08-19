#version 450

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerLTC1; // for inverse M
layout (binding = 4) uniform sampler2D samplerLTC2; // GGX norm, fresnel, 0(unused), sphere

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
        vec3(-0.5001,  0.5, -0.5),
        vec3(-0.5001, -0.5, -0.5),
        vec3(-0.5001, -0.5,  0.5),
        vec3(-0.5001,  0.5,  0.5)
    ),
    
    vec3[4](
        vec3(0.5001,  0.5, -0.5),
        vec3(0.5001, -0.5, -0.5),
        vec3(0.5001, -0.5,  0.5),
        vec3(0.5001,  0.5,  0.5)
    ),
    
    vec3[4](
        vec3(-0.5, 0.5001, -0.5),
        vec3( 0.5, 0.5001, -0.5),
        vec3( 0.5, 0.5001,  0.5),
        vec3(-0.5, 0.5001,  0.5)
    ),
    
    vec3[4](
        vec3(-0.5, -0.5001, -0.5),
        vec3( 0.5, -0.5001, -0.5),
        vec3( 0.5, -0.5001,  0.5),
        vec3(-0.5, -0.5001,  0.5)
    ),
    
    vec3[4](
        vec3(-0.5, -0.5, 0.5001),
        vec3( 0.5, -0.5, 0.5001),
        vec3( 0.5,  0.5, 0.5001),
        vec3(-0.5,  0.5, 0.5001)
    ),
    
    vec3[4](
        vec3(-0.5, -0.5, -0.5001),
        vec3( 0.5, -0.5, -0.5001),
        vec3( 0.5,  0.5, -0.5001),
        vec3(-0.5,  0.5, -0.5001)
    )
);

// FaceIndex0 Left (negative X)
// FaceIndex1 Right (positive X)
// FaceIndex2 Bottom (positive Y)
// FaceIndex3 Top (negative Y)
// FaceIndex4 Back (positive Z)
// FaceIndex5 Front (negative Z)

const float LUT_SIZE = 64.0; // ltc_texture size
const float LUT_SCALE = (LUT_SIZE - 1.0)/LUT_SIZE;
const float LUT_BIAS = 0.5/LUT_SIZE;

vec3 IntegrateEdgeVec(vec3 v1, vec3 v2) {
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;

    return cross(v1, v2) * theta_sintheta;
}

vec3 LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided) {
    vec3 T1 = normalize(V - N * dot(V, N));
    vec3 T2 = cross(N, T1);

    Minv = Minv * transpose(mat3(T1, T2, N));

    vec3 L[4];
    L[0] = Minv * (points[0] - P);
    L[1] = Minv * (points[1] - P);
    L[2] = Minv * (points[2] - P);
    L[3] = Minv * (points[3] - P);

    vec3 dir = points[0] - P;
    vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0);

    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    vec3 vsum = vec3(0.0);
    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);

    float len = length(vsum);

    float z = vsum.z / len;
    if (behind) z = -z;

    vec2 uv = vec2(z * 0.5 + 0.5, len);
    uv = uv * LUT_SCALE + LUT_BIAS;

    float scale = texture(samplerLTC2, uv).w;

    float sum = len * scale;
    if (!behind && !twoSided) sum = 0.0;

    vec3 Lo_i = vec3(sum, sum, sum);
    return Lo_i;
}

vec3 calculateAreaLight(PointLight light, vec3 fragPos, vec3 normal, vec3 albedo, int faceIndex) {
    vec3 translatedPoints[4];
    for (int j = 0; j < 4; ++j) {
        translatedPoints[j] = vec3(ubo.view * vec4(light.position + FACE_OFFSET[faceIndex][j], 1.0));
    }

    vec3 viewPos = vec3(ubo.invView[3]); // Camera position in world space
    vec3 V = normalize(viewPos - fragPos); // View direction

    vec4 t1 = texture(samplerLTC1, inUV);
    mat3 Minv = mat3(
        vec3(t1.x, 0, t1.y),
        vec3(0, 1, 0),
        vec3(t1.z, 0, t1.w)
    );

    vec3 diffuse = LTC_Evaluate(normal, V, fragPos, mat3(1), translatedPoints, false);
    vec3 specular = LTC_Evaluate(normal, V, fragPos, Minv, translatedPoints, false);

    return light.color.rgb * light.color.a * (specular + albedo * diffuse);
}

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

    for (uint i = 0; i < 262; i++) {
        if (pointLights[i].visibilityMask == 0) continue;
        for (int faceIndex = 0; faceIndex < 1; ++faceIndex) {
            if ((pointLights[i].visibilityMask & (1u << faceIndex)) != 0) {
                finalColor += calculateAreaLight(pointLights[i], fragPos, normal, albedo, faceIndex);
//                 for (int j = 0; j < 4; j++)
//                     finalColor += calculatePointLight(pointLights[i], fragPos, normal, albedo, FACE_OFFSET[faceIndex][j]);
            }
        }
    }

    outFragColor = vec4(finalColor, 1.0);
}

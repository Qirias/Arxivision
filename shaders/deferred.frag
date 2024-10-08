#version 450
#extension GL_GOOGLE_include_directive : require
#include "common_structs.glsl"

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

layout (binding = 6) readonly buffer PointLightsBuffer {
    PointLight pointLights[];
};

layout (std430, binding = 7) restrict buffer ClusterLightsBuffer {
    ClusterLights clusterLights[];
};

layout (binding = 8) uniform LightCount {
    uint lightCount;
};

layout (binding = 9) uniform FrustumParams {
    mat4 notUsed;
    uvec3 gridSize;
    uint padding0;
    uvec2 screenDimensions;
    float zNear;
    float zFar;
};

layout (binding = 10) uniform EditorParams {
    EditorData editorData;
};

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

const float OFFSET = 0.5001f;
const float gamma = 2.2;

// Need 4 points for each face to create area lights
// Light's position is in the center of each voxel
// Winding order matters for the edge integration
const vec3 FACE_OFFSET[6][4] = vec3[6][4] (
    vec3[4](
        vec3(-OFFSET,  0.5, -0.5),
        vec3(-OFFSET, -0.5, -0.5),
        vec3(-OFFSET, -0.5,  0.5),
        vec3(-OFFSET,  0.5,  0.5)
    ),
    
    vec3[4](
        vec3(OFFSET,  0.5, -0.5),
        vec3(OFFSET,  0.5,  0.5),
        vec3(OFFSET, -0.5,  0.5),
        vec3(OFFSET, -0.5, -0.5)
    ),
    
    vec3[4](
        vec3(-0.5, OFFSET, -0.5),
        vec3(-0.5, OFFSET,  0.5),
        vec3( 0.5, OFFSET,  0.5),
        vec3( 0.5, OFFSET, -0.5)
    ),
    
    vec3[4](
        vec3(-0.5, -OFFSET, -0.5),
        vec3( 0.5, -OFFSET, -0.5),
        vec3( 0.5, -OFFSET,  0.5),
        vec3(-0.5, -OFFSET,  0.5)
    ),
    
    vec3[4](
        vec3(-0.5, -0.5, OFFSET),
        vec3( 0.5, -0.5, OFFSET),
        vec3( 0.5,  0.5, OFFSET),
        vec3(-0.5,  0.5, OFFSET)
    ),
    
    vec3[4](
        vec3(-0.5, -0.5, -OFFSET),
        vec3(-0.5,  0.5, -OFFSET),
        vec3( 0.5,  0.5, -OFFSET),
        vec3( 0.5, -0.5, -OFFSET)
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

const float EPSILON = 0.01;

vec3 PowVec3(vec3 v, float p) {
    return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}

vec3 ToLinear(vec3 v) {
    return PowVec3(v, gamma);
}
vec3 ToSRGB(vec3 v)   { return PowVec3(v, 1.0/gamma); }
vec3 IntegrateEdgeVec(vec3 v1, vec3 v2) {
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;

    return cross(v1, v2) * theta_sintheta;
}

vec3 LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], int faceIndex, bool twoSided) {
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
//    if (!behind && !twoSided) sum = 0.0; // Don't need that in deferred (?)

    vec3 Lo_i = vec3(sum, sum, sum);
    return Lo_i;
}

vec3 calculateAreaLight(PointLight light, vec3 fragPos, vec3 normal, vec3 albedo, int faceIndex) {
    vec3 lightPosViewSpace = (ubo.view * vec4(light.position, 1.0)).xyz;
    float distanceToLight = length(lightPosViewSpace - fragPos);

    // Early exit for both specular and diffuse if beyond MULTIPLIER * maxDistance
    if (distanceToLight > DIFFUSE_MULTIPLIER * editorData.maxDistance) {
        return vec3(0.0);
    }

    vec3 translatedPoints[4];
    for (int j = 0; j < 4; ++j) {
        translatedPoints[j] = vec3(ubo.view * vec4(light.position + FACE_OFFSET[faceIndex][j], 1.0));
    }

    vec3 V = normalize(-fragPos);
    float dotNV = clamp(dot(normal, V), 0.0f, 1.0f);

    vec2 uv = vec2(0.1, sqrt(1.0f - dotNV));
    uv = uv * LUT_SCALE + LUT_BIAS;
    
    vec3 mSpecular = ToLinear(vec3(0.23f, 0.23f, 0.23f));

    vec4 t1 = texture(samplerLTC1, uv);
    vec4 t2 = texture(samplerLTC2, uv);

    mat3 Minv = mat3(
        vec3(t1.x, 0, t1.y),
        vec3(0, 1, 0),
        vec3(t1.z, 0, t1.w)
    );

    vec3 diffuse = LTC_Evaluate(normal, V, fragPos, mat3(1), translatedPoints, faceIndex, false);
    
    float diffuseAttenuation = 1.0 - smoothstep(0.0, DIFFUSE_MULTIPLIER * editorData.maxDistance, distanceToLight);
    vec3 diffuseContribution = albedo * (diffuse * diffuseAttenuation * diffuseAttenuation);

    // Early exit for specular if beyond maxDistance
    vec3 specularContribution = vec3(0.0);
    if (distanceToLight <= editorData.maxDistance) {
        vec3 specular = LTC_Evaluate(normal, V, fragPos, Minv, translatedPoints, faceIndex, false);
        // GGX BRDF shadowing and Fresnel
		// t2.x: shadowedF90 (F90 normally it should be 1.0)
		// t2.y: Smith function for Geometric Attenuation Term, it is dot(V or L, H).
        specular *= mSpecular*t2.x + (1.0f - mSpecular) * t2.y;
        specularContribution = specular;
    }

    return light.color.rgb * light.color.a * (specularContribution + diffuseContribution);
}

vec3 calculatePointLight(PointLight light, vec3 fragPos, vec3 normal, vec3 albedo) {
    vec3 lightPosViewSpace = vec3(ubo.view * vec4(light.position, 1.0));
    
    vec3 lightDir = normalize(lightPosViewSpace - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    
    float distance = length(lightPosViewSpace - fragPos);
    float attenuation = 1.0 / (distance * distance);
    
    vec3 diffuse = diff * light.color.rgb * light.color.a * albedo;
    return diffuse * attenuation;
}

vec3 getSliceColor(float zViewPosition) {
    uint sliceIndex = uint(floor((log(abs(zViewPosition) / zNear) * gridSize.z) / log(zFar / zNear)));

    float hue = float(sliceIndex) / float(gridSize.z);

    vec3 color = vec3(0.0);
    color.r = abs(sin(hue * 3.14));
    color.g = abs(sin((hue + 0.33) * 3.14));
    color.b = abs(sin((hue + 0.66) * 3.14));
    
    return color;
}

vec4 getNormal(int faceIndex) {
    if (faceIndex == 0)
        return vec4(-1, 0, 0, 0);
    else if (faceIndex == 1)
        return vec4(1, 0, 0, 0);
    else if (faceIndex == 2)
        return vec4(0, 1, 0, 0);
    else if (faceIndex == 3)
        return vec4(0, -1, 0, 0);
    else if (faceIndex == 4)
        return vec4(0, 0, 1, 0);
    else 
        return vec4(0, 0, -1, 0);
    
    return vec4(0, 0, 0, 0);
}

vec3 getOffset(int faceIndex) {
    if (faceIndex == 0)
        return vec3(OFFSET, 0, 0);
    else if (faceIndex == 1)
        return vec3(-OFFSET, 0, 0);
    else if (faceIndex == 2)
        return vec3(0, -OFFSET, 0);
    else if (faceIndex == 3)
        return vec3(0, OFFSET, 0);
    else if (faceIndex == 4)
        return vec3(0, 0, -OFFSET);
    else
        return vec3(0, 0, OFFSET);

    return vec3(0, 0, 0);
}

void main() {
    vec3 fragPos = texture(samplerPosition, inUV).rgb;
    vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);
    vec3 albedo = texture(samplerAlbedo, inUV).rgb;
    
    // Locating which cluster this fragment is part of
    uint zTile = uint((log(abs(fragPos.z) / zNear) * gridSize.z) / log(zFar / zNear));
    vec2 tileSize = screenDimensions / vec2(gridSize.x, gridSize.y);
    uvec3 tile = uvec3(gl_FragCoord.xy / tileSize, zTile);
    uint tileIndex = tile.x + (tile.y * gridSize.x) + (tile.z * gridSize.x * gridSize.y);

    uint clusterLightCount = clusterLights[tileIndex].count;
    
    vec3 finalColor = vec3(0.0);
    
//    vec3 clusterColor = getClusterColor(tileIndex);

    for (uint i = 0; i < clusterLightCount; i++) {
        
        uint lightIndex = clusterLights[tileIndex].lightIndices[i];
        PointLight light = pointLights[lightIndex];
        
        if (light.visibilityMask == 0) continue;

        vec3 lightPosViewSpace = (ubo.view * vec4(light.position, 1.0)).xyz;
        float distToLight = length(fragPos - lightPosViewSpace);
        if (distToLight > DIFFUSE_MULTIPLIER * editorData.maxDistance) continue;
        if (distToLight < EPSILON) {
            finalColor = light.color.rgb * light.color.a;
            break;
        }
            
//        finalColor += calculatePointLight(light, fragPos, normal, albedo);

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            vec3 lightFacePos = (ubo.view * vec4(light.position + getOffset(faceIndex), 1.0f)).xyz;
            vec3 lightFaceNormal = normalize((ubo.view * getNormal(faceIndex)).xyz);
            vec3 lightToFrag = normalize(fragPos - lightFacePos);
    
            float LdotF = dot(lightFaceNormal, lightToFrag);

            // Faces that are looking the same way as the light source and they are not the light source are skipped
            if (dot(normal, lightFaceNormal) > 0.99 && length(fragPos - lightPosViewSpace) > 0.9) continue;

            if ((light.visibilityMask & (1u << faceIndex)) != 0) {
                if (LdotF > 0.51f) // cos(89)
                    finalColor += calculateAreaLight(light, fragPos, normal, albedo, faceIndex);
            }
        }
    }

    outFragColor = vec4(ToSRGB(finalColor), 1.0);
}

#version 450

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerSSAO;
layout (binding = 4) uniform sampler2D samplerSSAOBlur;
layout (binding = 5) uniform sampler2D samplerDeferred;

layout (binding = 6) uniform UBO {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
} uboParams;

struct EditorData
{
    // Camera parameters
    uint frustumCulling;
    uint occlusionCulling;
    uint enableCulling;
    uint padding0;
    float zNear;
    float zFar;
    float speed;
    float padding1;

    // Lighting parameters
    uint ssaoEnabled;
    uint ssaoOnly;
    uint ssaoBlur;
    uint deferred;
    float directLightColor;
    float directLightIntensity;
    float maxDistance;
    float padding2;
};


layout (binding = 7) uniform EditorParams {
    EditorData editorData;
};

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outFragColor;

vec3 kelvinToRGB(float kelvin) {
    float temp = kelvin / 100.0;
    float r, g, b;

    if (temp <= 66.0) {
        r = 1.0;
        g = clamp(0.39008157876901960784 * log(temp) - 0.63184144378862745098, 0.0, 1.0);
        b = temp <= 19.0 ? 0.0 : clamp(0.54320678911019607843 * log(temp - 10.0) - 1.19625408914, 0.0, 1.0);
    } else {
        r = clamp(1.29293618606274509804 * pow(temp - 60.0, -0.1332047592), 0.0, 1.0);
        g = clamp(1.12989086089529411765 * pow(temp - 60.0, -0.0755148492), 0.0, 1.0);
        b = 1.0;
    }

    return vec3(r, g, b);
}

void main() {
    vec3 fragPos = texture(samplerPosition, inUV).rgb;
    vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);
    vec4 albedo = texture(samplerAlbedo, inUV);
    
    float ssao = (editorData.ssaoBlur == 1) ? texture(samplerSSAOBlur, inUV).r : texture(samplerSSAO, inUV).r;
    vec3 deferredColor = texture(samplerDeferred, inUV).rgb;

    vec3 sunDirectionWorld = normalize(vec3(1.0, 1.0, -1.0));
    vec3 sunColor = kelvinToRGB(editorData.directLightColor);
    float sunIntensity = editorData.directLightIntensity;

    vec3 sunDirectionView = normalize((uboParams.view * vec4(sunDirectionWorld, 0.0)).xyz);

    float diff = max(dot(normal, -sunDirectionView), 0.0);
    vec3 diffuse = diff * sunColor * sunIntensity * albedo.rgb;

    vec3 ambient = vec3(0.01) * albedo.rgb;
    vec3 finalColor;
    
    if (editorData.ssaoOnly == 1) {
        finalColor = vec3(ssao);
    } else {
        finalColor = ambient + diffuse;
        
        if (editorData.ssaoEnabled == 1) finalColor *= ssao.rrr;
        
        if (editorData.deferred == 1)
            finalColor += deferredColor;
        finalColor = clamp(finalColor, 0.0, 1.0);
    }
    
    outFragColor = vec4(finalColor, 1.0);
}

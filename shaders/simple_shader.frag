#version 450

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosWorld;
layout (location = 2) in vec3 fragNormalWorld;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 invView;
    float nearPlane;
    float farPlane;
} ubo;

layout (push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

void main() {
    // Light and material properties
    vec3 lightPos = vec3(35, -740, -220);
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 viewPos = vec3(ubo.invView[3]);

    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse lighting
    vec3 norm = normalize(fragNormalWorld);
    vec3 lightDir = normalize(lightPos - fragPosWorld);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular lighting
    float specularStrength = 0.5;
    float shininess = 4.0;
    vec3 viewDir = normalize(viewPos - fragPosWorld);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * fragColor;

    //    float linearDepth = (2.0 * ubo.nearPlane) / (ubo.farPlane + ubo.nearPlane - gl_FragCoord.z * (ubo.farPlane - ubo.nearPlane));
    //    outColor = vec4(vec3(linearDepth), 1.0);
    outColor = vec4(result, 1.0);
}


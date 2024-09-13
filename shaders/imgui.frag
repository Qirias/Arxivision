#version 450
layout (binding = 0) uniform sampler2D imguiTexture;


layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(imguiTexture, inUV);

    outColor = texColor;
}

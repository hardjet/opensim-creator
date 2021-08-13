#version 330 core

uniform bool uIsTextured = false;
uniform bool uIsShaded = true;
uniform sampler2D uSampler0;

in vec4 GouraudBrightness;
in vec4 Rgba0;
in float RimIntensity;
in vec2 TexCoord;

layout (location = 0) out vec4 Color0Out;
layout (location = 1) out float Color1Out;

void main() {
    // write shaded geometry color
    vec4 color = uIsTextured ? texture(uSampler0, TexCoord) : Rgba0;

    if (uIsShaded) {
        color *= GouraudBrightness;
    }

    Color0Out = color;
    Color1Out = RimIntensity;
}

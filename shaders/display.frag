#version 430 core
out vec4 FragColor;
in vec2 fragCoord;
uniform sampler2D screenTex;
uniform float uExposure;

void main() {
    FragColor = uExposure * texture(screenTex, fragCoord);
}

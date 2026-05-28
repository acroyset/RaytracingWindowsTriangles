out vec4 FragColor;
in vec2 fragCoord;
uniform sampler2D u_src;
uniform vec2      u_texelSize;

const float weights[5] = float[](0.227027, 0.194595, 0.121622, 0.054054, 0.016216);

void main() {
    vec3 r = texture(u_src, fragCoord).rgb * weights[0];
    for (int i = 1; i < 5; i++) {
        r += texture(u_src, fragCoord + vec2(0.0, u_texelSize.y * float(i))).rgb * weights[i];
        r += texture(u_src, fragCoord - vec2(0.0, u_texelSize.y * float(i))).rgb * weights[i];
    }
    FragColor = vec4(r, 1.0);
}
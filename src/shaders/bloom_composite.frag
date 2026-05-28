out vec4 FragColor;
in vec2 fragCoord;

uniform sampler2D u_hdr;
uniform sampler2D u_bloom;
uniform float     u_strength;

void main() {
    vec3 hdr   = texture(u_hdr,   fragCoord).rgb;
    vec3 bloom = texture(u_bloom, fragCoord).rgb;
    FragColor  = vec4(hdr + bloom * u_strength, 1.0);
}
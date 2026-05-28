out vec4 FragColor;
in vec2 fragCoord;

uniform sampler2D u_input;
uniform float u_threshold;
uniform float u_knee;

float luminance(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

void main() {
    vec3 color = texture(u_input, fragCoord).rgb;
    float lum = luminance(color);

    // Quadratic soft threshold (no hard cutoff)
    float knee  = u_threshold * u_knee;
    float rq    = clamp(lum - u_threshold + knee, 0.0, 2.0 * knee);
    rq          = (rq * rq) / (4.0 * knee + 0.00001);
    float weight = max(rq, lum - u_threshold) / max(lum, 0.00001);

    FragColor = vec4(color * weight, 1.0);
}
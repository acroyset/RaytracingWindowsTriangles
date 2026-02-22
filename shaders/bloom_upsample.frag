out vec4 FragColor;
in vec2 fragCoord;

uniform sampler2D u_smaller;   // the mip below (already upsampled)
uniform sampler2D u_current;   // this mip level's downsampled content
uniform vec2      u_texelSize; // 1.0 / u_smaller resolution
uniform float     u_weight;

vec3 upsample(sampler2D tex, vec2 uv, vec2 t) {
    // 9-tap tent filter
    vec3 a = texture(tex, uv + t * vec2(-1, -1)).rgb;
    vec3 b = texture(tex, uv + t * vec2( 0, -1)).rgb;
    vec3 c = texture(tex, uv + t * vec2( 1, -1)).rgb;
    vec3 d = texture(tex, uv + t * vec2(-1,  0)).rgb;
    vec3 e = texture(tex, uv + t * vec2( 0,  0)).rgb;
    vec3 f = texture(tex, uv + t * vec2( 1,  0)).rgb;
    vec3 g = texture(tex, uv + t * vec2(-1,  1)).rgb;
    vec3 h = texture(tex, uv + t * vec2( 0,  1)).rgb;
    vec3 i = texture(tex, uv + t * vec2( 1,  1)).rgb;

    // Tent weights: corners=1, edges=2, center=4, total=16
    vec3 result  = e * (4.0 / 16.0);
    result      += (b + d + f + h) * (2.0 / 16.0);
    result      += (a + c + g + i) * (1.0 / 16.0);
    return result;
}

void main() {
    vec3 up  = upsample(u_smaller, fragCoord, u_texelSize);
    vec3 cur = texture(u_current, fragCoord).rgb;
    // accumulate: each level adds its frequency band
    FragColor = vec4(up*u_weight + cur, 1.0);
}
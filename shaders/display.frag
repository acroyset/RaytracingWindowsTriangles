#version 430 core
out vec4 FragColor;
in vec2 fragCoord;
uniform sampler2D screenTex;

uniform uvec2 resolution;
uniform int frameCount;
uniform vec3  uHudColor = vec3(0.9); // text color
uniform int   uHudScale = 3;  // 1 = super small, 2 = a bit bigger
uniform int   uHudMargin = 2; // pixels from screen edges

const uint DIGITS_3x5[50] = uint[50](
// 0
7u, 5u, 5u, 5u, 7u,
// 1
2u, 6u, 2u, 2u, 7u,
// 2
7u, 1u, 7u, 4u, 7u,
// 3
7u, 1u, 7u, 1u, 7u,
// 4
5u, 5u, 7u, 1u, 1u,
// 5
7u, 4u, 7u, 1u, 7u,
// 6
7u, 4u, 7u, 5u, 7u,
// 7
7u, 1u, 1u, 1u, 1u,
// 8
7u, 5u, 7u, 5u, 7u,
// 9
7u, 5u, 7u, 1u, 7u
);

uint getDigit(uint n, int posFromRight) {
    for (int i = 0; i < posFromRight; ++i) n /= 10u;
    return n % 10u;
}

int countDigits(uint n) {
    if (n == 0u) return 1;
    int c = 0;
    while (n > 0u) { n /= 10u; ++c; }
    return c;
}

bool digitPixelOn(uint digit, int x, int yTopToBottom) {
    // yTopToBottom: 0 = top row, 4 = bottom row
    uint rowBits = DIGITS_3x5[int(digit) * 5 + yTopToBottom];
    // bit for column x: MSB is leftmost col (x=0 -> bit 2)
    return ((rowBits >> uint(2 - x)) & 1u) != 0u;
}

float hudMaskBR() {
    int scale  = max(uHudScale, 1);
    int digits = countDigits(frameCount);

    // 3x5 font in GRID units (not pixels)
    const int gw = 3, gh = 5, gs = 1;     // glyph width/height, spacing
    const int cellW = gw + gs;

    // total size in grid units, then scale to pixels
    int totalWg = digits * gw + (digits - 1) * gs;
    int totalHg = gh;
    int totalW  = totalWg * scale;
    int totalH  = totalHg * scale;

    int x0 = int(resolution.x) - uHudMargin - totalW; // left of HUD box
    int y0 = uHudMargin;                               // bottom of HUD box

    ivec2 p = ivec2(gl_FragCoord.xy);
    if (p.x < x0 || p.x >= x0 + totalW || p.y < y0 || p.y >= y0 + totalH) return 0.0;

    // pixel -> grid coords
    int gx = (p.x - x0) / scale;   // 0..totalWg-1
    int gy = (p.y - y0) / scale;   // 0..totalHg-1

    int cellIndex = gx / cellW;    // which digit (0..digits-1)
    int inCellX   = gx % cellW;    // 0..gw-1 (gw..cellW-1 = spacing)

    if (cellIndex < 0 || cellIndex >= digits) return 0.0;
    if (inCellX >= gw || gy < 0 || gy >= gh)  return 0.0;

    int yTopToBottom = (gh - 1) - gy;
    int posFromRight = digits - 1 - cellIndex;
    uint d = getDigit(frameCount, posFromRight);

    return digitPixelOn(d, inCellX, yTopToBottom) ? 1.0 : 0.0;
}

// Call this at the end to overlay text on your rendered color.
vec3 applyHud(vec3 baseColor) {
    float m = hudMaskBR();
    return mix(baseColor, uHudColor, m); // simple “over” with no alpha
}

void main() {
    vec3 color = texture(screenTex, fragCoord).rgb;
    color = applyHud(color);
    FragColor = vec4(color, 1);
}

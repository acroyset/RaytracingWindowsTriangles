
out vec4 FragColor;

in vec2 fragCoord; // [0,1]

uniform sampler2D u_input;
uniform uvec2 u_resolution;

vec3 tonemapACES(vec3 x){
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x*(a*x+b)) / (x*(c*x+d)+e), 0.0, 1.0);
}

void main(){

    vec4 t = texture(u_input, fragCoord);

    vec3 color = t.rgb;


    color = tonemapACES(color);

    color = pow(color, vec3(1/2.2));

    FragColor = vec4(color, 1);
}
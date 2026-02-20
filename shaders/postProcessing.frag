
out vec4 FragColor;

in vec2 fragCoord; // [0,1]

uniform sampler2D u_input;


void main(){

    vec3 color = texture(u_input, fragCoord).rgb;

    //color.r = pow(color.r, 1/2.2);
    //color.g = pow(color.g, 1/2.2);
    //color.b = pow(color.b, 1/2.2);

    FragColor = vec4(color, 1);
}
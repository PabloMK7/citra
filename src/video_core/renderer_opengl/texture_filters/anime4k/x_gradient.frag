//? #version 330
in vec2 tex_coord;

out vec2 frag_color;

uniform sampler2D tex_input;

const vec3 K = vec3(0.2627, 0.6780, 0.0593);
// TODO: improve handling of alpha channel
#define GetLum(xoffset) dot(K, textureOffset(tex_input, tex_coord, ivec2(xoffset, 0)).rgb)

void main() {
    float l = GetLum(-1);
    float c = GetLum(0);
    float r = GetLum(1);

    frag_color = vec2(r - l, l + 2.0 * c + r);
}

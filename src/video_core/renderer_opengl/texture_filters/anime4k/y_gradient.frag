//? #version 330
precision mediump float;

in vec2 tex_coord;

out float frag_color;

uniform sampler2D tex_input;

void main() {
    vec2 t = textureLodOffset(tex_input, tex_coord, 0.0, ivec2(0, 1)).xy;
    vec2 c = textureLod(tex_input, tex_coord, 0.0).xy;
    vec2 b = textureLodOffset(tex_input, tex_coord, 0.0, ivec2(0, -1)).xy;

    vec2 grad = vec2(t.x + 2.0 * c.x + b.x, b.y - t.y);

    frag_color = 1.0 - length(grad);
}

//? #version 330
precision mediump float;

in vec2 tex_coord;

out vec4 frag_color;

uniform sampler2D input_texture;

void main() {
    frag_color = texture(input_texture, tex_coord);
}

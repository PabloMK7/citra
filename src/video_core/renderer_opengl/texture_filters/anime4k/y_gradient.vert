//? #version 330
out vec2 input_max;

uniform sampler2D tex_size;

const vec2 vertices[4] =
    vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));

void main() {
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    input_max = textureSize(tex_size, 0) * 2 - 1;
}

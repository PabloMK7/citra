//? #version 330
out vec2 tex_coord;
out vec2 source_size;
out vec2 output_size;

uniform sampler2D tex;
uniform float scale;

const vec2 vertices[4] =
    vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));

void main() {
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    tex_coord = (vertices[gl_VertexID] + 1.0) / 2.0;
    source_size = textureSize(tex, 0);
    output_size = source_size * scale;
}

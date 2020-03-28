//? #version 330
in vec2 input_max;

out float frag_color;

uniform sampler2DRect tex_input;

void main() {
    vec2 t = texture(tex_input, min(gl_FragCoord.xy + vec2(0.0, 1.0), input_max)).xy;
    vec2 c = texture(tex_input, gl_FragCoord.xy).xy;
    vec2 b = texture(tex_input, max(gl_FragCoord.xy - vec2(0.0, 1.0), vec2(0.0))).xy;

    vec2 grad = vec2(t.x + 2 * c.x + b.x, b.y - t.y);

    frag_color = 1 - length(grad);
}

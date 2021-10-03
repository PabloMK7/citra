//? #version 320 es

out highp uint color;

uniform highp sampler2D depth;
uniform int lod;

void main() {
    color = uint(texelFetch(depth, ivec2(gl_FragCoord.xy), lod).x * (exp2(32.0) - 1.0));
}

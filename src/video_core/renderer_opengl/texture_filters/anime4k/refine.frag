//? #version 330
in vec2 tex_coord;
in vec2 input_max;

out vec4 frag_color;

uniform sampler2D HOOKED;
uniform sampler2DRect LUMAD;
uniform sampler2DRect LUMAG;

uniform float final_scale;

const float LINE_DETECT_THRESHOLD = 0.4;
const float STRENGTH = 0.6;

// the original shader used the alpha channel for luminance,
// which doesn't work for our use case
struct RGBAL {
    vec4 c;
    float l;
};

vec4 getAverage(vec4 cc, vec4 a, vec4 b, vec4 c) {
    return cc * (1 - STRENGTH) + ((a + b + c) / 3) * STRENGTH;
}

#define GetRGBAL(offset)                                                                           \
    RGBAL(textureOffset(HOOKED, tex_coord, offset),                                                \
          texture(LUMAD, clamp((gl_FragCoord.xy + offset) * final_scale, vec2(0.0), input_max)).x)

float min3v(float a, float b, float c) {
    return min(min(a, b), c);
}

float max3v(float a, float b, float c) {
    return max(max(a, b), c);
}

vec4 Compute() {
    RGBAL cc = GetRGBAL(ivec2(0));

    if (cc.l > LINE_DETECT_THRESHOLD) {
        return cc.c;
    }

    RGBAL tl = GetRGBAL(ivec2(-1, -1));
    RGBAL t = GetRGBAL(ivec2(0, -1));
    RGBAL tr = GetRGBAL(ivec2(1, -1));

    RGBAL l = GetRGBAL(ivec2(-1, 0));

    RGBAL r = GetRGBAL(ivec2(1, 0));

    RGBAL bl = GetRGBAL(ivec2(-1, 1));
    RGBAL b = GetRGBAL(ivec2(0, 1));
    RGBAL br = GetRGBAL(ivec2(1, 1));

    // Kernel 0 and 4
    float maxDark = max3v(br.l, b.l, bl.l);
    float minLight = min3v(tl.l, t.l, tr.l);

    if (minLight > cc.l && minLight > maxDark) {
        return getAverage(cc.c, tl.c, t.c, tr.c);
    } else {
        maxDark = max3v(tl.l, t.l, tr.l);
        minLight = min3v(br.l, b.l, bl.l);
        if (minLight > cc.l && minLight > maxDark) {
            return getAverage(cc.c, br.c, b.c, bl.c);
        }
    }

    // Kernel 1 and 5
    maxDark = max3v(cc.l, l.l, b.l);
    minLight = min3v(r.l, t.l, tr.l);

    if (minLight > maxDark) {
        return getAverage(cc.c, r.c, t.c, tr.c);
    } else {
        maxDark = max3v(cc.l, r.l, t.l);
        minLight = min3v(bl.l, l.l, b.l);
        if (minLight > maxDark) {
            return getAverage(cc.c, bl.c, l.c, b.c);
        }
    }

    // Kernel 2 and 6
    maxDark = max3v(l.l, tl.l, bl.l);
    minLight = min3v(r.l, br.l, tr.l);

    if (minLight > cc.l && minLight > maxDark) {
        return getAverage(cc.c, r.c, br.c, tr.c);
    } else {
        maxDark = max3v(r.l, br.l, tr.l);
        minLight = min3v(l.l, tl.l, bl.l);
        if (minLight > cc.l && minLight > maxDark) {
            return getAverage(cc.c, l.c, tl.c, bl.c);
        }
    }

    // Kernel 3 and 7
    maxDark = max3v(cc.l, l.l, t.l);
    minLight = min3v(r.l, br.l, b.l);

    if (minLight > maxDark) {
        return getAverage(cc.c, r.c, br.c, b.c);
    } else {
        maxDark = max3v(cc.l, r.l, b.l);
        minLight = min3v(t.l, l.l, tl.l);
        if (minLight > maxDark) {
            return getAverage(cc.c, t.c, l.c, tl.c);
        }
    }

    return cc.c;
}

void main() {
    frag_color = Compute();
}

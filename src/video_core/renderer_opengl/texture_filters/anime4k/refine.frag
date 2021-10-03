//? #version 330
precision mediump float;

in vec2 tex_coord;

out vec4 frag_color;

uniform sampler2D HOOKED;
uniform sampler2D LUMAD;

const float LINE_DETECT_THRESHOLD = 0.4;
const float STRENGTH = 0.6;

// the original shader used the alpha channel for luminance,
// which doesn't work for our use case
struct RGBAL {
    vec4 c;
    float l;
};

vec4 getAverage(vec4 cc, vec4 a, vec4 b, vec4 c) {
    return cc * (1.0 - STRENGTH) + ((a + b + c) / 3.0) * STRENGTH;
}

#define GetRGBAL(x_offset, y_offset)                                                               \
    RGBAL(textureLodOffset(HOOKED, tex_coord, 0.0, ivec2(x_offset, y_offset)),                     \
          textureLodOffset(LUMAD, tex_coord, 0.0, ivec2(x_offset, y_offset)).x)

float min3v(float a, float b, float c) {
    return min(min(a, b), c);
}

float max3v(float a, float b, float c) {
    return max(max(a, b), c);
}

vec4 Compute() {
    RGBAL cc = GetRGBAL(0, 0);

    if (cc.l > LINE_DETECT_THRESHOLD) {
        return cc.c;
    }

    RGBAL tl = GetRGBAL(-1, -1);
    RGBAL t = GetRGBAL(0, -1);
    RGBAL tr = GetRGBAL(1, -1);

    RGBAL l = GetRGBAL(-1, 0);

    RGBAL r = GetRGBAL(1, 0);

    RGBAL bl = GetRGBAL(-1, 1);
    RGBAL b = GetRGBAL(0, 1);
    RGBAL br = GetRGBAL(1, 1);

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

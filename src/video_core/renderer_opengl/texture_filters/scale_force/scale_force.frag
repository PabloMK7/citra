//? #version 320 es

// from https://github.com/BreadFish64/ScaleFish/tree/master/scale_force

// MIT License
//
// Copyright (c) 2020 BreadFish64
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

precision mediump float;

in vec2 tex_coord;

out vec4 frag_color;

uniform sampler2D input_texture;

vec2 tex_size;
vec2 inv_tex_size;

vec4 cubic(float v) {
    vec3 n = vec3(1.0, 2.0, 3.0) - v;
    vec3 s = n * n * n;
    float x = s.x;
    float y = s.y - 4.0 * s.x;
    float z = s.z - 4.0 * s.y + 6.0 * s.x;
    float w = 6.0 - x - y - z;
    return vec4(x, y, z, w) / 6.0;
}

// Bicubic interpolation
vec4 textureBicubic(vec2 tex_coords) {
    tex_coords = tex_coords * tex_size - 0.5;

    vec2 fxy = modf(tex_coords, tex_coords);

    vec4 xcubic = cubic(fxy.x);
    vec4 ycubic = cubic(fxy.y);

    vec4 c = tex_coords.xxyy + vec2(-0.5, +1.5).xyxy;

    vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
    vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;

    offset *= inv_tex_size.xxyy;

    vec4 sample0 = textureLod(input_texture, offset.xz, 0.0);
    vec4 sample1 = textureLod(input_texture, offset.yz, 0.0);
    vec4 sample2 = textureLod(input_texture, offset.xw, 0.0);
    vec4 sample3 = textureLod(input_texture, offset.yw, 0.0);

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}

mat4x3 center_matrix;
vec4 center_alpha;

// Finds the distance between four colors and cc in YCbCr space
vec4 ColorDist(vec4 A, vec4 B, vec4 C, vec4 D) {
    // https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.2020_conversion
    const vec3 K = vec3(0.2627, 0.6780, 0.0593);
    const float LUMINANCE_WEIGHT = .6;
    const mat3 YCBCR_MATRIX =
        mat3(K * LUMINANCE_WEIGHT, -.5 * K.r / (1.0 - K.b), -.5 * K.g / (1.0 - K.b), .5, .5,
             -.5 * K.g / (1.0 - K.r), -.5 * K.b / (1.0 - K.r));

    mat4x3 colors = mat4x3(A.rgb, B.rgb, C.rgb, D.rgb) - center_matrix;
    mat4x3 YCbCr = YCBCR_MATRIX * colors;
    vec4 color_dist = vec3(1.0) * YCbCr;
    color_dist *= color_dist;
    vec4 alpha = vec4(A.a, B.a, C.a, D.a);

    return sqrt((color_dist + abs(center_alpha - alpha)) * alpha * center_alpha);
}

void main() {
    vec4 bl = textureLodOffset(input_texture, tex_coord, 0.0, ivec2(-1, -1));
    vec4 bc = textureLodOffset(input_texture, tex_coord, 0.0, ivec2(0, -1));
    vec4 br = textureLodOffset(input_texture, tex_coord, 0.0, ivec2(1, -1));
    vec4 cl = textureLodOffset(input_texture, tex_coord, 0.0, ivec2(-1, 0));
    vec4 cc = textureLod(input_texture, tex_coord, 0.0);
    vec4 cr = textureLodOffset(input_texture, tex_coord, 0.0, ivec2(1, 0));
    vec4 tl = textureLodOffset(input_texture, tex_coord, 0.0, ivec2(-1, 1));
    vec4 tc = textureLodOffset(input_texture, tex_coord, 0.0, ivec2(0, 1));
    vec4 tr = textureLodOffset(input_texture, tex_coord, 0.0, ivec2(1, 1));


    tex_size = vec2(textureSize(input_texture, 0));
    inv_tex_size = 1.0 / tex_size;
    center_matrix = mat4x3(cc.rgb, cc.rgb, cc.rgb, cc.rgb);
    center_alpha = cc.aaaa;

    vec4 offset_tl = ColorDist(tl, tc, tr, cr);
    vec4 offset_br = ColorDist(br, bc, bl, cl);

    // Calculate how different cc is from the texels around it
    float total_dist = dot(offset_tl + offset_br, vec4(1.0));

    // Add together all the distances with direction taken into account
    vec4 tmp = offset_tl - offset_br;
    vec2 total_offset = tmp.wy + tmp.zz + vec2(-tmp.x, tmp.x);

    if (total_dist == 0.0) {
        // Doing bicubic filtering just past the edges where the offset is 0 causes black floaters
        // and it doesn't really matter which filter is used when the colors aren't changing.
        frag_color = cc;
    } else {
        // When the image has thin points, they tend to split apart.
        // This is because the texels all around are different
        // and total_offset reaches into clear areas.
        // This works pretty well to keep the offset in bounds for these cases.
        float clamp_val = length(total_offset) / total_dist;
        vec2 final_offset = clamp(total_offset, -clamp_val, clamp_val) * inv_tex_size;

        frag_color = textureBicubic(tex_coord - final_offset);
    }
}

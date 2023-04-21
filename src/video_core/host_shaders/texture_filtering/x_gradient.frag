// MIT License
//
// Copyright (c) 2019 bloc97
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

//? #version 430 core
precision mediump float;

layout(location = 0) in vec2 tex_coord;
layout(location = 0) out vec2 frag_color;

layout(binding = 0) uniform sampler2D tex_input;

const vec3 K = vec3(0.2627, 0.6780, 0.0593);
// TODO: improve handling of alpha channel
#define GetLum(xoffset) dot(K, textureLodOffset(tex_input, tex_coord, 0.0, ivec2(xoffset, 0)).rgb)

void main() {
    float l = GetLum(-1);
    float c = GetLum(0);
    float r = GetLum(1);

    frag_color = vec2(r - l, l + 2.0 * c + r);
}

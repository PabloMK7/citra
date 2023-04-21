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
layout(location = 0) out float frag_color;

layout(binding = 2) uniform sampler2D tex_input;

void main() {
    vec2 t = textureLodOffset(tex_input, tex_coord, 0.0, ivec2(0, 1)).xy;
    vec2 c = textureLod(tex_input, tex_coord, 0.0).xy;
    vec2 b = textureLodOffset(tex_input, tex_coord, 0.0, ivec2(0, -1)).xy;

    vec2 grad = vec2(t.x + 2.0 * c.x + b.x, b.y - t.y);

    frag_color = 1.0 - length(grad);
}

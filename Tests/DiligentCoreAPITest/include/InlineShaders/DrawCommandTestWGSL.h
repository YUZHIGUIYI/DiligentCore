/*
 *  Copyright 2019-2023 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include <string>

namespace
{

namespace WGSL
{

// clang-format off
const std::string DrawTest_ProceduralTriangleVS = R"(
struct VertexOutput
{
    @builtin(position) Position: vec4f;
    @location(0)       Color:    vec3f;
};

@vertex
fn main(@builtin(vertex_index) VertId: u32) -> VertexOutput
{
    let Position = array<vec4f>(
        vec4f(-1.0, -0.5, 0.0, 1.0),
        vec4f(-0.5, +0.5, 0.0, 1.0),
        vec4f( 0.0, -0.5, 0.0, 1.0),

        vec4f(+0.0, -0.5, 0.0, 1.0),
        vec4f(+0.5, +0.5, 0.0, 1.0),
        vec4f(+1.0, -0.5, 0.0, 1.0)
    );

    let Color = array<vec3f>(
        vec3f(1.0, 0.0, 0.0),
        vec3f(0.0, 1.0, 0.0),
        vec3f(0.0, 0.0, 1.0),

        vec3f(1.0, 0.0, 0.0),
        vec3f(0.0, 1.0, 0.0),
        vec3f(0.0, 0.0, 1.0)
    );

    var Output: VertexOutput;
    Output.Position = Position[VertId];
    Output.Color    = Color[VertId];
}
)";

const std::string DrawTest_PS =
R"(
struct PixelInput
{
    @builtin(position) Position: vec4f;
    @location(0)       Color   : vec3f;
};

fn main(Input: PixelInput) -> @location(0) vec4f
{
    return vec4f(Input.Color.rgb, 1.0);
}
)";

const std::string InputAttachmentTest_PS =
R"(
@group(0) @binding(0) var g_SubpassInput: texture_2d<f32>;

struct PixelInput
{
    @builtin(position) Position: vec4f;
    @location(0)       Color   : vec3f;
};

fn main(Input: PixelInput) -> @location(0) vec4f
{
    var Color: vec4f;
    Color.rgb = Input.Color.rgb * 0.125;
    Color.rgb += (float3(1.0, 1.0, 1.0) - textureLoad(g_SubpassInput, Input.Position.xy, 0)).brg) * 0.875;
    Color.a = 1.0;
    return Color;
}

)";
// clang-format on

} // namespace WGSL

} // namespace

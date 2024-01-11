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

#pragma once

#include "EngineWebGPUImplTraits.hpp"
#include "ShaderBase.hpp"
#include "SPIRVShaderResources.hpp"

namespace Diligent
{

class IDXCompiler;

/// Shader object implementation in WebGPU backend.
class ShaderWebGPUImpl final : public ShaderBase<EngineWebGPUImplTraits>
{
public:
    using TShaderBase = ShaderBase<EngineWebGPUImplTraits>;

    //TODO
    static constexpr INTERFACE_ID IID_InternalImpl =
        {0xa62b7e6a, 0x566b, 0x4c8d, {0xbd, 0xe0, 0x2f, 0x63, 0xcf, 0xca, 0x78, 0xc8}};

    struct CreateInfo
    {
        IDXCompiler* const         pDXCompiler;
        const RenderDeviceInfo&    DeviceInfo;
        const GraphicsAdapterInfo& AdapterInfo;
    };

    ShaderWebGPUImpl(IReferenceCounters*     pRefCounters,
                     RenderDeviceWebGPUImpl* pDeviceWebGPU,
                     const ShaderCreateInfo& ShaderCI,
                     const CreateInfo&       WebGPUShaderCI,
                     bool                    IsDeviceInternal = false);

    /// Implementation of IShader::GetResourceCount() in WebGPU backend.
    Uint32 DILIGENT_CALL_TYPE GetResourceCount() const override;

    /// Implementation of IShader::GetResourceDesc() in WebGPU backend.
    void DILIGENT_CALL_TYPE GetResourceDesc(Uint32 Index, ShaderResourceDesc& ResourceDesc) const override;

    /// Implementation of IShader::GetConstantBufferDesc() in WebGPU backend.
    const ShaderCodeBufferDesc* DILIGENT_CALL_TYPE GetConstantBufferDesc(Uint32 Index) const override;

    /// Implementation of IShader::GetBytecode() in WebGPU backend.
    void DILIGENT_CALL_TYPE GetBytecode(const void** ppBytecode, Uint64& Size) const override;

private:
    std::shared_ptr<const SPIRVShaderResources> m_pShaderResources;

    std::string           m_EntryPoint;
    std::vector<uint32_t> m_SPIRV;
};

} // namespace Diligent

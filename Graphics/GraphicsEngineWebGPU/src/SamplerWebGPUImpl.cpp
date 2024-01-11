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

#include "pch.h"

#include "SamplerWebGPUImpl.hpp"
#include "RenderDeviceWebGPUImpl.hpp"
#include "WebGPUTypeConversions.hpp"

namespace Diligent
{

namespace
{

WGPUSamplerDescriptor SamplerDescToWGPUSamplerDescriptor(const SamplerDesc& Desc) noexcept
{
    WGPUSamplerDescriptor wgpuSamplerDesc{};
    wgpuSamplerDesc.label         = Desc.Name;
    wgpuSamplerDesc.addressModeU  = TexAddressModeToWGPUAddressMode(Desc.AddressU);
    wgpuSamplerDesc.addressModeV  = TexAddressModeToWGPUAddressMode(Desc.AddressV);
    wgpuSamplerDesc.addressModeW  = TexAddressModeToWGPUAddressMode(Desc.AddressW);
    wgpuSamplerDesc.magFilter     = FilterTypeToWGPUFilterMode(Desc.MagFilter);
    wgpuSamplerDesc.minFilter     = FilterTypeToWGPUFilterMode(Desc.MinFilter);
    wgpuSamplerDesc.mipmapFilter  = FilterTypeToWGPUMipMapMode(Desc.MipFilter);
    wgpuSamplerDesc.lodMinClamp   = Desc.MinLOD;
    wgpuSamplerDesc.lodMaxClamp   = Desc.MaxLOD;
    wgpuSamplerDesc.compare       = ComparisonFuncToWGPUCompareFunction(Desc.ComparisonFunc);
    wgpuSamplerDesc.maxAnisotropy = static_cast<Uint16>(IsAnisotropicFilter(Desc.MinFilter) ? Desc.MaxAnisotropy : 1);
    return wgpuSamplerDesc;
}

} // namespace

SamplerWebGPUImpl::SamplerWebGPUImpl(IReferenceCounters*     pRefCounters,
                                     RenderDeviceWebGPUImpl* pDevice,
                                     const SamplerDesc&      Desc) :
    // clang-format off
    TSamplerBase
    {
        pRefCounters,
        pDevice,
        Desc
    }
// clang-format on
{
    WGPUSamplerDescriptor wgpuSamplerDesc = SamplerDescToWGPUSamplerDescriptor(m_Desc);
    m_wgpuSampler.Reset(wgpuDeviceCreateSampler(pDevice->GetWebGPUDevice(), &wgpuSamplerDesc));
    if (!m_wgpuSampler)
        LOG_ERROR_AND_THROW("Failed to create WebGPU sampler ", " '", m_Desc.Name ? m_Desc.Name : "", '\'');
}

} // namespace Diligent

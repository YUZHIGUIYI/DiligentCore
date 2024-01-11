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

/// \file
/// Declaration of Diligent::PipelineResourceSignatureWebGPUImpl class

#include "EngineWebGPUImplTraits.hpp"
#include "PipelineResourceAttribsWebGPU.hpp"
#include "PipelineResourceSignatureBase.hpp"
#include "ShaderResourceBindingWebGPUImpl.hpp"
#include "ShaderVariableManagerWebGPU.hpp"

namespace Diligent
{
/// Implementation of the Diligent::PipelineResourceSignatureWebGPUImpl class
class PipelineResourceSignatureWebGPUImpl final : public PipelineResourceSignatureBase<EngineWebGPUImplTraits>
{
public:
    using TPipelineResourceSignatureBase = PipelineResourceSignatureBase<EngineWebGPUImplTraits>;

    PipelineResourceSignatureWebGPUImpl(IReferenceCounters*                  pRefCounters,
                                        RenderDeviceWebGPUImpl*              pDevice,
                                        const PipelineResourceSignatureDesc& Desc,
                                        SHADER_TYPE                          ShaderStages      = SHADER_TYPE_UNKNOWN,
                                        bool                                 bIsDeviceInternal = false);
    /*
    PipelineResourceSignatureWebGPUImpl(IReferenceCounters*                               pRefCounters,
                                       RenderDeviceWebGPUImpl*                            pDevice,
                                       const PipelineResourceSignatureDesc&              Desc,
                                       const PipelineResourceSignatureInternalDataD3D11& InternalData);
    */

    // Copies static resources from the static resource cache to the destination cache
    void CopyStaticResources(ShaderResourceCacheWebGPU& ResourceCache) const;
    // Make the base class method visible
    using TPipelineResourceSignatureBase::CopyStaticResources;
};

} // namespace Diligent

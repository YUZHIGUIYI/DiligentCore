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
/// Declaration of Diligent::BufferWebGPUImpl class

#include "EngineWebGPUImplTraits.hpp"
#include "BufferBase.hpp"
#include "BufferViewWebGPUImpl.hpp" // Required by BufferBase
#include "WebGPUObjectWrappers.hpp"
#include "IndexWrapper.hpp"
#include "SharedMemoryManagerWebGPU.hpp"

namespace Diligent
{

/// Buffer implementation in WebGPU backend.
class BufferWebGPUImpl final : public BufferBase<EngineWebGPUImplTraits>
{
public:
    using TBufferBase = BufferBase<EngineWebGPUImplTraits>;

    BufferWebGPUImpl(IReferenceCounters*        pRefCounters,
                     FixedBlockMemoryAllocator& BuffViewObjMemAllocator,
                     RenderDeviceWebGPUImpl*    pDevice,
                     const BufferDesc&          Desc,
                     const BufferData*          pInitData = nullptr);

    // Attaches to an existing WebGPU resource
    BufferWebGPUImpl(IReferenceCounters*        pRefCounters,
                     FixedBlockMemoryAllocator& BuffViewObjMemAllocator,
                     RenderDeviceWebGPUImpl*    pDevice,
                     const BufferDesc&          Desc,
                     RESOURCE_STATE             InitialState,
                     WGPUBuffer                 wgpuBuffer);

    /// Implementation of IBuffer::QueryInterface().
    void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override;

    /// Implementation of IBuffer::GetNativeHandle().
    Uint64 DILIGENT_CALL_TYPE GetNativeHandle() override { return BitCast<Uint64>(m_wgpuBuffer.Get()); }

    /// Implementation of IBuffer::GetSparseProperties().
    SparseBufferProperties DILIGENT_CALL_TYPE GetSparseProperties() const override;

    /// Implementation of IBufferWebGPU::GetWebGPUBuffer().
    WGPUBuffer DILIGENT_CALL_TYPE GetWebGPUBuffer() const override { return m_wgpuBuffer.Get(); }

    void Map(MAP_TYPE MapType, Uint32 MapFlags, PVoid& pMappedData);

    void Unmap(MAP_TYPE MapType);

    Uint64 GetAlignment() const;

    const SharedMemoryManagerWebGPU::Allocation& GetDynamicAllocation(DeviceContextIndex CtxId) const;

    void SetDynamicAllocation(DeviceContextIndex CtxId, SharedMemoryManagerWebGPU::Allocation&& Allocation);

private:
    void CreateViewInternal(const BufferViewDesc& ViewDesc, IBufferView** ppView, bool IsDefaultView) override;

private:
    // Use 64-byte alignment to avoid cache issues
    static constexpr size_t CacheLineSize = 64;
    struct alignas(64) DynamicAllocation : SharedMemoryManagerWebGPU::Allocation
    {
        DynamicAllocation& operator=(const Allocation& Allocation)
        {
            *static_cast<SharedMemoryManagerWebGPU::Allocation*>(this) = Allocation;
            return *this;
        }
        Uint8 Padding[CacheLineSize - sizeof(Allocation)] = {};
    };
    static_assert(sizeof(DynamicAllocation) == CacheLineSize, "Unexpected sizeof(DynamicAllocation)");

    using DynamicAllocationList = std::vector<DynamicAllocation, STDAllocatorRawMem<DynamicAllocation>>;

    WebGPUBufferWrapper   m_wgpuBuffer;
    std::vector<uint8_t>  m_MappedData;
    DynamicAllocationList m_DynamicAllocations;
    Uint64                m_Alignment;
};

} // namespace Diligent

/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "GraphicsAccessories.hpp"
#include "../../../Graphics/GraphicsEngineD3DBase/interface/ShaderD3D.h"

#include "TestingEnvironment.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

#ifdef DILIGENT_DEVELOPMENT

RefCntAutoPtr<IShader> CreateShader(const char* Name, const char* Source, SHADER_TYPE ShaderType)
{
    ShaderCreateInfo ShaderCI;
    ShaderCI.EntryPoint                 = "main";
    ShaderCI.UseCombinedTextureSamplers = true;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Source                     = Source;

    ShaderCI.Desc.Name       = Name;
    ShaderCI.Desc.ShaderType = ShaderType;

    RefCntAutoPtr<IShader> pShader;
    TestingEnvironment::GetInstance()->GetDevice()->CreateShader(ShaderCI, &pShader);
    return pShader;
}


void TestNullResourceBinding(IShader* pVS, IShader* pPS, SHADER_RESOURCE_VARIABLE_TYPE VarType)
{
    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto* const pEnv       = TestingEnvironment::GetInstance();
    auto* const pDevice    = pEnv->GetDevice();
    auto* const pContext   = pEnv->GetDeviceContext();
    auto* const pSwapChain = pEnv->GetSwapChain();

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    auto& PSODesc          = PSOCreateInfo.PSODesc;
    auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    PSODesc.Name                                  = "Null resource test PSO";
    GraphicsPipeline.NumRenderTargets             = 1;
    GraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_RGBA8_UNORM;
    GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSODesc.ResourceLayout.DefaultVariableType = VarType;

    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pPSO->CreateShaderResourceBinding(&pSRB, false);

    ITextureView* ppRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);
    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->Draw(DrawAttribs{3, DRAW_FLAG_VERIFY_ALL});
}


class NullConstantBuffer : public testing::TestWithParam<SHADER_RESOURCE_VARIABLE_TYPE>
{
protected:
    static void SetUpTestSuite()
    {
        constexpr char NullConstantBufferVS[] = R"(
cbuffer MissingVSBuffer
{
    float4 g_f4Position;
}
float4 main() : SV_Position
{
    return g_f4Position;
}
)";

        constexpr char NullConstantBufferPS[] = R"(
cbuffer MissingPSBuffer
{
    float4 g_f4Color;
}
float4 main() : SV_Target
{
    return g_f4Color;
}
)";

        pVS = CreateShader("Null CB binding VS", NullConstantBufferVS, SHADER_TYPE_VERTEX);
        pPS = CreateShader("Null CB binding PS", NullConstantBufferPS, SHADER_TYPE_PIXEL);
    }

    static void TearDownTestSuite()
    {
        pVS.Release();
        pPS.Release();
    }
    static RefCntAutoPtr<IShader> pVS;
    static RefCntAutoPtr<IShader> pPS;
};
RefCntAutoPtr<IShader> NullConstantBuffer::pVS;
RefCntAutoPtr<IShader> NullConstantBuffer::pPS;

TEST_P(NullConstantBuffer, Test)
{
    auto* const pEnv       = TestingEnvironment::GetInstance();
    auto* const pDevice    = pEnv->GetDevice();
    const auto& DeviceInfo = pDevice->GetDeviceInfo();

    if (DeviceInfo.IsVulkanDevice())
        GTEST_SKIP() << "Null resources result in device removal in Vulkan";
    else if (DeviceInfo.IsMetalDevice())
        GTEST_SKIP() << "Null resources result in debug break in Metal";
    else if (DeviceInfo.Type == RENDER_DEVICE_TYPE_D3D12 && pDevice->GetAdapterInfo().Type != ADAPTER_TYPE_SOFTWARE)
        GTEST_SKIP() << "Null resources result in device removal in HW D3D12";

    pEnv->SetErrorAllowance(2, "No worries, errors are expected: testing null resource bindings\n");
    pEnv->PushExpectedErrorSubstring("No resource is bound to variable 'MissingPSBuffer'");
    pEnv->PushExpectedErrorSubstring("No resource is bound to variable 'MissingVSBuffer'", false);

    if (DeviceInfo.Type == RENDER_DEVICE_TYPE_D3D11)
    {
        pEnv->SetErrorAllowance(4);
        pEnv->PushExpectedErrorSubstring("Constant buffer at slot 0 is null", false);
        pEnv->PushExpectedErrorSubstring("Constant buffer at slot 0 is null", false);
    }

    TestNullResourceBinding(pVS, pPS, GetParam());
}

INSTANTIATE_TEST_SUITE_P(NullResourceBindings,
                         NullConstantBuffer,
                         testing::Values(SHADER_RESOURCE_VARIABLE_TYPE_STATIC, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC),
                         [](const testing::TestParamInfo<SHADER_RESOURCE_VARIABLE_TYPE>& info) //
                         {
                             return GetShaderVariableTypeLiteralName(info.param);
                         }); //




class NullStructBuffer : public testing::TestWithParam<SHADER_RESOURCE_VARIABLE_TYPE>
{
protected:
    static void SetUpTestSuite()
    {
        constexpr char NullStructBufferVS[] = R"(
struct BufferData
{
    float4 Data;
};
StructuredBuffer<BufferData> g_MissingVSStructBuffer;
float4 main() : SV_Position
{
    return g_MissingVSStructBuffer[0].Data;
}
)";

        constexpr char NullStructBufferPS[] = R"(
struct BufferData
{
    float4 Data;
};
StructuredBuffer<BufferData> g_MissingPSStructBuffer;
float4 main() : SV_Target
{
    return g_MissingPSStructBuffer[0].Data;
}
)";

        pVS = CreateShader("Null struct buffer binding VS", NullStructBufferVS, SHADER_TYPE_VERTEX);
        pPS = CreateShader("Null struct buffer binding PS", NullStructBufferPS, SHADER_TYPE_PIXEL);
    }

    static void TearDownTestSuite()
    {
        pVS.Release();
        pPS.Release();
    }
    static RefCntAutoPtr<IShader> pVS;
    static RefCntAutoPtr<IShader> pPS;
};
RefCntAutoPtr<IShader> NullStructBuffer::pVS;
RefCntAutoPtr<IShader> NullStructBuffer::pPS;

TEST_P(NullStructBuffer, Test)
{
    auto* const pEnv       = TestingEnvironment::GetInstance();
    auto* const pDevice    = pEnv->GetDevice();
    const auto& DeviceInfo = pDevice->GetDeviceInfo();

    if (DeviceInfo.IsVulkanDevice())
        GTEST_SKIP() << "Null resources result in device removal in Vulkan";
    else if (DeviceInfo.IsMetalDevice())
        GTEST_SKIP() << "Null resources result in debug break in Metal";
    else if (DeviceInfo.Type == RENDER_DEVICE_TYPE_D3D12 && pDevice->GetAdapterInfo().Type != ADAPTER_TYPE_SOFTWARE)
        GTEST_SKIP() << "Null structured buffer result in device removal in HW D3D12 and an exception in WARP";

    pEnv->SetErrorAllowance(2, "No worries, errors are expected: testing null resource bindings\n");
    pEnv->PushExpectedErrorSubstring("No resource is bound to variable 'g_MissingPSStructBuffer'");
    pEnv->PushExpectedErrorSubstring("No resource is bound to variable 'g_MissingVSStructBuffer'", false);

    if (DeviceInfo.Type == RENDER_DEVICE_TYPE_D3D11)
    {
        pEnv->SetErrorAllowance(4);
        pEnv->PushExpectedErrorSubstring("Shader resource view at slot 0 is null", false);
        pEnv->PushExpectedErrorSubstring("Shader resource view at slot 0 is null", false);
    }

    TestNullResourceBinding(pVS, pPS, GetParam());
}

INSTANTIATE_TEST_SUITE_P(NullResourceBindings,
                         NullStructBuffer,
                         testing::Values(SHADER_RESOURCE_VARIABLE_TYPE_STATIC, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC),
                         [](const testing::TestParamInfo<SHADER_RESOURCE_VARIABLE_TYPE>& info) //
                         {
                             return GetShaderVariableTypeLiteralName(info.param);
                         }); //



class NullFormattedBuffer : public testing::TestWithParam<SHADER_RESOURCE_VARIABLE_TYPE>
{
protected:
    static void SetUpTestSuite()
    {
        constexpr char NullFormattedBufferVS[] = R"(
Buffer<float4> g_MissingVSFmtBuffer;
float4 main() : SV_Position
{
    return g_MissingVSFmtBuffer.Load(0);
}
)";

        constexpr char NullFormattedBufferPS[] = R"(
Buffer<float4> g_MissingPSFmtBuffer;
float4 main() : SV_Target
{
    return g_MissingPSFmtBuffer.Load(0);
}
)";

        pVS = CreateShader("Null formatted buffer binding VS", NullFormattedBufferVS, SHADER_TYPE_VERTEX);
        pPS = CreateShader("Null formatted buffer binding PS", NullFormattedBufferPS, SHADER_TYPE_PIXEL);
    }

    static void TearDownTestSuite()
    {
        pVS.Release();
        pPS.Release();
    }
    static RefCntAutoPtr<IShader> pVS;
    static RefCntAutoPtr<IShader> pPS;
};
RefCntAutoPtr<IShader> NullFormattedBuffer::pVS;
RefCntAutoPtr<IShader> NullFormattedBuffer::pPS;

TEST_P(NullFormattedBuffer, Test)
{
    auto* const pEnv       = TestingEnvironment::GetInstance();
    auto* const pDevice    = pEnv->GetDevice();
    const auto& DeviceInfo = pDevice->GetDeviceInfo();

    if (DeviceInfo.IsVulkanDevice())
        GTEST_SKIP() << "Null resources result in device removal in Vulkan";
    else if (DeviceInfo.IsMetalDevice())
        GTEST_SKIP() << "Null resources result in debug break in Metal";
    else if (DeviceInfo.Type == RENDER_DEVICE_TYPE_D3D12)
        GTEST_SKIP() << "Null buffer results in device removal in HW D3D12 and an exception in WARP";

    pEnv->SetErrorAllowance(2, "No worries, errors are expected: testing null resource bindings\n");
    pEnv->PushExpectedErrorSubstring("No resource is bound to variable 'g_MissingPSFmtBuffer'");
    pEnv->PushExpectedErrorSubstring("No resource is bound to variable 'g_MissingVSFmtBuffer'", false);

    if (DeviceInfo.Type == RENDER_DEVICE_TYPE_D3D11)
    {
        pEnv->SetErrorAllowance(4);
        pEnv->PushExpectedErrorSubstring("Shader resource view at slot 0 is null", false);
        pEnv->PushExpectedErrorSubstring("Shader resource view at slot 0 is null", false);
    }

    TestNullResourceBinding(pVS, pPS, GetParam());
}

INSTANTIATE_TEST_SUITE_P(NullResourceBindings,
                         NullFormattedBuffer,
                         testing::Values(SHADER_RESOURCE_VARIABLE_TYPE_STATIC, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC),
                         [](const testing::TestParamInfo<SHADER_RESOURCE_VARIABLE_TYPE>& info) //
                         {
                             return GetShaderVariableTypeLiteralName(info.param);
                         }); //




class NullTexture : public testing::TestWithParam<SHADER_RESOURCE_VARIABLE_TYPE>
{
protected:
    static void SetUpTestSuite()
    {
        constexpr char NullTextureVS[] = R"(
Texture2D<float4> g_MissingVSTexture;
float4 main() : SV_Position
{
    return g_MissingVSTexture.Load(int3(0,0,0));
}
)";

        constexpr char DummyVS[] = R"(
float4 main() : SV_Position
{
    return float4(0.0, 0.0, 0.0, 0.0);
}
)";

        constexpr char NullTexturePS[] = R"(
Texture2D<float4> g_MissingPSTexture;
float4 main() : SV_Target
{
    return g_MissingPSTexture.Load(int3(0,0,0));
}
)";

        auto* const pEnv    = TestingEnvironment::GetInstance();
        auto* const pDevice = pEnv->GetDevice();

        // Using null texture in VS results in an exception in WARP, but works OK in PS
        UseDummyVS =
            pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D12 &&
            pDevice->GetAdapterInfo().Type == ADAPTER_TYPE_SOFTWARE;

        pVS = CreateShader("Null texture binding VS", UseDummyVS ? DummyVS : NullTextureVS, SHADER_TYPE_VERTEX);
        pPS = CreateShader("Null texture binding PS", NullTexturePS, SHADER_TYPE_PIXEL);
    }

    static void TearDownTestSuite()
    {
        pVS.Release();
        pPS.Release();
    }
    static bool                   UseDummyVS;
    static RefCntAutoPtr<IShader> pVS;
    static RefCntAutoPtr<IShader> pPS;
};
bool                   NullTexture::UseDummyVS = false;
RefCntAutoPtr<IShader> NullTexture::pVS;
RefCntAutoPtr<IShader> NullTexture::pPS;

TEST_P(NullTexture, Test)
{
    auto* const pEnv       = TestingEnvironment::GetInstance();
    auto* const pDevice    = pEnv->GetDevice();
    const auto& DeviceInfo = pDevice->GetDeviceInfo();

    if (DeviceInfo.IsVulkanDevice())
        GTEST_SKIP() << "Null resources result in device removal in Vulkan";
    else if (DeviceInfo.IsMetalDevice())
        GTEST_SKIP() << "Null resources result in debug break in Metal";
    else if (DeviceInfo.Type == RENDER_DEVICE_TYPE_D3D12 && pDevice->GetAdapterInfo().Type != ADAPTER_TYPE_SOFTWARE)
        GTEST_SKIP() << "Null resources result in device removal in HW D3D12";

    pEnv->SetErrorAllowance(UseDummyVS ? 1 : 2, "No worries, errors are expected: testing null resource bindings\n");
    pEnv->PushExpectedErrorSubstring("No resource is bound to variable 'g_MissingPSTexture'");
    if (!UseDummyVS)
        pEnv->PushExpectedErrorSubstring("No resource is bound to variable 'g_MissingVSTexture'", false);

    if (DeviceInfo.Type == RENDER_DEVICE_TYPE_D3D11)
    {
        pEnv->SetErrorAllowance(4);
        pEnv->PushExpectedErrorSubstring("Shader resource view at slot 0 is null", false);
        pEnv->PushExpectedErrorSubstring("Shader resource view at slot 0 is null", false);
    }

    TestNullResourceBinding(pVS, pPS, GetParam());
}

INSTANTIATE_TEST_SUITE_P(NullResourceBindings,
                         NullTexture,
                         testing::Values(SHADER_RESOURCE_VARIABLE_TYPE_STATIC, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC),
                         [](const testing::TestParamInfo<SHADER_RESOURCE_VARIABLE_TYPE>& info) //
                         {
                             return GetShaderVariableTypeLiteralName(info.param);
                         }); //




class NullRWResources : public testing::TestWithParam<SHADER_RESOURCE_VARIABLE_TYPE>
{
protected:
    static void SetUpTestSuite()
    {
        constexpr char NullRWResourcesCS[] = R"(
RWTexture2D<float4 /*format=rgba32f*/> g_MissingRWTexture;
RWBuffer<float4 /*format=rgba32f*/>    g_MissingRWBuffer;
[numthreads(1, 1, 1)]
void main()
{
    if (g_MissingRWTexture.Load(int2(0, 0)).x == 1.0)
        GroupMemoryBarrierWithGroupSync();
    if (g_MissingRWBuffer.Load(0).x == 1.0)
        GroupMemoryBarrierWithGroupSync();
}
)";
        // NB: writes to null images cause crash in GL. Reads seem to work fine
        pCS = CreateShader("Null RW resource binding CS", NullRWResourcesCS, SHADER_TYPE_COMPUTE);
    }

    static void TearDownTestSuite()
    {
        pCS.Release();
    }
    static RefCntAutoPtr<IShader> pCS;
};
RefCntAutoPtr<IShader> NullRWResources::pCS;

TEST_P(NullRWResources, Test)
{
    auto* const pEnv       = TestingEnvironment::GetInstance();
    auto* const pDevice    = pEnv->GetDevice();
    auto* const pContext   = pEnv->GetDeviceContext();
    const auto& DeviceInfo = pDevice->GetDeviceInfo();

    if (DeviceInfo.IsVulkanDevice())
        GTEST_SKIP() << "Null resources result in device removal in Vulkan";
    else if (DeviceInfo.IsMetalDevice())
        GTEST_SKIP() << "Null resources result in debug break in Metal";
    else if (DeviceInfo.Type == RENDER_DEVICE_TYPE_D3D12 && pDevice->GetAdapterInfo().Type != ADAPTER_TYPE_SOFTWARE)
        GTEST_SKIP() << "Null resources result in device removal in HW D3D12";

    ComputePipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name                               = "Null resource test PSO";
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = GetParam();
    PSOCreateInfo.pCS                                        = pCS;

    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreateComputePipelineState(PSOCreateInfo, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pPSO->CreateShaderResourceBinding(&pSRB, false);

    pEnv->SetErrorAllowance(2, "No worries, errors are expected: testing null resource bindings\n");
    if (DeviceInfo.IsGLDevice())
    {
        pEnv->PushExpectedErrorSubstring("No resource is bound to variable 'g_MissingRWTexture'");
        pEnv->PushExpectedErrorSubstring("No resource is bound to variable 'g_MissingRWBuffer'", false);
    }
    else
    {
        pEnv->PushExpectedErrorSubstring("No resource is bound to variable 'g_MissingRWBuffer'");
        pEnv->PushExpectedErrorSubstring("No resource is bound to variable 'g_MissingRWTexture'", false);
    }

    if (DeviceInfo.Type == RENDER_DEVICE_TYPE_D3D11)
    {
        pEnv->SetErrorAllowance(4);
        pEnv->PushExpectedErrorSubstring("Unordered access view at slot 1 is null", false);
        pEnv->PushExpectedErrorSubstring("Unordered access view at slot 0 is null", false);
    }

    pContext->SetPipelineState(pPSO);
    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->DispatchCompute(DispatchComputeAttribs{1, 1, 1});
}

INSTANTIATE_TEST_SUITE_P(NullResourceBindings,
                         NullRWResources,
                         testing::Values(SHADER_RESOURCE_VARIABLE_TYPE_STATIC, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC),
                         [](const testing::TestParamInfo<SHADER_RESOURCE_VARIABLE_TYPE>& info) //
                         {
                             return GetShaderVariableTypeLiteralName(info.param);
                         }); //
#endif

} // namespace

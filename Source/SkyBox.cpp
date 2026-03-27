//#include "System/Misc.h"
//#include "Skybox.h"
//#include "GpuResourceUtils.h"
//#include "RHI/ICommandList.h"
//
//
//std::unordered_map<std::string, std::unique_ptr<Skybox>> Skybox::s_cache;
//
//Skybox* Skybox::Get(ID3D11Device* device, const std::string& filename)
//{
//    if (filename.empty()) return nullptr;
//
//    auto it = s_cache.find(filename);
//    if (it != s_cache.end())
//    {
//        return it->second.get();
//    }
//
//    auto skybox = std::make_unique<Skybox>(device, filename.c_str());
//    Skybox* ptr = skybox.get();
//    s_cache[filename] = std::move(skybox);
//
//    return ptr;
//}
//
//
//
//Skybox::Skybox(ID3D11Device* device, const char* filename)
//{
//    D3D11_TEXTURE2D_DESC texture2d_desc;
//    GpuResourceUtils::LoadTexture(device, filename, shaderResourceView.GetAddressOf(), &texture2d_desc);
//
//
//    GpuResourceUtils::LoadVertexShader(device, "Data/Shader/SkyBoxVS.cso", nullptr, 0, nullptr, vertexShader.GetAddressOf());
//    GpuResourceUtils::LoadPixelShader(device, "Data/Shader/SkyBoxPS.cso", pixelShader.GetAddressOf());
//
//    D3D11_BUFFER_DESC buffer_desc{};
//    buffer_desc.ByteWidth = sizeof(Constants);
//    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
//    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
//
//    HRESULT hr = device->CreateBuffer(&buffer_desc, nullptr, constantBuffer.GetAddressOf());
//    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//}
//
//void Skybox::Draw(const RenderContext& rc, const DirectX::XMFLOAT4X4& viewProjection)
//{
//    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//    dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
//    dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
//
//    ID3D11SamplerState* samplers[] = {
//        rc.renderState->GetSamplerState(SamplerState::LinearClamp)
//    };
//    dc->PSSetSamplers(0, 1, samplers);
//
//    dc->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
//    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
//    dc->IASetInputLayout(nullptr);
//
//    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
//    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
//
//    dc->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());
//
//    Constants data;
//    DirectX::XMStoreFloat4x4(&data.inverseViewProjection,
//        DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&viewProjection)));
//    dc->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &data, 0, 0);
//    dc->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
//    dc->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
//
//
//    dc->Draw(4, 0);
//
//    dc->VSSetShader(nullptr, nullptr, 0);
//    dc->PSSetShader(nullptr, nullptr, 0);
//}
#include "Skybox.h"
#include "System/Misc.h"
#include "GpuResourceUtils.h"
#include "Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/IShader.h"
#include "RHI/IBuffer.h"
#include "RHI/IPipelineState.h"
#include "RHI/PipelineStateDesc.h"
#include "System/ResourceManager.h"
#include "System/PathResolver.h"
#include "Console/Logger.h"
#include <DirectXTex.h>
#include <vector>
#include <cstring>
#include "RHI/DX12/DX12CommandList.h"
#include "RHI/DX12/DX12Texture.h"

std::unordered_map<std::string, std::unique_ptr<Skybox>> Skybox::s_cache;

Skybox::~Skybox() = default;

Skybox* Skybox::Get(IResourceFactory* factory, const std::string& filename)
{
    if (filename.empty()) return nullptr;

    auto it = s_cache.find(filename);
    if (it != s_cache.end()) return it->second.get();

    auto skybox = std::make_unique<Skybox>(factory, filename.c_str());
    Skybox* ptr = skybox.get();
    s_cache[filename] = std::move(skybox);
    return ptr;
}

Skybox::Skybox(IResourceFactory* factory, const char* filename)
{
    m_cubeTexture = ResourceManager::Instance().GetTexture(filename);
    LOG_INFO("[Skybox] load path=%s texture=%p", filename, m_cubeTexture.get());

    std::string resolvedPath = PathResolver::Resolve(filename);
    std::wstring widePath(resolvedPath.begin(), resolvedPath.end());

    DirectX::TexMetadata metadata = {};
    DirectX::ScratchImage image;
    HRESULT loadHr = DirectX::LoadFromDDSFile(widePath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, image);
    if (SUCCEEDED(loadHr) && metadata.arraySize >= 6) {
        bool allFacesReady = true;
        for (size_t face = 0; face < 6; ++face) {
            DirectX::ScratchImage faceImage;
            HRESULT initHr = faceImage.Initialize2D(metadata.format, metadata.width, metadata.height, 1, metadata.mipLevels);
            if (FAILED(initHr)) {
                allFacesReady = false;
                break;
            }

            for (size_t mip = 0; mip < metadata.mipLevels; ++mip) {
                const DirectX::Image* src = image.GetImage(mip, face, 0);
                const DirectX::Image* dstConst = faceImage.GetImage(mip, 0, 0);
                auto* dst = const_cast<DirectX::Image*>(dstConst);
                if (!src || !dst) {
                    allFacesReady = false;
                    break;
                }

                const size_t rowBytes = (src->rowPitch < dst->rowPitch) ? src->rowPitch : dst->rowPitch;
                for (size_t y = 0; y < src->height; ++y) {
                    memcpy(dst->pixels + y * dst->rowPitch, src->pixels + y * src->rowPitch, rowBytes);
                }
            }

            if (!allFacesReady) {
                break;
            }

            const DirectX::ScratchImage* uploadImage = &faceImage;
            DirectX::ScratchImage convertedFace;
            if (metadata.format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
                HRESULT convertHr = DirectX::Convert(
                    faceImage.GetImages(), faceImage.GetImageCount(), faceImage.GetMetadata(),
                    DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, convertedFace);
                if (SUCCEEDED(convertHr)) {
                    uploadImage = &convertedFace;
                } else {
                    LOG_WARN("[Skybox] face %zu convert failed hr=0x%08X", face, static_cast<unsigned>(convertHr));
                }
            }

            auto faceTexture = factory->CreateTextureFromMemory(*uploadImage, uploadImage->GetMetadata());
            if (!faceTexture) {
                allFacesReady = false;
                break;
            }

            m_faceTextures[face] = std::shared_ptr<ITexture>(faceTexture.release());
        }

        m_hasFaceTextures = allFacesReady;
        LOG_INFO("[Skybox] face-texture path=%s enabled=%d", filename, m_hasFaceTextures ? 1 : 0);

        if (m_hasFaceTextures && Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
            auto* dx12Device = Graphics::Instance().GetDX12Device();
            auto* d3dDevice = dx12Device ? dx12Device->GetDevice() : nullptr;
            if (d3dDevice) {
                D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
                heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                heapDesc.NumDescriptors = 64;
                heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                if (SUCCEEDED(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_dx12SrvHeap)))) {
                    const UINT descriptorSize = dx12Device->GetCBVSRVUAVDescriptorSize();
                    auto cpuBase = m_dx12SrvHeap->GetCPUDescriptorHandleForHeapStart();
                    m_dx12SrvGpuBase = m_dx12SrvHeap->GetGPUDescriptorHandleForHeapStart();

                    D3D12_SHADER_RESOURCE_VIEW_DESC nullDesc = {};
                    nullDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    nullDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    nullDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    nullDesc.Texture2D.MipLevels = 1;

                    for (UINT slot = 0; slot < 64; ++slot) {
                        auto dst = cpuBase;
                        dst.ptr += static_cast<SIZE_T>(slot) * descriptorSize;
                        d3dDevice->CreateShaderResourceView(nullptr, &nullDesc, dst);
                    }

                    for (UINT face = 0; face < 6; ++face) {
                        auto* dx12Face = dynamic_cast<DX12Texture*>(m_faceTextures[face].get());
                        if (!dx12Face || !dx12Face->HasSRV()) {
                            m_dx12SrvHeap.Reset();
                            break;
                        }
                        auto dst = cpuBase;
                        dst.ptr += static_cast<SIZE_T>(face) * descriptorSize;
                        d3dDevice->CopyDescriptorsSimple(1, dst, dx12Face->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                    }

                    LOG_INFO("[Skybox] dedicated DX12 SRV heap=%p", m_dx12SrvHeap.Get());
                }
            }
        }
    } else {
        LOG_WARN("[Skybox] failed to load cubemap faces path=%s hr=0x%08X", filename, static_cast<unsigned>(loadHr));
    }

    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/SkyBoxVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel, "Data/Shader/SkyBoxPS.cso");
    m_cb = factory->CreateBuffer(sizeof(Constants), BufferType::Constant);

    PipelineStateDesc desc{};
    desc.vertexShader = m_vs.get();
    desc.pixelShader = m_ps.get();
    desc.inputLayout = nullptr;
    desc.primitiveTopology = PrimitiveTopology::TriangleStrip;

    auto* rs = Graphics::Instance().GetRenderState();
    desc.rasterizerState = rs->GetRasterizerState(RasterizerState::SolidCullNone);
    desc.depthStencilState = rs->GetDepthStencilState(DepthState::TestAndWrite);
    desc.blendState = rs->GetBlendState(BlendState::Opaque);
    desc.numRenderTargets = 1;
    desc.rtvFormats[0] = TextureFormat::R16G16B16A16_FLOAT;
    desc.dsvFormat = TextureFormat::D32_FLOAT;

    m_pso = factory->CreatePipelineState(desc);
}


void Skybox::Draw(const RenderContext& rc, const DirectX::XMFLOAT4X4& viewProjection)
{
    rc.commandList->SetPipelineState(m_pso.get());

    Constants data;
    DirectX::XMMATRIX vp = DirectX::XMLoadFloat4x4(&viewProjection);
    DirectX::XMStoreFloat4x4(&data.inverseViewProjection, DirectX::XMMatrixInverse(nullptr, vp));

    if (auto* dx12Cmd = dynamic_cast<DX12CommandList*>(rc.commandList)) {
        dx12Cmd->VSSetDynamicConstantBuffer(0, &data, sizeof(data));
        dx12Cmd->PSSetDynamicConstantBuffer(0, &data, sizeof(data));
    } else {
        rc.commandList->UpdateBuffer(m_cb.get(), &data, sizeof(data));
        rc.commandList->VSSetConstantBuffer(0, m_cb.get());
        rc.commandList->PSSetConstantBuffer(0, m_cb.get());
    }

    // Use standard PSSetTextures path for all APIs.
    // Avoids SetDescriptorHeaps thrashing which invalidates root descriptor tables.
    if (m_hasFaceTextures) {
        ITexture* faces[6] = {
            m_faceTextures[0].get(), m_faceTextures[1].get(), m_faceTextures[2].get(),
            m_faceTextures[3].get(), m_faceTextures[4].get(), m_faceTextures[5].get()
        };
        rc.commandList->PSSetTextures(0, 6, faces);
    } else {
        rc.commandList->PSSetTexture(0, m_cubeTexture.get());
    }

    auto* sampler = rc.renderState->GetSamplerState(SamplerState::LinearClamp);
    rc.commandList->PSSetSampler(3, sampler);

    rc.commandList->SetVertexBuffer(0, nullptr, 0, 0);
    rc.commandList->Draw(4, 0);

    for (uint32_t slot = 0; slot < 6; ++slot) {
        rc.commandList->PSSetTexture(slot, nullptr);
    }
}

#include "CurlNoiseGenerator.h"
#include "FastNoiseLite.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>

using namespace Microsoft::WRL;
using namespace DirectX;



HRESULT CurlNoiseGenerator::CreateCurlNoiseTexture(
    ID3D11Device* device,
    const Config& config,
    ComPtr<ID3D11ShaderResourceView>& outSRV)
{
    if (!device) return E_INVALIDARG;

    // -------------------------------------------------------------
    // -------------------------------------------------------------
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFrequency(config.frequency);
    noise.SetSeed(config.seed);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(3);

    // -------------------------------------------------------------
    // -------------------------------------------------------------
    size_t totalPixels = (size_t)config.width * config.height * config.depth;

    std::vector<float> data(totalPixels * 4);

    const float e = 1.0f;

    const float offset_y = 1000.0f;
    const float offset_z = 2000.0f;

    // -------------------------------------------------------------
    // -------------------------------------------------------------
    for (int z = 0; z < config.depth; ++z)
    {
        for (int y = 0; y < config.height; ++y)
        {
            for (int x = 0; x < config.width; ++x)
            {
                float fx = (float)x;
                float fy = (float)y;
                float fz = (float)z;

                auto samplePotential = [&](float _x, float _y, float _z) {
                    float p1 = noise.GetNoise(_x, _y, _z);            // Psi_x
                    float p2 = noise.GetNoise(_x, _y + offset_y, _z); // Psi_y
                    float p3 = noise.GetNoise(_x, _y, _z + offset_z); // Psi_z
                    return XMFLOAT3(p1, p2, p3);
                    };

                XMFLOAT3 p_y0 = samplePotential(fx, fy - e, fz);
                XMFLOAT3 p_y1 = samplePotential(fx, fy + e, fz);

                XMFLOAT3 p_z0 = samplePotential(fx, fy, fz - e);
                XMFLOAT3 p_z1 = samplePotential(fx, fy, fz + e);

                XMFLOAT3 p_x0 = samplePotential(fx - e, fy, fz);
                XMFLOAT3 p_x1 = samplePotential(fx + e, fy, fz);

                float div = 1.0f / (2.0f * e);

                float dP3_dy = (p_y1.z - p_y0.z) * div; // d(Psi_z)/dy
                float dP2_dz = (p_z1.y - p_z0.y) * div; // d(Psi_y)/dz

                float dP1_dz = (p_z1.x - p_z0.x) * div; // d(Psi_x)/dz
                float dP3_dx = (p_x1.z - p_x0.z) * div; // d(Psi_z)/dx

                float dP2_dx = (p_x1.y - p_x0.y) * div; // d(Psi_y)/dx
                float dP1_dy = (p_y1.x - p_y0.x) * div; // d(Psi_x)/dy

                float vx = dP3_dy - dP2_dz;
                float vy = dP1_dz - dP3_dx;
                float vz = dP2_dx - dP1_dy;

                size_t index = ((size_t)z * config.height * config.width + (size_t)y * config.width + x) * 4;
                data[index + 0] = vx; // R
                data[index + 1] = vy; // G
                data[index + 2] = vz; // B
                data[index + 3] = 1.0f; // A
            }
        }
    }

    // -------------------------------------------------------------
    // -------------------------------------------------------------
    D3D11_TEXTURE3D_DESC desc = {};
    desc.Width = config.width;
    desc.Height = config.height;
    desc.Depth = config.depth;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data.data();
    initData.SysMemPitch = config.width * sizeof(float) * 4;
    initData.SysMemSlicePitch = config.width * config.height * sizeof(float) * 4;

    ComPtr<ID3D11Texture3D> texture;
    HRESULT hr = device->CreateTexture3D(&desc, &initData, texture.GetAddressOf());
    if (FAILED(hr)) return hr;

    // -------------------------------------------------------------
    // -------------------------------------------------------------
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MostDetailedMip = 0;
    srvDesc.Texture3D.MipLevels = 1;

    return device->CreateShaderResourceView(texture.Get(), &srvDesc, outSRV.GetAddressOf());
}

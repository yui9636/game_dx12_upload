//#include "ShadowMap.h"
//#include "System/Misc.h"
//#include "GpuResourceUtils.h"
//#include "RenderContext/RenderContext.h"
//#include "Actor/Actor.h"
//#include <algorithm>
//#include <cmath>
//#include "RHI/ICommandList.h"
//
//using namespace DirectX;
//using namespace Microsoft::WRL;
//
//ShadowMap::ShadowMap(ID3D11Device* device)
//{
//    // 1. 入力レイアウト定義
//    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
//    {
//        {"POSITION",     0,DXGI_FORMAT_R32G32B32_FLOAT,   0,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
//        {"BONE_WEIGHTS",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
//        {"BONE_INDICES",0,DXGI_FORMAT_R32G32B32A32_UINT, 0,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
//    };
//
//    GpuResourceUtils::LoadVertexShader(
//        device,
//        "Data/Shader/ShadowMapVS.cso",
//        inputElementDesc,
//        _countof(inputElementDesc),
//        inputLayout.GetAddressOf(),
//        vertexShader.GetAddressOf());
//
//    GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbScene), sceneConstantBuffer.GetAddressOf());
//    GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbSkeleton), skeletonConstantBuffer.GetAddressOf());
//
//    // -----------------------------------------------------------------------
//    // 2. Texture2D Array 作成 (3枚分)
//    // -----------------------------------------------------------------------
//    D3D11_TEXTURE2D_DESC texture2dDesc{};
//    texture2dDesc.Width = textureSize;
//    texture2dDesc.Height = textureSize;
//    texture2dDesc.MipLevels = 1;
//    texture2dDesc.ArraySize = CASCADE_COUNT; // ★3枚作成
//    texture2dDesc.Format = DXGI_FORMAT_R32_TYPELESS;
//    texture2dDesc.SampleDesc.Count = 1;
//    texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
//    texture2dDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
//
//    ComPtr<ID3D11Texture2D> texture2d;
//    HRESULT hr = device->CreateTexture2D(&texture2dDesc, 0, texture2d.GetAddressOf());
//    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//    // DSV 作成 (スライスごとに作成)
//    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
//    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
//    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
//    dsvDesc.Texture2DArray.ArraySize = 1; // 1つずつビューを作る
//    dsvDesc.Texture2DArray.MipSlice = 0;
//
//    depthStencilViews.resize(CASCADE_COUNT);
//    for (int i = 0; i < CASCADE_COUNT; ++i)
//    {
//        dsvDesc.Texture2DArray.FirstArraySlice = i; // i番目のテクスチャを見る
//        hr = device->CreateDepthStencilView(texture2d.Get(), &dsvDesc, depthStencilViews[i].GetAddressOf());
//        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//    }
//
//    // SRV 作成 (全スライスまとめて)
//    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
//    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
//    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY; // ★配列として読む
//    srvDesc.Texture2DArray.ArraySize = CASCADE_COUNT;
//    srvDesc.Texture2DArray.FirstArraySlice = 0;
//    srvDesc.Texture2DArray.MipLevels = 1;
//    hr = device->CreateShaderResourceView(texture2d.Get(), &srvDesc, shaderResourceView.GetAddressOf());
//    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//    // 3. サンプラー作成
//    D3D11_SAMPLER_DESC samplerDesc{};
//    samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
//    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
//    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
//    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
//    samplerDesc.BorderColor[0] = 1.0f;
//    samplerDesc.BorderColor[1] = 1.0f;
//    samplerDesc.BorderColor[2] = 1.0f;
//    samplerDesc.BorderColor[3] = 1.0f;
//    samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
//
//    hr = device->CreateSamplerState(&samplerDesc, samplerState.GetAddressOf());
//    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//}
//
//// -----------------------------------------------------------------------
//// 数学ヘルパー: 視錐台のコーナー8点を取得
//// -----------------------------------------------------------------------
//std::array<XMVECTOR, 8> ShadowMap::GetFrustumCorners(float fov, float aspect, float nearZ, float farZ, const XMFLOAT4X4& viewMat)
//{
//    // ★ゼロ初期化でエラー回避
//    std::array<XMVECTOR, 8> corners = {};
//
//    XMMATRIX proj = XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);
//    XMMATRIX view = XMLoadFloat4x4(&viewMat);
//    XMMATRIX invViewProj = XMMatrixInverse(nullptr, view * proj);
//
//    XMVECTOR ndcCorners[8] = {
//        XMVectorSet(-1.0f,  1.0f, 0.0f, 1.0f),
//        XMVectorSet(1.0f,  1.0f, 0.0f, 1.0f),
//        XMVectorSet(1.0f, -1.0f, 0.0f, 1.0f),
//        XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f),
//        XMVectorSet(-1.0f,  1.0f, 1.0f, 1.0f),
//        XMVectorSet(1.0f,  1.0f, 1.0f, 1.0f),
//        XMVectorSet(1.0f, -1.0f, 1.0f, 1.0f),
//        XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f),
//    };
//
//    for (int i = 0; i < 8; ++i)
//    {
//        corners[i] = XMVector3TransformCoord(ndcCorners[i], invViewProj);
//    }
//    return corners;
//}
//
//// -----------------------------------------------------------------------
//// 数学ヘルパー: カスケード1枚分の行列計算
//// -----------------------------------------------------------------------
//XMMATRIX ShadowMap::CalcCascadeMatrix(const RenderContext& rc, float nearZ, float farZ)
//{
//    // ★重要: const_castでCamera.hの変更を不要にする
//    auto corners = GetFrustumCorners(rc.fovY, rc.aspect, nearZ, farZ, rc.viewMatrix);
//
//
//    // コーナー群の中心(重心)を計算
//    XMVECTOR center = XMVectorZero();
//    for (const auto& v : corners) center += v;
//    center /= 8.0f;
//
//    // ライトのビュー行列
//    const DirectionalLight& dLight = rc.directionalLight;
//    XMVECTOR lightDir = XMLoadFloat3(&dLight.direction);
//    if (XMVectorGetX(XMVector3LengthSq(lightDir)) < 0.0001f) lightDir = XMVectorSet(0.1f, -1.0f, 0.1f, 0);
//    lightDir = XMVector3Normalize(lightDir);
//
//    // 中心からライト逆方向に十分離れた点
//    // 正投影なので距離自体は画角に影響しませんが、手前のオブジェクトを含むために距離を取ります
//    XMVECTOR lightPos = center - lightDir * 5000.0f;
//    XMVECTOR up = (fabsf(XMVectorGetY(lightDir)) > 0.99f) ? XMVectorSet(0, 0, 1, 0) : XMVectorSet(0, 1, 0, 0);
//
//    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, center, up);
//
//    // 視錐台のコーナーをライト空間に変換してAABBを求める
//    XMVECTOR minBox = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
//    XMVECTOR maxBox = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);
//
//    for (const auto& v : corners)
//    {
//        XMVECTOR lv = XMVector3Transform(v, lightView); // ライト空間へ
//        minBox = XMVectorMin(minBox, lv);
//        maxBox = XMVectorMax(maxBox, lv);
//    }
//
//    // ★テクセル・スナップ処理 (影のチラつき防止)
//    float w = XMVectorGetX(maxBox) - XMVectorGetX(minBox);
//    float h = XMVectorGetY(maxBox) - XMVectorGetY(minBox);
//    float worldUnitsPerTexel = max(w, h) / static_cast<float>(textureSize);
//
//    XMVECTOR vWorldUnits = XMVectorSet(worldUnitsPerTexel, worldUnitsPerTexel, worldUnitsPerTexel, 0.0f);
//
//    minBox = XMVectorFloor(XMVectorDivide(minBox, vWorldUnits));
//    minBox = XMVectorMultiply(minBox, vWorldUnits);
//
//    maxBox = XMVectorFloor(XMVectorDivide(maxBox, vWorldUnits));
//    maxBox = XMVectorMultiply(maxBox, vWorldUnits);
//
//    // 正投影行列を作る
//    float minX = XMVectorGetX(minBox);
//    float maxX = XMVectorGetX(maxBox);
//    float minY = XMVectorGetY(minBox);
//    float maxY = XMVectorGetY(maxBox);
//    float minZ = XMVectorGetZ(minBox);
//    float maxZ = XMVectorGetZ(maxBox);
//
//    // Z手前方向へ拡張 (遮蔽物を含めるため)
//    minZ -= 5000.0f;
//
//    XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(
//        minX, maxX,
//        minY, maxY,
//        minZ, maxZ
//    );
//
//    return lightView * lightProj;
//}
//
//// -----------------------------------------------------------------------
//// 毎フレーム呼ぶ: 全カスケードの行列計算
//// -----------------------------------------------------------------------
//void ShadowMap::UpdateCascades(const RenderContext& rc)
//{
//    // ★重要: const_castでCamera.hの変更を不要にする
//    float camNear = rc.nearZ;
//    float camFar = rc.farZ;
//    float clipRange = camFar - camNear;
//
//    float prevSplitDist = camNear;
//
//
//    for (int i = 0; i < CASCADE_COUNT; ++i)
//    {
//        // 割合(0.001, 0.01, 0.1) から距離を決定
//        float splitDist = camNear + (cascadeSplits[i] * clipRange);
//
//        // 保存 (Shaderへ送る用)
//        cascadeEndClips[i] = splitDist;
//
//        // 行列計算
//        XMMATRIX mat = CalcCascadeMatrix(rc, prevSplitDist, splitDist);
//        XMStoreFloat4x4(&shadowMatrices[i], mat);
//
//        prevSplitDist = splitDist;
//    }
//}
//
//// -----------------------------------------------------------------------
//// 特定のカスケードへの描画開始
//// -----------------------------------------------------------------------
//void ShadowMap::BeginCascade(const RenderContext& rc, int cascadeIndex)
//{
//    if (cascadeIndex < 0 || cascadeIndex >= CASCADE_COUNT) return;
//
//    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//    ID3D11ShaderResourceView* nullSRV = nullptr;
//    dc->PSSetShaderResources(4, 1, &nullSRV);
//
//    // 最初の1回だけビューポート等を保存
//    if (cascadeIndex == 0)
//    {
//        UINT num = 1;
//        dc->RSGetViewports(&num, &cachedViewport);
//        dc->OMGetRenderTargets(1, cachedRenderTargetView.ReleaseAndGetAddressOf(), cachedDepthStencilView.ReleaseAndGetAddressOf());
//    }
//
//    // ビューポート設定
//    D3D11_VIEWPORT vp{};
//    vp.Width = static_cast<float>(textureSize);
//    vp.Height = static_cast<float>(textureSize);
//    vp.MinDepth = 0.0f;
//    vp.MaxDepth = 1.0f;
//    dc->RSSetViewports(1, &vp);
//
//    // レンダーターゲット設定
//    ID3D11RenderTargetView* nullRTV = nullptr;
//    dc->OMSetRenderTargets(0, &nullRTV, depthStencilViews[cascadeIndex].Get());
//    dc->ClearDepthStencilView(depthStencilViews[cascadeIndex].Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
//
//    // 定数バッファ更新
//    CbScene cbScene{};
//    cbScene.lightViewProjection = shadowMatrices[cascadeIndex];
//    dc->UpdateSubresource(sceneConstantBuffer.Get(), 0, nullptr, &cbScene, 0, 0);
//
//    // パイプライン設定
//    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
//    dc->VSSetConstantBuffers(0, 1, sceneConstantBuffer.GetAddressOf());
//    dc->PSSetShader(nullptr, nullptr, 0);
//    dc->IASetInputLayout(inputLayout.Get());
//    dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
//
//    // ★追加: 深度ステートを「テスト有効・書き込み有効」に強制設定
//    // これがないと、前のパス(UI等)の影響で書き込みが無効化される場合があります
//    dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
//
//
//}
//
//// -----------------------------------------------------------------------
//// 描画終了
//// -----------------------------------------------------------------------
//void ShadowMap::End(const RenderContext& rc)
//{
//    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//    dc->RSSetViewports(1, &cachedViewport);
//    dc->OMSetRenderTargets(1, cachedRenderTargetView.GetAddressOf(), cachedDepthStencilView.Get());
//
//    cachedRenderTargetView.Reset();
//    cachedDepthStencilView.Reset();
//}
//
//// --- 以下、描画関数 (変更なし) ---
//
//void ShadowMap::DrawSceneImmediate(const RenderContext& rc, const std::vector<std::shared_ptr<Actor>>& actors)
//{
//    for (auto& actor : actors)
//    {
//        Model* model = actor->GetModelRaw();
//        if (!model) continue;
//        this->Draw(rc, model->GetModelResource().get(), actor->GetTransform());
//    }
//}
//
//void ShadowMap::Draw(const RenderContext& rc, const Model* model, const DirectX::XMFLOAT4X4& worldMatrix)
//{
//    if (!modelResource) return;
//    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//    DirectX::XMMATRIX W_Actor = DirectX::XMLoadFloat4x4(&worldMatrix);
//
//    for (const Model::Mesh& mesh : model->GetMeshes())
//    {
//        UINT stride = sizeof(Model::Vertex);
//        UINT offset = 0;
//        dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
//        dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
//        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//
//        CbSkeleton cbSkeleton{};
//        if (mesh.bones.size() > 0)
//        {
//            for (size_t i = 0; i < mesh.bones.size(); ++i)
//            {
//                const Model::Bone& bone = mesh.bones.at(i);
//                DirectX::XMMATRIX NodeTransform = DirectX::XMLoadFloat4x4(&bone.node->worldTransform);
//                DirectX::XMMATRIX OffsetTransform = DirectX::XMLoadFloat4x4(&bone.offsetTransform);
//                DirectX::XMMATRIX BoneTransform = OffsetTransform * NodeTransform * W_Actor;
//                DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], BoneTransform);
//            }
//        }
//        else
//        {
//            DirectX::XMMATRIX NodeTransform = DirectX::XMLoadFloat4x4(&mesh.node->worldTransform);
//            DirectX::XMMATRIX WorldTransform = NodeTransform * W_Actor;
//            DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[0], WorldTransform);
//        }
//
//        dc->VSSetConstantBuffers(6, 1, skeletonConstantBuffer.GetAddressOf());
//        dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, nullptr, &cbSkeleton, 0, 0);
//        dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
//    }
//}
//#include "ShadowMap.h"
//#include "System/Misc.h"
//#include "GpuResourceUtils.h"
//#include "RenderContext/RenderContext.h"
//#include "Actor/Actor.h"
//#include <algorithm>
//#include <cmath>
//#include "RHI/ICommandList.h"
//#include "RHI/IShader.h"
//#include "RHI/DX11/DX11Shader.h"
//#include "RHI/IBuffer.h"
//#include "RHI/DX11/DX11Buffer.h"
//#include "RHI/ISampler.h"
//#include "RHI/DX11/DX11Sampler.h"
//#include "RHI/IState.h"
//#include "RHI/DX11/DX11State.h"
//#include <RHI\DX11\DX11Texture.h>
//#include "RHI/PipelineStateDesc.h"
//#include "RHI/IPipelineState.h"
//#include "Graphics.h"
//
//using namespace DirectX;
//using namespace Microsoft::WRL;
//
//// ★ デストラクタの実体
//ShadowMap::~ShadowMap() = default;
//
//ShadowMap::ShadowMap(ID3D11Device* device)
//{
//    // 1. 入力レイアウト定義
//    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
//    {
//        {"POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
//        {"BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
//        {"BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
//    };
//
//    // 頂点シェーダーの RHI 生成
//    vertexShader = std::make_unique<DX11Shader>(device, ShaderType::Vertex, "Data/Shader/ShadowMapVS.cso");
//
//    // インプットレイアウトの RHI 生成
//    auto dx11VS = static_cast<DX11Shader*>(vertexShader.get());
//    ComPtr<ID3D11InputLayout> rawLayout;
//    HRESULT hr = device->CreateInputLayout(
//        inputElementDesc, _countof(inputElementDesc),
//        dx11VS->GetByteCode(), dx11VS->GetByteCodeSize(),
//        rawLayout.GetAddressOf());
//    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//    inputLayout = std::make_unique<DX11InputLayout>(rawLayout.Get());
//
//    // 定数バッファの RHI 生成
//    sceneConstantBuffer = std::make_unique<DX11Buffer>(device, sizeof(CbScene), BufferType::Constant);
//    skeletonConstantBuffer = std::make_unique<DX11Buffer>(device, sizeof(CbSkeleton), BufferType::Constant);
//
//    // -----------------------------------------------------------------------
//    // 2. Texture2D Array 作成 (3枚分) ※ここはDX11依存のまま
//    // -----------------------------------------------------------------------
//    D3D11_TEXTURE2D_DESC texture2dDesc{};
//    texture2dDesc.Width = textureSize;
//    texture2dDesc.Height = textureSize;
//    texture2dDesc.MipLevels = 1;
//    texture2dDesc.ArraySize = CASCADE_COUNT;
//    texture2dDesc.Format = DXGI_FORMAT_R32_TYPELESS;
//    texture2dDesc.SampleDesc.Count = 1;
//    texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
//    texture2dDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
//
//    ComPtr<ID3D11Texture2D> texture2d;
//    hr = device->CreateTexture2D(&texture2dDesc, 0, texture2d.GetAddressOf());
//    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
//    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
//    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
//    dsvDesc.Texture2DArray.ArraySize = 1;
//    dsvDesc.Texture2DArray.MipSlice = 0;
//
//    m_cascadeTextures.resize(CASCADE_COUNT);
//    for (int i = 0; i < CASCADE_COUNT; ++i)
//    {
//        ComPtr<ID3D11DepthStencilView> rawDSV;
//        dsvDesc.Texture2DArray.FirstArraySlice = i;
//        hr = device->CreateDepthStencilView(texture2d.Get(), &dsvDesc, rawDSV.GetAddressOf());
//        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//        m_cascadeTextures[i] = std::make_shared<DX11Texture>(rawDSV.Get(), textureSize, textureSize);
//    }
//
//    // ★ 修正：生のSRVを作成し、DX11Textureでラップする
//    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
//    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
//    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
//    srvDesc.Texture2DArray.ArraySize = CASCADE_COUNT;
//    srvDesc.Texture2DArray.FirstArraySlice = 0;
//    srvDesc.Texture2DArray.MipLevels = 1;
//
//    ComPtr<ID3D11ShaderResourceView> rawSRV;
//    hr = device->CreateShaderResourceView(texture2d.Get(), &srvDesc, rawSRV.GetAddressOf());
//    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//    m_shadowTexture = std::make_shared<DX11Texture>(rawSRV.Get());
//
//    // 3. サンプラーの RHI 生成
//    D3D11_SAMPLER_DESC samplerDesc{};
//    samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
//    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
//    samplerDesc.BorderColor[0] = samplerDesc.BorderColor[1] = samplerDesc.BorderColor[2] = samplerDesc.BorderColor[3] = 1.0f;
//    samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
//
//    samplerState = std::make_unique<DX11Sampler>(device, samplerDesc);
//}
//
//// -----------------------------------------------------------------------
//// 数学ヘルパー: 視錐台のコーナー8点を取得
//// -----------------------------------------------------------------------
//std::array<XMVECTOR, 8> ShadowMap::GetFrustumCorners(float fov, float aspect, float nearZ, float farZ, const XMFLOAT4X4& viewMat)
//{
//    std::array<XMVECTOR, 8> corners = {};
//
//    XMMATRIX proj = XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);
//    XMMATRIX view = XMLoadFloat4x4(&viewMat);
//    XMMATRIX invViewProj = XMMatrixInverse(nullptr, view * proj);
//
//    XMVECTOR ndcCorners[8] = {
//        XMVectorSet(-1.0f,  1.0f, 0.0f, 1.0f),
//        XMVectorSet(1.0f,  1.0f, 0.0f, 1.0f),
//        XMVectorSet(1.0f, -1.0f, 0.0f, 1.0f),
//        XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f),
//        XMVectorSet(-1.0f,  1.0f, 1.0f, 1.0f),
//        XMVectorSet(1.0f,  1.0f, 1.0f, 1.0f),
//        XMVectorSet(1.0f, -1.0f, 1.0f, 1.0f),
//        XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f),
//    };
//
//    for (int i = 0; i < 8; ++i)
//    {
//        corners[i] = XMVector3TransformCoord(ndcCorners[i], invViewProj);
//    }
//    return corners;
//}
//
//// -----------------------------------------------------------------------
//// 数学ヘルパー: カスケード1枚分の行列計算
//// -----------------------------------------------------------------------
//XMMATRIX ShadowMap::CalcCascadeMatrix(const RenderContext& rc, float nearZ, float farZ)
//{
//    auto corners = GetFrustumCorners(rc.fovY, rc.aspect, nearZ, farZ, rc.viewMatrix);
//
//    XMVECTOR center = XMVectorZero();
//    for (const auto& v : corners) center += v;
//    center /= 8.0f;
//
//    const DirectionalLight& dLight = rc.directionalLight;
//    XMVECTOR lightDir = XMLoadFloat3(&dLight.direction);
//    if (XMVectorGetX(XMVector3LengthSq(lightDir)) < 0.0001f) lightDir = XMVectorSet(0.1f, -1.0f, 0.1f, 0);
//    lightDir = XMVector3Normalize(lightDir);
//
//    XMVECTOR lightPos = center - lightDir * 5000.0f;
//    XMVECTOR up = (fabsf(XMVectorGetY(lightDir)) > 0.99f) ? XMVectorSet(0, 0, 1, 0) : XMVectorSet(0, 1, 0, 0);
//
//    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, center, up);
//
//    XMVECTOR minBox = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
//    XMVECTOR maxBox = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);
//
//    for (const auto& v : corners)
//    {
//        XMVECTOR lv = XMVector3Transform(v, lightView);
//        minBox = XMVectorMin(minBox, lv);
//        maxBox = XMVectorMax(maxBox, lv);
//    }
//
//    float w = XMVectorGetX(maxBox) - XMVectorGetX(minBox);
//    float h = XMVectorGetY(maxBox) - XMVectorGetY(minBox);
//    float worldUnitsPerTexel = max(w, h) / static_cast<float>(textureSize);
//
//    XMVECTOR vWorldUnits = XMVectorSet(worldUnitsPerTexel, worldUnitsPerTexel, worldUnitsPerTexel, 0.0f);
//    minBox = XMVectorFloor(XMVectorDivide(minBox, vWorldUnits));
//    minBox = XMVectorMultiply(minBox, vWorldUnits);
//    maxBox = XMVectorFloor(XMVectorDivide(maxBox, vWorldUnits));
//    maxBox = XMVectorMultiply(maxBox, vWorldUnits);
//
//    float minX = XMVectorGetX(minBox);
//    float maxX = XMVectorGetX(maxBox);
//    float minY = XMVectorGetY(minBox);
//    float maxY = XMVectorGetY(maxBox);
//    float minZ = XMVectorGetZ(minBox);
//    float maxZ = XMVectorGetZ(maxBox);
//    minZ -= 5000.0f;
//
//    return lightView * XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);
//}
//
//// -----------------------------------------------------------------------
//// 毎フレーム呼ぶ: 全カスケードの行列計算
//// -----------------------------------------------------------------------
//void ShadowMap::UpdateCascades(const RenderContext& rc)
//{
//    float camNear = rc.nearZ;
//    float camFar = rc.farZ;
//    float clipRange = camFar - camNear;
//    float prevSplitDist = camNear;
//
//    for (int i = 0; i < CASCADE_COUNT; ++i)
//    {
//        float splitDist = camNear + (cascadeSplits[i] * clipRange);
//        cascadeEndClips[i] = splitDist;
//        XMMATRIX mat = CalcCascadeMatrix(rc, prevSplitDist, splitDist);
//        XMStoreFloat4x4(&shadowMatrices[i], mat);
//        prevSplitDist = splitDist;
//    }
//}
//
//// -----------------------------------------------------------------------
//// 特定のカスケードへの描画開始
//// -----------------------------------------------------------------------
//void ShadowMap::BeginCascade(const RenderContext& rc, int cascadeIndex)
//{
//    if (cascadeIndex < 0 || cascadeIndex >= CASCADE_COUNT) return;
//
//    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//    // スロット4のSRV解除
//    ID3D11ShaderResourceView* nullSRV = nullptr;
//    dc->PSSetShaderResources(4, 1, &nullSRV);
//
//    if (cascadeIndex == 0)
//    {
//        UINT num = 1;
//        dc->RSGetViewports(&num, &cachedViewport);
//        dc->OMGetRenderTargets(1, cachedRenderTargetView.ReleaseAndGetAddressOf(), cachedDepthStencilView.ReleaseAndGetAddressOf());
//    }
//
//    // ビューポートを RHI 化
//    rc.commandList->SetViewport(0.0f, 0.0f, (float)textureSize, (float)textureSize);
//
//    // レンダーターゲット設定 (DSVはComPtrのため dc を使用)
//    rc.commandList->SetRenderTarget(nullptr, m_cascadeTextures[cascadeIndex].get());
//    rc.commandList->ClearDepthStencil(m_cascadeTextures[cascadeIndex].get(), 1.0f, 0);
//
//
//    // 定数バッファ更新
//    CbScene cbScene{};
//    cbScene.lightViewProjection = shadowMatrices[cascadeIndex];
//    //ID3D11Resource* cbRes = static_cast<ID3D11Resource*>(sceneConstantBuffer->GetNativeBuffer());
//    //dc->UpdateSubresource(cbRes, 0, nullptr, &cbScene, 0, 0);
//    rc.commandList->UpdateBuffer(sceneConstantBuffer.get(), &cbScene, sizeof(cbScene));
//
//
//
//    // パイプライン設定を RHI 化
//    rc.commandList->VSSetShader(vertexShader.get());
//    rc.commandList->VSSetConstantBuffer(0, sceneConstantBuffer.get());
//    rc.commandList->PSSetShader(nullptr);
//    rc.commandList->SetInputLayout(inputLayout.get());
//    rc.commandList->SetRasterizerState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
//    rc.commandList->SetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
//}
//
//// -----------------------------------------------------------------------
//// 描画終了
//// -----------------------------------------------------------------------
//void ShadowMap::End(const RenderContext& rc)
//{
//    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//    // 元のビューポートに復帰
//    rc.commandList->SetViewport(cachedViewport.TopLeftX, cachedViewport.TopLeftY, cachedViewport.Width, cachedViewport.Height, cachedViewport.MinDepth, cachedViewport.MaxDepth);
//    dc->OMSetRenderTargets(1, cachedRenderTargetView.GetAddressOf(), cachedDepthStencilView.Get());
//
//    cachedRenderTargetView.Reset();
//    cachedDepthStencilView.Reset();
//}
//
//void ShadowMap::DrawSceneImmediate(const RenderContext& rc, const std::vector<std::shared_ptr<Actor>>& actors)
//{
//    for (auto& actor : actors)
//    {
//        Model* model = actor->GetModelRaw();
//        if (!model) continue;
//        this->Draw(rc, model->GetModelResource().get(), actor->GetTransform());
//    }
//}
//
//void ShadowMap::Draw(const RenderContext& rc, const Model* model, const DirectX::XMFLOAT4X4& worldMatrix)
//{
//    if (!modelResource) return;
//    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//    DirectX::XMMATRIX W_Actor = DirectX::XMLoadFloat4x4(&worldMatrix);
//
//    for (const Model::Mesh& mesh : model->GetMeshes())
//    {
//        UINT stride = sizeof(Model::Vertex);
//        UINT offset = 0;
//
//        // 頂点・インデックスバッファは mesh 側が ComPtr のため dc を使用
//        dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
//        dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
//
//        // トポロジー設定を RHI 化
//        rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);
//
//        // スケルトン計算
//        CbSkeleton cbSkeleton{};
//        if (mesh.bones.size() > 0)
//        {
//            for (size_t i = 0; i < mesh.bones.size(); ++i)
//            {
//                const Model::Bone& bone = mesh.bones.at(i);
//                XMMATRIX NodeTransform = XMLoadFloat4x4(&bone.node->worldTransform);
//                XMMATRIX OffsetTransform = XMLoadFloat4x4(&bone.offsetTransform);
//                XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], OffsetTransform * NodeTransform * W_Actor);
//            }
//        }
//        else
//        {
//            XMMATRIX NodeTransform = XMLoadFloat4x4(&mesh.node->worldTransform);
//            XMStoreFloat4x4(&cbSkeleton.boneTransforms[0], NodeTransform * W_Actor);
//        }
//
//        // 定数バッファバインドを RHI 化
//        rc.commandList->VSSetConstantBuffer(6, skeletonConstantBuffer.get());
//
//        // バッファ更新
//        //ID3D11Resource* cbRes = static_cast<ID3D11Resource*>(skeletonConstantBuffer->GetNativeBuffer());
//        //dc->UpdateSubresource(cbRes, 0, nullptr, &cbSkeleton, 0, 0);
//
//        rc.commandList->UpdateBuffer(skeletonConstantBuffer.get(), &cbSkeleton, sizeof(cbSkeleton));
//
//        // 描画実行を RHI 化
//        rc.commandList->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
//    }
//}
#include "ShadowMap.h"
#include "System/Misc.h"
#include "GpuResourceUtils.h"
#include "RenderContext/RenderContext.h"
#include <algorithm>
#include <cmath>

// RHI 関連
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/IShader.h"
#include "RHI/IBuffer.h"
#include "RHI/ISampler.h"
#include "RHI/IState.h"
#include "RHI/ITexture.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
#include "RHI/DX11/DX11Texture.h"
#include "RHI/DX11/DX11Sampler.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RHI/DX12/DX12Texture.h"
#include "Graphics.h"
#include "Console/Logger.h"

using namespace DirectX;
using namespace Microsoft::WRL;

// デストラクタの実体（unique_ptr の不完全型解消のため）
ShadowMap::~ShadowMap() = default;

ShadowMap::ShadowMap(IResourceFactory* factory)
{
    // 2. シェーダー & レイアウト生成 (RHI)
    vertexShader = factory->CreateShader(ShaderType::Vertex, "Data/Shader/ShadowMapVS.cso");
    instancedVertexShader = factory->CreateShader(ShaderType::Vertex, "Data/Shader/ShadowMapInstancedVS.cso");

    InputLayoutElement layoutElements[] = {
        { "POSITION",     0, TextureFormat::R32G32B32_FLOAT,    0, kAppendAlignedElement },
        { "BONE_WEIGHTS", 0, TextureFormat::R32G32B32A32_FLOAT, 0, kAppendAlignedElement },
        { "BONE_INDICES", 0, TextureFormat::R32G32B32A32_UINT,  0, kAppendAlignedElement },
        { "TEXCOORD",     0, TextureFormat::R32G32_FLOAT,       0, kAppendAlignedElement },
        { "NORMAL",       0, TextureFormat::R32G32B32_FLOAT,    0, kAppendAlignedElement },
        { "TANGENT",      0, TextureFormat::R32G32B32_FLOAT,    0, kAppendAlignedElement },
    };
    InputLayoutDesc layoutDesc{ layoutElements, _countof(layoutElements) };
    inputLayout = factory->CreateInputLayout(layoutDesc, vertexShader.get());

    // インスタンシング用入力レイアウト (slot1 にインスタンスデータ)
    InputLayoutElement instancedLayoutElements[] = {
        { "POSITION",     0, TextureFormat::R32G32B32_FLOAT,    0, kAppendAlignedElement },
        { "BONE_WEIGHTS", 0, TextureFormat::R32G32B32A32_FLOAT, 0, kAppendAlignedElement },
        { "BONE_INDICES", 0, TextureFormat::R32G32B32A32_UINT,  0, kAppendAlignedElement },
        { "TEXCOORD",     0, TextureFormat::R32G32_FLOAT,       0, kAppendAlignedElement },
        { "NORMAL",       0, TextureFormat::R32G32B32_FLOAT,    0, kAppendAlignedElement },
        { "TANGENT",      0, TextureFormat::R32G32B32_FLOAT,    0, kAppendAlignedElement },
        { "INSTANCE_WORLD", 0, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1 },
        { "INSTANCE_WORLD", 1, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1 },
        { "INSTANCE_WORLD", 2, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1 },
        { "INSTANCE_WORLD", 3, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1 },
        { "INSTANCE_PREV_WORLD", 0, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1 },
        { "INSTANCE_PREV_WORLD", 1, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1 },
        { "INSTANCE_PREV_WORLD", 2, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1 },
        { "INSTANCE_PREV_WORLD", 3, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1 },
    };
    InputLayoutDesc instancedLayoutDesc{ instancedLayoutElements, _countof(instancedLayoutElements) };
    instancedInputLayout = factory->CreateInputLayout(instancedLayoutDesc, instancedVertexShader.get());

    // 3. 定数バッファ生成 (RHI)
    sceneConstantBuffer = factory->CreateBuffer(sizeof(CbScene), BufferType::Constant);
    skeletonConstantBuffer = factory->CreateBuffer(sizeof(CbSkeleton), BufferType::Constant);

    // 4. カスケード用テクスチャ配列の生成
    const bool isDX12 = (Graphics::Instance().GetAPI() == GraphicsAPI::DX12);

    if (isDX12) {
        // DX12: DX12Texture の配列コンストラクタを使用
        auto* dx12Dev = Graphics::Instance().GetDX12Device();

        auto arrayTex = std::make_shared<DX12Texture>(
            dx12Dev, textureSize, textureSize,
            TextureFormat::R32_TYPELESS, CASCADE_COUNT,
            TextureBindFlags::DepthStencil | TextureBindFlags::ShaderResource);
        m_shadowTexture = arrayTex;

        m_cascadeTextures.resize(CASCADE_COUNT);
        for (int i = 0; i < CASCADE_COUNT; ++i) {
            m_cascadeTextures[i] = std::make_shared<DX12Texture>(
                dx12Dev, arrayTex->GetNativeResourceComPtr(),
                textureSize, textureSize, static_cast<uint32_t>(i));
        }
        // DX12: サンプラーはスタティックサンプラー s1 (ShadowCompare) で対応済み
    } else {
        // DX11: 既存のネイティブコード
        ID3D11Device* device = Graphics::Instance().GetDevice();

        D3D11_TEXTURE2D_DESC texture2dDesc{};
        texture2dDesc.Width = textureSize;
        texture2dDesc.Height = textureSize;
        texture2dDesc.MipLevels = 1;
        texture2dDesc.ArraySize = CASCADE_COUNT;
        texture2dDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        texture2dDesc.SampleDesc.Count = 1;
        texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
        texture2dDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        ComPtr<ID3D11Texture2D> texture2d;
        HRESULT hr = device->CreateTexture2D(&texture2dDesc, 0, texture2d.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.ArraySize = 1;
        dsvDesc.Texture2DArray.MipSlice = 0;

        m_cascadeTextures.resize(CASCADE_COUNT);
        for (int i = 0; i < CASCADE_COUNT; ++i)
        {
            ComPtr<ID3D11DepthStencilView> rawDSV;
            dsvDesc.Texture2DArray.FirstArraySlice = i;
            hr = device->CreateDepthStencilView(texture2d.Get(), &dsvDesc, rawDSV.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
            m_cascadeTextures[i] = std::make_shared<DX11Texture>(rawDSV.Get(), textureSize, textureSize);
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.ArraySize = CASCADE_COUNT;
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.MipLevels = 1;

        ComPtr<ID3D11ShaderResourceView> rawSRV;
        hr = device->CreateShaderResourceView(texture2d.Get(), &srvDesc, rawSRV.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        m_shadowTexture = std::make_shared<DX11Texture>(rawSRV.Get());

        // 5. サンプラー生成 (DX11 のみ)
        D3D11_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.BorderColor[0] = samplerDesc.BorderColor[1] = samplerDesc.BorderColor[2] = samplerDesc.BorderColor[3] = 1.0f;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        samplerState = std::make_unique<DX11Sampler>(device, samplerDesc);
    }

    // 6. 影描画専用 PSO の構築
    PipelineStateDesc psoDesc{};
    psoDesc.vertexShader = vertexShader.get();
    psoDesc.pixelShader = nullptr; // 深度のみ
    psoDesc.inputLayout = inputLayout.get();
    psoDesc.primitiveTopology = PrimitiveTopology::TriangleList;

    psoDesc.numRenderTargets = 0;
    psoDesc.dsvFormat = TextureFormat::D32_FLOAT;

    auto* rs = Graphics::Instance().GetRenderState();
    psoDesc.rasterizerState = rs->GetRasterizerState(RasterizerState::SolidCullNone);
    psoDesc.depthStencilState = rs->GetDepthStencilState(DepthState::TestAndWrite);
    psoDesc.blendState = rs->GetBlendState(BlendState::Opaque);

    m_pso = factory->CreatePipelineState(psoDesc);

    PipelineStateDesc instancedPsoDesc = psoDesc;
    instancedPsoDesc.vertexShader = instancedVertexShader.get();
    instancedPsoDesc.inputLayout = instancedInputLayout.get();
    m_instancedPso = factory->CreatePipelineState(instancedPsoDesc);
}

void ShadowMap::UpdateCascades(const RenderContext& rc)
{
    float camNear = rc.nearZ;
    float camFar = rc.farZ;
    float clipRange = camFar - camNear;
    float prevSplitDist = camNear;

    for (int i = 0; i < CASCADE_COUNT; ++i)
    {
        float splitDist = camNear + (cascadeSplits[i] * clipRange);
        cascadeEndClips[i] = splitDist;
        XMMATRIX mat = CalcCascadeMatrix(rc, prevSplitDist, splitDist);
        XMStoreFloat4x4(&shadowMatrices[i], mat);
        prevSplitDist = splitDist;
    }
}

void ShadowMap::BeginCascade(const RenderContext& rc, int cascadeIndex)
{
    if (cascadeIndex < 0 || cascadeIndex >= CASCADE_COUNT) return;

    // 1. 影用SRVを解除 (RHI)
    rc.commandList->PSSetTexture(4, nullptr);
    if (m_shadowTexture) {
        static bool s_loggedShadowResource = false;
        if (!s_loggedShadowResource && Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
            if (auto* dx12Shadow = dynamic_cast<DX12Texture*>(m_shadowTexture.get())) {
                LOG_INFO("[ShadowMap] resource=%p", dx12Shadow->GetNativeResource());
                s_loggedShadowResource = true;
            }
        }
        rc.commandList->TransitionBarrier(m_shadowTexture.get(), ResourceState::DepthWrite);
    }

    static uint32_t s_shadowBeginLogCount = 0;
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12 && s_shadowBeginLogCount < 8) {
        auto* dx12Shadow = dynamic_cast<DX12Texture*>(m_shadowTexture.get());
        auto* dx12Slice = dynamic_cast<DX12Texture*>(m_cascadeTextures[cascadeIndex].get());
        LOG_INFO(
            "[ShadowMapBegin] frame=%u cascade=%d fullState=%d sliceState=%d fullRes=%p sliceRes=%p",
            s_shadowBeginLogCount,
            cascadeIndex,
            dx12Shadow ? static_cast<int>(dx12Shadow->GetCurrentState()) : -1,
            dx12Slice ? static_cast<int>(dx12Slice->GetCurrentState()) : -1,
            dx12Shadow ? dx12Shadow->GetNativeResource() : nullptr,
            dx12Slice ? dx12Slice->GetNativeResource() : nullptr);
        if (cascadeIndex == CASCADE_COUNT - 1) {
            ++s_shadowBeginLogCount;
        }
    }

    // 2. 現在の状態を「保存」する
    if (cascadeIndex == 0)
    {
        m_cachedViewport = rc.mainViewport;
        m_cachedRT = rc.mainRenderTarget;
        m_cachedDS = rc.mainDepthStencil;
    }

    // ====================================================
    // ★ 修正：MinDepth(0.0f) と MaxDepth(1.0f) を明示的に指定する！
    // ====================================================
    rc.commandList->SetViewport(RhiViewport(0.0f, 0.0f, (float)textureSize, (float)textureSize, 0.0f, 1.0f));

    rc.commandList->SetRenderTarget(nullptr, m_cascadeTextures[cascadeIndex].get());
    rc.commandList->ClearDepthStencil(m_cascadeTextures[cascadeIndex].get(), 1.0f, 0);

    // 4. PSO と定数バッファのバインド
    rc.commandList->SetPipelineState(m_pso.get());

    CbScene cbScene{};
    cbScene.lightViewProjection = shadowMatrices[cascadeIndex];
    if (auto* dx12Cmd = dynamic_cast<DX12CommandList*>(rc.commandList)) {
        dx12Cmd->VSSetDynamicConstantBuffer(0, &cbScene, sizeof(cbScene));
    }
    else {
        rc.commandList->UpdateBuffer(sceneConstantBuffer.get(), &cbScene, sizeof(cbScene));
        rc.commandList->VSSetConstantBuffer(0, sceneConstantBuffer.get());
    }
}

void ShadowMap::End(const RenderContext& rc)
{
    // 保存しておいた RhiViewport 構造体をそのまま渡す
    rc.commandList->SetViewport(m_cachedViewport);

    // 保存しておいたメインのレンダーターゲットと深度バッファを戻す
    rc.commandList->SetRenderTarget(m_cachedRT, m_cachedDS);

    // キャッシュしたポインタをクリア
    m_cachedRT = nullptr;
    m_cachedDS = nullptr;

    static uint32_t s_shadowEndLogCount = 0;
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12 && s_shadowEndLogCount < 8) {
        auto* dx12Shadow = dynamic_cast<DX12Texture*>(m_shadowTexture.get());
        LOG_INFO(
            "[ShadowMapEnd] frame=%u fullState=%d fullRes=%p",
            s_shadowEndLogCount,
            dx12Shadow ? static_cast<int>(dx12Shadow->GetCurrentState()) : -1,
            dx12Shadow ? dx12Shadow->GetNativeResource() : nullptr);
        ++s_shadowEndLogCount;
    }
}

void ShadowMap::Draw(const RenderContext& rc, const ModelResource* modelResource, const DirectX::XMFLOAT4X4& worldMatrix)
{
    if (!modelResource) return;
    XMMATRIX actorWorld = XMLoadFloat4x4(&worldMatrix);
    for (int meshIndex = 0; meshIndex < modelResource->GetMeshCount(); ++meshIndex)
    {
        const ModelResource::MeshResource* mesh = modelResource->GetMeshResource(meshIndex);
        if (!mesh) continue;
        if (!modelResource->BindMeshBuffers(rc.commandList, meshIndex)) continue;

        CbSkeleton cbSkeleton{};
        if (!mesh->bones.empty())
        {
            for (size_t i = 0; i < mesh->bones.size(); ++i)
            {
                const auto& bone = mesh->bones[i];
                XMMATRIX nodeTransform = XMLoadFloat4x4(&bone.worldTransform);
                XMMATRIX offsetTransform = XMLoadFloat4x4(&bone.offsetTransform);
                XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], offsetTransform * nodeTransform * actorWorld);
            }
        }
        else
        {
            XMMATRIX nodeTransform = XMLoadFloat4x4(&mesh->nodeWorldTransform);
            // Static meshes still need the entity transform applied here.
            XMStoreFloat4x4(&cbSkeleton.boneTransforms[0], nodeTransform * actorWorld);
        }

        if (auto* dx12Cmd = dynamic_cast<DX12CommandList*>(rc.commandList)) {
            dx12Cmd->VSSetDynamicConstantBuffer(6, &cbSkeleton, sizeof(cbSkeleton));
        }
        else {
            rc.commandList->VSSetConstantBuffer(6, skeletonConstantBuffer.get());
            rc.commandList->UpdateBuffer(skeletonConstantBuffer.get(), &cbSkeleton, sizeof(cbSkeleton));
        }
        rc.commandList->DrawIndexed(modelResource->GetMeshIndexCount(meshIndex), 0, 0);
    }
}

void ShadowMap::DrawInstanced(const RenderContext& rc, const ModelResource* modelResource,
    int meshIndex,
    IBuffer* instanceBuffer, uint32_t instanceStride, uint32_t firstInstance, uint32_t instanceCount,
    IBuffer* argumentBuffer, uint32_t argumentOffsetBytes)
{
    if (!modelResource || !instanceBuffer || instanceCount == 0) return;

    const ModelResource::MeshResource* mesh = modelResource->GetMeshResource(meshIndex);
    if (!mesh || !mesh->bones.empty()) {
        return;
    }
    if (!modelResource->BindMeshBuffers(rc.commandList, meshIndex)) {
        return;
    }

    rc.commandList->SetPipelineState(m_instancedPso ? m_instancedPso.get() : m_pso.get());
    rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);
    rc.commandList->SetVertexBuffer(1, instanceBuffer, instanceStride, 0);

    if (argumentBuffer) {
        rc.commandList->ExecuteIndexedIndirect(argumentBuffer, argumentOffsetBytes);
    } else {
        rc.commandList->DrawIndexedInstanced(modelResource->GetMeshIndexCount(meshIndex), instanceCount, 0, 0, firstInstance);
    }
}

void ShadowMap::DrawInstancedMulti(const RenderContext& rc, const ModelResource* modelResource,
    int meshIndex,
    IBuffer* instanceBuffer, uint32_t instanceStride,
    IBuffer* argumentBuffer, uint32_t argumentOffsetBytes,
    uint32_t commandCount, uint32_t commandStride)
{
    if (!modelResource || !instanceBuffer || !argumentBuffer || commandCount == 0) return;

    const ModelResource::MeshResource* mesh = modelResource->GetMeshResource(meshIndex);
    if (!mesh || !mesh->bones.empty()) {
        return;
    }
    if (!modelResource->BindMeshBuffers(rc.commandList, meshIndex)) {
        return;
    }

    rc.commandList->SetPipelineState(m_instancedPso ? m_instancedPso.get() : m_pso.get());
    rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);
    rc.commandList->SetVertexBuffer(1, instanceBuffer, instanceStride, 0);
    rc.commandList->ExecuteIndexedIndirectMulti(argumentBuffer, argumentOffsetBytes, commandCount, commandStride);
}

// ───── 数学ヘルパー ─────────────────────────────────────────────

std::array<XMVECTOR, 8> ShadowMap::GetFrustumCorners(float fov, float aspect, float nearZ, float farZ, const XMFLOAT4X4& viewMat)
{
    std::array<XMVECTOR, 8> corners = {};
    XMMATRIX proj = XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);
    XMMATRIX view = XMLoadFloat4x4(&viewMat);
    XMMATRIX invViewProj = XMMatrixInverse(nullptr, view * proj);

    XMVECTOR ndcCorners[8] = {
        XMVectorSet(-1.0f,  1.0f, 0.0f, 1.0f), XMVectorSet(1.0f,  1.0f, 0.0f, 1.0f),
        XMVectorSet(1.0f, -1.0f, 0.0f, 1.0f), XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f),
        XMVectorSet(-1.0f,  1.0f, 1.0f, 1.0f), XMVectorSet(1.0f,  1.0f, 1.0f, 1.0f),
        XMVectorSet(1.0f, -1.0f, 1.0f, 1.0f), XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f),
    };

    for (int i = 0; i < 8; ++i) corners[i] = XMVector3TransformCoord(ndcCorners[i], invViewProj);
    return corners;
}

XMMATRIX ShadowMap::CalcCascadeMatrix(const RenderContext& rc, float nearZ, float farZ)
{
    auto corners = GetFrustumCorners(rc.fovY, rc.aspect, nearZ, farZ, rc.viewMatrix);
    XMVECTOR center = XMVectorZero();
    for (const auto& v : corners) center += v;
    center /= 8.0f;

    XMVECTOR lightDir = XMLoadFloat3(&rc.directionalLight.direction);
    if (XMVectorGetX(XMVector3LengthSq(lightDir)) < 0.0001f) lightDir = XMVectorSet(0.1f, -1.0f, 0.1f, 0);
    lightDir = XMVector3Normalize(lightDir);

    XMVECTOR lightPos = center - lightDir * 5000.0f;
    XMVECTOR up = (fabsf(XMVectorGetY(lightDir)) > 0.99f) ? XMVectorSet(0, 0, 1, 0) : XMVectorSet(0, 1, 0, 0);
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, center, up);

    XMVECTOR minBox = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
    XMVECTOR maxBox = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);

    for (const auto& v : corners)
    {
        XMVECTOR lv = XMVector3Transform(v, lightView);
        minBox = XMVectorMin(minBox, lv);
        maxBox = XMVectorMax(maxBox, lv);
    }

    float w = XMVectorGetX(maxBox) - XMVectorGetX(minBox);
    float h = XMVectorGetY(maxBox) - XMVectorGetY(minBox);
    float worldUnitsPerTexel = (w > h ? w : h) / static_cast<float>(textureSize);

    XMVECTOR vWorldUnits = XMVectorSet(worldUnitsPerTexel, worldUnitsPerTexel, worldUnitsPerTexel, 0.0f);
    minBox = XMVectorFloor(XMVectorDivide(minBox, vWorldUnits)) * vWorldUnits;
    maxBox = XMVectorFloor(XMVectorDivide(maxBox, vWorldUnits)) * vWorldUnits;

    float minX = XMVectorGetX(minBox);
    float maxX = XMVectorGetX(maxBox);
    float minY = XMVectorGetY(minBox);
    float maxY = XMVectorGetY(maxBox);
    float minZ = XMVectorGetZ(minBox) - 5000.0f;
    float maxZ = XMVectorGetZ(maxBox);

    static uint32_t s_shadowCascadeRangeLogCount = 0;
    if (s_shadowCascadeRangeLogCount < 8) {
        LOG_INFO(
            "[ShadowCascadeRange] frame=%u near=%.3f far=%.3f depthRange=%.3f width=%.3f height=%.3f minZBox=%.3f maxZBox=%.3f",
            s_shadowCascadeRangeLogCount,
            minZ,
            maxZ,
            maxZ - minZ,
            maxX - minX,
            maxY - minY,
            XMVectorGetZ(minBox),
            XMVectorGetZ(maxBox));
        ++s_shadowCascadeRangeLogCount;
    }

    return lightView * XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);
}

#pragma once

#include <vector>
#include <DirectXMath.h>
#include <memory>
#include "RenderContext/RenderContext.h"

class IShader;
class IBuffer;
class IInputLayout;
class IResourceFactory;

class Gizmos
{
public:
    // box / sphere / cylinder / capsule の wire mesh と shader を作成する。
    Gizmos(IResourceFactory* factory);

    ~Gizmos();

    // box gizmo の描画 instance を追加する。
    void DrawBox(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT4& color);
    // sphere gizmo の描画 instance を追加する。
    void DrawSphere(const DirectX::XMFLOAT3& position, float radius, const DirectX::XMFLOAT4& color);
    // cylinder gizmo の描画 instance を追加する。
    void DrawCylinder(const DirectX::XMFLOAT3& position, float radius, float height, const DirectX::XMFLOAT4& color);
    // capsule gizmo の描画 instance を追加する。
    void DrawCapsule(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, float radius, float height, const DirectX::XMFLOAT4& color);

    // 追加済み instance を描画し、描画後に instance list を空にする。
    void Render(const RenderContext& rc);

private:
    struct Mesh
    {
        std::unique_ptr<IBuffer> vertexBuffer; // wire mesh の頂点 buffer。
        UINT                     vertexCount;  // 描画する頂点数。
    };

    struct CbMesh
    {
        DirectX::XMFLOAT4X4 worldViewProjection; // instance ごとの WVP。
        DirectX::XMFLOAT4 color;                 // instance color。
    };

    struct Instance
    {
        Mesh* mesh; // 使用する共有 mesh。非所有。
        DirectX::XMFLOAT4X4 worldTransform; // world transform。
        DirectX::XMFLOAT4 color; // 表示色。
    };

    // position list から line mesh buffer を作成する。
    void CreateMesh(IResourceFactory* factory, const std::vector<DirectX::XMFLOAT3>& vertices, Mesh& mesh);
    // box wire mesh を作成する。
    void CreateBoxMesh(IResourceFactory* factory, float width, float height, float depth);
    // sphere wire mesh を作成する。
    void CreateSphereMesh(IResourceFactory* factory, float radius, int subdivisions);
    // cylinder wire mesh を作成する。
    void CreateCylinderMesh(IResourceFactory* factory, float radius, float height, int subdivision);
    // capsule wire mesh を作成する。
    void CreateCapsuleMesh(IResourceFactory* factory, float radius, float height, int subdivision);

private:
    Mesh                  boxMesh;      // box 用共有 mesh。
    Mesh                  sphereMesh;   // sphere 用共有 mesh。
    Mesh                  cylinderMesh; // cylinder 用共有 mesh。
    Mesh                  capsuleMesh;  // capsule 用共有 mesh。
    std::vector<Instance> instances;    // 今 frame 描画する gizmo instance。

    std::unique_ptr<IShader>       vertexShader;   // gizmo VS。
    std::unique_ptr<IShader>       pixelShader;    // gizmo PS。
    std::unique_ptr<IInputLayout>  inputLayout;    // position only layout。
    std::unique_ptr<IBuffer>       constantBuffer; // CbMesh 用 constant buffer。
};

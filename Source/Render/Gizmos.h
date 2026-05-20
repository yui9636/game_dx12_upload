#pragma once

#include <vector>
#include <DirectXMath.h>
#include <memory>
#include "RenderContext/RenderContext.h"
// IShader はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class IShader;
class IBuffer;
class IInputLayout;
class IResourceFactory;

class Gizmos
{
public:
    Gizmos(IResourceFactory* factory);

    ~Gizmos();

    void DrawBox(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT4& color);
    void DrawSphere(const DirectX::XMFLOAT3& position, float radius, const DirectX::XMFLOAT4& color);
    void DrawCylinder(const DirectX::XMFLOAT3& position, float radius, float height, const DirectX::XMFLOAT4& color);
    void DrawCapsule(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, float radius, float height, const DirectX::XMFLOAT4& color);

    void Render(const RenderContext& rc);

private:
    struct Mesh
    {
        std::unique_ptr<IBuffer> vertexBuffer;
        UINT                     vertexCount;
    };

    struct CbMesh
    {
        DirectX::XMFLOAT4X4 worldViewProjection;
        DirectX::XMFLOAT4 color;
    };

    struct Instance
    {
        Mesh* mesh;
        DirectX::XMFLOAT4X4 worldTransform;
        DirectX::XMFLOAT4 color;
    };

    void CreateMesh(IResourceFactory* factory, const std::vector<DirectX::XMFLOAT3>& vertices, Mesh& mesh);
    void CreateBoxMesh(IResourceFactory* factory, float width, float height, float depth);
    void CreateSphereMesh(IResourceFactory* factory, float radius, int subdivisions);
    void CreateCylinderMesh(IResourceFactory* factory, float radius, float height, int subdivision);
    void CreateCapsuleMesh(IResourceFactory* factory, float radius, float height, int subdivision);

private:
    Mesh                  boxMesh;
    Mesh                  sphereMesh;
    Mesh                  cylinderMesh;
    Mesh                  capsuleMesh;
    std::vector<Instance> instances;

    std::unique_ptr<IShader>       vertexShader;
    std::unique_ptr<IShader>       pixelShader;
    std::unique_ptr<IInputLayout>  inputLayout;
    std::unique_ptr<IBuffer>       constantBuffer;
};

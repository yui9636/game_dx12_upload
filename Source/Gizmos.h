//#pragma once
//#pragma once
//
//#include<vector>
//#include<wrl.h>
//#include<d3d11.h>
//#include<DirectXMath.h>
//#include"RenderContext/RenderContext.h"
//
//class Gizmos
//{
//public:
//	Gizmos(ID3D11Device* device);
//	~Gizmos() {}
//
//	//���`��
//	void DrawBox(
//		const DirectX::XMFLOAT3& position,
//		const DirectX::XMFLOAT3& angle,
//		const DirectX::XMFLOAT3& size,
//		const DirectX::XMFLOAT4& color);
//
//	//���`��
//	void DrawSphere(
//		const DirectX::XMFLOAT3& position,
//		float radius,
//		const DirectX::XMFLOAT4& color);
//
//	void DrawCylinder(const DirectX::XMFLOAT3& position,
//		float radius,
//		float height,
//		const DirectX::XMFLOAT4& color);
//
//	void DrawCapsule(const DirectX::XMFLOAT3& position,
//		const DirectX::XMFLOAT3& angle,
//		float radius,
//		float height,
//		const DirectX::XMFLOAT4& color);
//
//
//	//�`����s
//	void Render(const RenderContext& rc);
//
//
//private:
//	struct Mesh
//	{
//		Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
//		UINT								 vertexCount;
//	};
//
//
//	struct CbMesh
//	{
//		DirectX::XMFLOAT4X4 worldViewProjection;
//		DirectX::XMFLOAT4 color;
//	};
//
//
//
//	struct Instance
//	{
//		Mesh* mesh;
//		DirectX::XMFLOAT4X4 worldTransform;
//		DirectX::XMFLOAT4 color;
//	};
//
//	//���b�V������
//	void CreateMesh(ID3D11Device* device, const std::vector<DirectX::XMFLOAT3>& vertices, Mesh& mesh);
//
//	//�����b�V������
//	void CreateBoxMesh(ID3D11Device* device, float width, float height, float depth);
//
//	//�����b�V������
//	void CreateSphereMesh(ID3D11Device* device, float radius, int subdivisions);
//
//	//�~�����b�V������
//	void CreateCylinderMesh(ID3D11Device* device, float radius, float height, int subdivision);
//
//	void CreateCapsuleMesh(ID3D11Device* device, float radius, float height, int subdivision);
//
//private:
//	Mesh					boxMesh;
//	Mesh                    sphereMesh;
//	Mesh   cylinderMesh;
//	Mesh   capsuleMesh;
//	std::vector<Instance>	instances;
//
//	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
//	Microsoft::WRL::ComPtr<ID3D11PixelShader>pixelShader;
//	Microsoft::WRL::ComPtr<ID3D11InputLayout>inputLayout;
//	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
//
//};
//
#pragma once

#include <vector>
#include <DirectXMath.h>
#include <memory> // �� �ǉ�
#include "RenderContext/RenderContext.h"

// �� �ǉ�: �O���錾
class IShader;
class IBuffer;
class IInputLayout;
class IResourceFactory;

class Gizmos
{
public:
    Gizmos(IResourceFactory* factory);

    // �� �C���F= default; �������Đ錾�݂̂ɂ���I(unique_ptr�̕s���S�Ȍ^�G���[���)
    ~Gizmos();

    void DrawBox(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT4& color);
    void DrawSphere(const DirectX::XMFLOAT3& position, float radius, const DirectX::XMFLOAT4& color);
    void DrawCylinder(const DirectX::XMFLOAT3& position, float radius, float height, const DirectX::XMFLOAT4& color);
    void DrawCapsule(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, float radius, float height, const DirectX::XMFLOAT4& color);

    void Render(const RenderContext& rc);

private:
    struct Mesh
    {
        // �� �ύX�FIBuffer��unique_ptr�ɕύX
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

    // �� �ύX�F�S�� RHI �̃C���^�[�t�F�[�X�ɕύX�I
    std::unique_ptr<IShader>       vertexShader;
    std::unique_ptr<IShader>       pixelShader;
    std::unique_ptr<IInputLayout>  inputLayout;
    std::unique_ptr<IBuffer>       constantBuffer;
};

#pragma once

#include <memory>
#include <vector>
#include <DirectXMath.h>
#include<wrl.h>
#include<d3d11.h>
#include<string>
#include "Easing.h"
#include <DirectXCollision.h>
#include <Collision\Collision.h>

class ITexture;
class IBuffer;
class ICommandList;


// モデル
class Model
{
public:
	Model(const char* filename, float scaling = 1.0f);

	~Model();

	struct Node
	{
		std::string name;
		std::vector<Node*>children;
		int         parentIndex;
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 rotation;
		DirectX::XMFLOAT3 scale;

		Node* parent = nullptr;
		DirectX::XMFLOAT4X4 localTransform;
		DirectX::XMFLOAT4X4 globalTransform;
		DirectX::XMFLOAT4X4 worldTransform;

		template<class Archive>
		void serialize(Archive& archive);
	};

	enum class AlphaMode
	{
		Opaque,
		Mask,
		Blend
	};


	struct Material
	{
		std::string				name;
		std::string             diffuseTextureFileName;
		std::string				normalTextureFileName;
		std::string				metallicTextureFileName;
		std::string				roughnessTextureFileName;
		std::string				albedoTextureFileName;
		std::string				occlusionTextureFileName;
		std::string             emissiveTextureFileName;

		float                   occlusionStrength; 
		float					metallicFactor = 0.0f;
		float					roughnessFactor = 0.0f;

		float				alphaCutoff = 0.5f;
		AlphaMode			alphaMode = AlphaMode::Opaque;


		DirectX::XMFLOAT4       color = { 1,1,1,1 };
		std::shared_ptr<ITexture> diffuseMap;
		std::shared_ptr<ITexture> normalMap;
		std::shared_ptr<ITexture> metallicMap;
		std::shared_ptr<ITexture> roughnessMap;
		std::shared_ptr<ITexture> albedoMap;
		std::shared_ptr<ITexture> occlusionMap;
		std::shared_ptr<ITexture> emissiveMap;


		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Vertex
	{
		DirectX::XMFLOAT3 position = { 0,0,0 };
		DirectX::XMFLOAT4 boneWeight = { 1,0,0,0 };
		DirectX::XMUINT4 boneIndex = { 0,0,0,0 };
		DirectX::XMFLOAT2 texcoord = { 0,0 };
		DirectX::XMFLOAT3 normal = { 0,0,0 };
		DirectX::XMFLOAT3 tangent = { 0,0,0 };
		DirectX::XMFLOAT4 color = { 1,1,1,1 };

		template<class Archive>
		void serialize(Archive& archive);
	};
	

	struct Bone
	{
		int nodeIndex;
		Node* node = nullptr;
		DirectX::XMFLOAT4X4 offsetTransform;


		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Mesh
	{
		Mesh();
		~Mesh();

		std::vector<Vertex> vertices;
		std::vector<uint32_t>indices;
		std::vector<Bone> bones;
		int materialIndex = 0;
		int nodeIndex = 0;
		Material* material = nullptr;
		Node* node = nullptr;

		std::shared_ptr<IBuffer> vertexBuffer;
		std::shared_ptr<IBuffer> indexBuffer;


		template<class Archive>
		void serialize(Archive& archive);
	};


	struct VectorKeyframe
	{
		float seconds;
		DirectX::XMFLOAT3 value;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct QuaternionKeyframe
	{
		float seconds;
		DirectX::XMFLOAT4 value;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct NodeAnim
	{
		std::vector<VectorKeyframe> positionKeyframes;
		std::vector<QuaternionKeyframe> rotationKeyframes;
		std::vector<VectorKeyframe> scaleKeyframes;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Animation
	{
		std::string name;
		float secondsLength;
		std::vector<NodeAnim> nodeAnims;

		template<class Archive>
		void serialize(Archive& archive);
	};

	//アニメーション再生中か
	bool IsPlayAnimation() const;

	//アニメーション再生
	void PlayAnimation(int index, bool loop, float blendSeconds = 0.2f);

	//アニメーション更新処理
	void UpdateAnimation(float dt);


	//トランスフォーム更新処理
	void UpdateTransform(const DirectX::XMFLOAT4X4& worldTransform);

	// ノードリスト取得
	const std::vector<Node>& GetNodes() const { return nodes; }
	std::vector<Node>& GetNodes() { return nodes; }


	struct NodePose
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 rotation;
		DirectX::XMFLOAT3 scale;
	};



	Node* GetNode(const std::string& name)
	{
		for (auto& node : nodes)
		{
			if (node.name == name)
			{
				return &node;
			}
		}
		return nullptr; // 該当するノードがない場合
	}
	const Node* GetNode(const std::string& name)const
	{
		for (const auto& node : nodes)
		{
			if (node.name == name)
			{
				return &node;
			}
		}
		return nullptr; // 該当するノードがない場合
	}


	//ルートノード取得
	Node* GetRootNode() { return nodes.data(); }

	// ノードインデックス取得
	int GetNodeIndex(const char* name) const;


	void SetAnimationSpeed(const float animationSpeed) { this->animationSpeed = animationSpeed; }
	const float GetAnimationSpeed()const { return animationSpeed; }

	const float GetCurrentAnimSeconds()const { return currentAnimationSeconds; }
	const float GetCurrentAnimLength()const;
	
	void BindBuffers(ICommandList* commandList, int meshIndex = 0);

	// ノード検索
	Node* FindNode(const char* name);

	//メッシュデータ取得
	const std::vector<Mesh>& GetMeshes()const { return meshes; }
	//マテリアルデータ取得
	const std::vector<Material>& GetMaterials()const { return materials; }

	// ★追加: マテリアルデータ取得 (編集用・非const版)
	std::vector<Material>& GetMaterialss() { return materials; }

	//アニメーションデータ取得
	const std::vector<Animation>& GetAnimations()const { return animations; }


	// アニメーションインデックス取得
	int GetAnimationIndex(const char* name) const;


	const DirectX::BoundingBox& GetWorldBounds() const { return bounds; }

	// ★追加: マウスレイと全頂点の当たり判定を行い、最も近い頂点の座標を返す
	// 戻り値: 見つかったら true
	// outVertexPos: 見つかった頂点のワールド座標
	bool GetNearestVertex(
		const DirectX::XMFLOAT3& rayOrigin,
		const DirectX::XMFLOAT3& rayDir,
		DirectX::XMFLOAT3& outVertexPos);

	bool Raycast(
		const DirectX::XMFLOAT3& rayOrigin,
		const DirectX::XMFLOAT3& rayDir,
		RaycastHit& outHit) const;




private:
	//アニメーション計算処理
	void ComputeAnimation(float dt);


	//ブレンディング計算処理
	void ComputeBlending(float dt);

	void ComputeWorldBounds();

	DirectX::BoundingBox bounds;

	//シリアライズ
	void Serialize(const char* filename);

	//デシリアライズ
	void Deserialize(const char* filename);


	struct NodeCache
	{
		DirectX::XMFLOAT3 position = { 0,0,0 };
		DirectX::XMFLOAT4 rotation = { 0,0,0,1 };
		DirectX::XMFLOAT3 scale = { 1,1,1 };
	};
	std::vector<NodeCache> nodeCaches;



	std::vector<Animation>animations;
	std::vector<Mesh> meshes;
	std::vector<Material>materials;
	std::vector<Node>nodes;



	int currentAnimationIndex = -1;
	float currentAnimationSeconds = 0;
	bool animationPlaying = false;
	bool animationLoop = false;

	float oldAnimationSeconds = 0.0f;

	float animationSpeed = 1.0f;

	const char* filename;


	float currentAnimationBlendSeconds = 0.0f;
	float animationBlendSecondsLength = -1.0f;
	bool  animationBlending = false;

	float scaling = 1.0f;


public:
	void ComputeAnimation(int animationIndex, int nodeIndex, float time, NodePose& nodePose) const;
	void ComputeAnimation(int animationIndex, float time, std::vector<NodePose>& nodePose);


	void SetNodePoses(const std::vector<NodePose>& nodePoses);

	void GetNodePoses(std::vector<NodePose>& nodePoses) const;

// =========================================================
// ★追加: メッシュ削減機能用インターフェース
// =========================================================

		// メッシュ情報の取得用構造体 (ポインタで渡す)
	struct MeshData {
		std::vector<Vertex>* vertices;
		std::vector<uint32_t>* indices;
	};

	// サブセット（メッシュパーツ）の数を取得
	int GetSubsetCount() const;

	// 指定したサブセットのメッシュデータを取得
	MeshData GetMeshData(int subsetIndex);

	// インデックスバッファを更新する（削減後のデータを適用）
	// device: バッファ再生成のために必要
	void UpdateMeshIndices(ID3D11Device* device, int subsetIndex, const std::vector<uint32_t>& newIndices);

};

#pragma once
// モデルのノード・メッシュ・マテリアル・アニメーション情報を保持するクラス定義です。

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
// Model の内容を GPU 描画しやすいリソース形式で保持します。
class ModelResource;

// モデルファイルから読み込んだノード・メッシュ・マテリアル・アニメーションを管理します。
class Model
{
public:
	// 指定されたモデルファイルを読み込んで Model を生成します。
	Model(const char* filename, float scaling = 1.0f, bool sourceOnly = false);

	// Model が保持するリソースを破棄します。
	~Model();

	// モデル内の階層ノードと変換情報を表します。
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
		// この構造体を cereal で保存・読み込みするための関数です。
		void serialize(Archive& archive);
	};

	// マテリアルのアルファ描画方式を表します。
	enum class AlphaMode
	{
		Opaque,
		Mask,
		Blend
	};

	// モデルメッシュに適用するマテリアル情報を保持します。
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
		// この構造体を cereal で保存・読み込みするための関数です。
		void serialize(Archive& archive);
	};

	// メッシュを構成する 1 頂点分の情報を保持します。
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
		// この構造体を cereal で保存・読み込みするための関数です。
		void serialize(Archive& archive);
	};

	// スキニングに使うボーンとオフセット行列を保持します。
	struct Bone
	{
		int nodeIndex;
		DirectX::XMFLOAT4X4 offsetTransform;

		template<class Archive>
		// この構造体を cereal で保存・読み込みするための関数です。
		void serialize(Archive& archive);
	};

	// 1 サブセット分の頂点・インデックス・材質・ボーン情報を保持します。
	struct Mesh
	{
		Mesh();
		~Mesh();

		std::vector<Vertex> vertices;
		std::vector<uint32_t>indices;
		std::vector<Bone> bones;
		int materialIndex = 0;
		int nodeIndex = 0;

		template<class Archive>
		// この構造体を cereal で保存・読み込みするための関数です。
		void serialize(Archive& archive);
	};

	// 位置やスケールのキーフレームを表します。
	struct VectorKeyframe
	{
		float seconds;
		DirectX::XMFLOAT3 value;

		template<class Archive>
		// この構造体を cereal で保存・読み込みするための関数です。
		void serialize(Archive& archive);
	};

	// 回転のキーフレームを表します。
	struct QuaternionKeyframe
	{
		float seconds;
		DirectX::XMFLOAT4 value;

		template<class Archive>
		// この構造体を cereal で保存・読み込みするための関数です。
		void serialize(Archive& archive);
	};

	// 1 ノード分の位置・回転・スケールアニメーションを保持します。
	struct NodeAnim
	{
		std::vector<VectorKeyframe> positionKeyframes;
		std::vector<QuaternionKeyframe> rotationKeyframes;
		std::vector<VectorKeyframe> scaleKeyframes;

		template<class Archive>
		// この構造体を cereal で保存・読み込みするための関数です。
		void serialize(Archive& archive);
	};

	// 1 つのアニメーションクリップ全体を保持します。
	struct Animation
	{
		std::string name;
		float secondsLength;
		std::vector<NodeAnim> nodeAnims;

		template<class Archive>
		// この構造体を cereal で保存・読み込みするための関数です。
		void serialize(Archive& archive);
	};

	// アニメーション再生中かどうかを取得します。
	bool IsPlayAnimation() const;

	// 指定したアニメーション番号を再生します。
	void PlayAnimation(int index, bool loop, float blendSeconds = 0.2f);

	// 1 フレーム分のアニメーション更新を行います。
	void UpdateAnimation(float dt);

	// 外部から渡されたワールド行列を基準にノード変換を更新します。
	void UpdateTransform(const DirectX::XMFLOAT4X4& worldTransform);

	// 読み取り専用のノード配列を取得します。
	const std::vector<Node>& GetNodes() const { return nodes; }
	// 編集可能なノード配列を取得します。
	std::vector<Node>& GetNodes() { return nodes; }

	// 外部評価用のノード姿勢を保持します。
	struct NodePose
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 rotation;
		DirectX::XMFLOAT3 scale;
	};

	// 名前から編集可能なノードを取得します。
	Node* GetNode(const std::string& name)
	{
		for (auto& node : nodes)
		{
			if (node.name == name)
			{
				return &node;
			}
		}
		return nullptr;
	}
	// 名前から読み取り専用のノードを取得します。
	const Node* GetNode(const std::string& name)const
	{
		for (const auto& node : nodes)
		{
			if (node.name == name)
			{
				return &node;
			}
		}
		return nullptr;
	}

	// ルートノードを取得します。
	Node* GetRootNode() { return nodes.data(); }

	// 名前からノード番号を取得します。
	int GetNodeIndex(const char* name) const;

	// アニメーション再生速度を設定します。
	void SetAnimationSpeed(const float animationSpeed) { this->animationSpeed = animationSpeed; }
	// アニメーション再生速度を取得します。
	const float GetAnimationSpeed()const { return animationSpeed; }

	// 現在のアニメーション再生時間を取得します。
	const float GetCurrentAnimSeconds()const { return currentAnimationSeconds; }
	// 現在のアニメーション長を取得します。
	const float GetCurrentAnimLength()const;

	// 名前からノードを検索します。
	Node* FindNode(const char* name);

	// 読み取り専用のメッシュ配列を取得します。
	const std::vector<Mesh>& GetMeshes()const { return meshes; }
	// 描画用 ModelResource を取得します。
	std::shared_ptr<ModelResource> GetModelResource() const { return modelResource; }
	float GetScaling() const { return scaling; }
	// 指定メッシュのマテリアル番号を取得します。
	int GetMeshMaterialIndex(int meshIndex) const;
	// 指定メッシュのノード番号を取得します。
	int GetMeshNodeIndex(int meshIndex) const;
	// 指定メッシュ内のボーンが参照するノード番号を取得します。
	int GetMeshBoneNodeIndex(int meshIndex, int boneIndex) const;
	// 読み取り専用のマテリアル配列を取得します。
	const std::vector<Material>& GetMaterials()const { return materials; }

	// 編集可能なマテリアル配列を取得します。
	std::vector<Material>& GetMaterialss() { return materials; }

	// 読み取り専用のアニメーション配列を取得します。
	const std::vector<Animation>& GetAnimations()const { return animations; }

	// 名前からアニメーション番号を取得します。
	int GetAnimationIndex(const char* name) const;

	// モデル全体のワールド境界を取得します。
	const DirectX::BoundingBox& GetWorldBounds() const { return bounds; }

	// 指定位置に最も近い頂点を取得します。
	bool GetNearestVertex(
		const DirectX::XMFLOAT3& rayOrigin,
		const DirectX::XMFLOAT3& rayDir,
		DirectX::XMFLOAT3& outVertexPos);

	// モデルに対するレイキャストを行います。
	bool Raycast(
		const DirectX::XMFLOAT3& rayOrigin,
		const DirectX::XMFLOAT3& rayDir,
		RaycastHit& outHit) const;

private:
	// 再生中アニメーションを計算します。
	void ComputeAnimation(float dt);

	// アニメーションブレンドを計算します。
	void ComputeBlending(float dt);

	// モデル全体のワールド境界を計算します。
	void ComputeWorldBounds();

	DirectX::BoundingBox bounds;

	// モデルデータをファイルへ保存します。
	void Serialize(const char* filename);

	// モデルデータをファイルから読み込みます。
	void Deserialize(const char* filename);

	// ModelResource 関連処理: Rebuild、SceneSync、MeshBufferSync を分けて扱う。
	void RefreshMeshBindingData();
	// 描画用 ModelResource を再構築します。
	void RebuildModelResource();
	// シーン側の変換情報を ModelResource へ同期します。
	void SyncModelResourceSceneData();
	// 指定メッシュの GPU バッファを同期します。
	void SyncModelResourceMeshBuffers(int subsetIndex);

	// ノード名検索を高速化するためのキャッシュ情報です。
	struct NodeCache
	{
		DirectX::XMFLOAT3 position = { 0,0,0 };
		DirectX::XMFLOAT4 rotation = { 0,0,0,1 };
		DirectX::XMFLOAT3 scale = { 1,1,1 };
	};
	// メッシュとノード・ボーンの対応関係を保持します。
	struct MeshBindingData
	{
		int materialIndex = -1;
		int nodeIndex = -1;
		std::vector<int> boneNodeIndices;
	};
	std::vector<NodeCache> nodeCaches;

	std::vector<Animation>animations;
	std::vector<Mesh> meshes;
	std::vector<MeshBindingData> meshBindingData;
	std::vector<Material>materials;
	std::vector<Node>nodes;
	std::shared_ptr<ModelResource> modelResource;

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
	// 指定ノードのアニメーション姿勢を計算します。
	void ComputeAnimation(int animationIndex, int nodeIndex, float time, NodePose& nodePose) const;
	// 指定アニメーションの全ノード姿勢を計算します。
	void ComputeAnimation(int animationIndex, float time, std::vector<NodePose>& nodePose);

	// ノード姿勢配列をモデルへ設定します。
	void SetNodePoses(const std::vector<NodePose>& nodePoses);

	// 現在のノード姿勢配列を取得します。
	void GetNodePoses(std::vector<NodePose>& nodePoses) const;
struct MeshData {
		std::vector<Vertex>* vertices;
		std::vector<uint32_t>* indices;
	};

	// メッシュサブセット数を取得します。
	int GetSubsetCount() const;

	// 指定サブセットのメッシュデータを取得します。
	MeshData GetMeshData(int subsetIndex);

	// 指定サブセットのインデックスバッファを更新します。
	void UpdateMeshIndices(ID3D11Device* device, int subsetIndex, const std::vector<uint32_t>& newIndices);

};

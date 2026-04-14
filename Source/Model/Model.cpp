#include "System/Misc.h"

#include "Model.h"
#include "ModelResource.h"

#include "AssimpImporter.h"

#include<filesystem>

#include"GpuResourceUtils.h"

#include <fstream>

#include <cereal/cereal.hpp>

#include <cereal/archives/binary.hpp>

#include <cereal/types/string.hpp>

#include <cereal/types/vector.hpp>

#include <limits>

#include <cfloat>

#include <cstring>

#include <Collision\Collision.h>

#include "System/ResourceManager.h"

#include "RHI/DX11/DX11Buffer.h"

#include "RHI/ICommandList.h"

#include "RHI/IBuffer.h"

#include "RHI/ITexture.h"

#include "RHI/IResourceFactory.h"

#include <RHI\DX11\DX11Texture.h>

#include "Graphics.h"

#include <DirectXTex.h>

#include "Console/Logger.h"



namespace DirectX



{

	template<class Archive>

	void serialize(Archive& archive, XMUINT4& v)

	{

		archive(

			cereal::make_nvp("x", v.x),

			cereal::make_nvp("y", v.y),

			cereal::make_nvp("z", v.z),

			cereal::make_nvp("w", v.w)

		);

	}



	template<class Archive>

	void serialize(Archive& archive, XMFLOAT2& v)

	{

		archive(

			cereal::make_nvp("x", v.x),

			cereal::make_nvp("y", v.y)

		);

	}



	template<class Archive>

	void serialize(Archive& archive, XMFLOAT3& v)

	{

		archive(

			cereal::make_nvp("x", v.x),

			cereal::make_nvp("y", v.y),

			cereal::make_nvp("z", v.z)

		);

	}



	template<class Archive>

	void serialize(Archive& archive, XMFLOAT4& v)

	{

		archive(

			cereal::make_nvp("x", v.x),

			cereal::make_nvp("y", v.y),

			cereal::make_nvp("z", v.z),

			cereal::make_nvp("w", v.w)

		);

	}



	template<class Archive>

	void serialize(Archive& archive, XMFLOAT4X4& m)

	{

		archive(

			cereal::make_nvp("_11", m._11),

			cereal::make_nvp("_12", m._12),

			cereal::make_nvp("_13", m._13),

			cereal::make_nvp("_14", m._14),

			cereal::make_nvp("_21", m._21),

			cereal::make_nvp("_22", m._22),

			cereal::make_nvp("_23", m._23),

			cereal::make_nvp("_24", m._24),

			cereal::make_nvp("_31", m._31),

			cereal::make_nvp("_32", m._32),

			cereal::make_nvp("_33", m._33),

			cereal::make_nvp("_34", m._34),

			cereal::make_nvp("_41", m._41),

			cereal::make_nvp("_42", m._42),

			cereal::make_nvp("_43", m._43),

			cereal::make_nvp("_44", m._44)

		);

	}



}



template<class Archive>

void Model::Node::serialize(Archive& archive)

{

	archive(

		CEREAL_NVP(name),

		CEREAL_NVP(parentIndex),

		CEREAL_NVP(position),

		CEREAL_NVP(rotation),

		CEREAL_NVP(scale)

	);

}



template<class Archive>

void Model::Material::serialize(Archive& archive)

{

	archive(

		CEREAL_NVP(name),

		CEREAL_NVP(diffuseTextureFileName),

		CEREAL_NVP(normalTextureFileName),

		CEREAL_NVP(metallicTextureFileName),

		CEREAL_NVP(roughnessTextureFileName),

		CEREAL_NVP(albedoTextureFileName),

		CEREAL_NVP(occlusionTextureFileName),

		CEREAL_NVP(emissiveTextureFileName),

		CEREAL_NVP(metallicFactor),

		CEREAL_NVP(roughnessFactor),

		CEREAL_NVP(occlusionStrength),

		CEREAL_NVP(color)

	);

}



template<class Archive>

void Model::Vertex::serialize(Archive& archive)

{

	archive(

		CEREAL_NVP(position),

		CEREAL_NVP(boneWeight),

		CEREAL_NVP(boneIndex),

		CEREAL_NVP(texcoord),

		CEREAL_NVP(normal),

		CEREAL_NVP(tangent)

	);

}



template<class Archive>

void Model::Bone::serialize(Archive& archive)

{

	archive(

		CEREAL_NVP(nodeIndex),

		CEREAL_NVP(offsetTransform)

	);

}



template<class Archive>

void Model::Mesh::serialize(Archive& archive)

{

	archive(

		CEREAL_NVP(vertices),

		CEREAL_NVP(indices),

		CEREAL_NVP(bones),

		CEREAL_NVP(nodeIndex),

		CEREAL_NVP(materialIndex)

	);

}



template<class Archive>

void Model::VectorKeyframe::serialize(Archive& archive)

{

	archive(

		CEREAL_NVP(seconds),

		CEREAL_NVP(value)

	);

}



template<class Archive>

void Model::QuaternionKeyframe::serialize(Archive& archive)

{

	archive(

		CEREAL_NVP(seconds),

		CEREAL_NVP(value)

	);

}



template<class Archive>

void Model::NodeAnim::serialize(Archive& archive)

{

	archive(

		CEREAL_NVP(positionKeyframes),

		CEREAL_NVP(rotationKeyframes),

		CEREAL_NVP(scaleKeyframes)

	);

}



template<class Archive>

void Model::Animation::serialize(Archive& archive)

{

	archive(

		CEREAL_NVP(name),

		CEREAL_NVP(secondsLength),

		CEREAL_NVP(nodeAnims)

	);

}





Model::~Model() = default;



Model::Mesh::Mesh() = default;

Model::Mesh::~Mesh() = default;



Model::Model(const char* filename, float scaling, bool sourceOnly)

	:scaling(scaling)

{

	IResourceFactory* factory = Graphics::Instance().GetResourceFactory();

	ID3D11Device* device = Graphics::Instance().GetDevice(); // DX12 ?? nullptr



	std::filesystem::path sourceFilepath(filename);

	std::filesystem::path dirpath(sourceFilepath.parent_path());

	std::filesystem::path cerealPath(sourceFilepath);



	cerealPath.replace_extension(".cereal");

	bool loadedFromCache = false;
	if (!sourceOnly && std::filesystem::exists(cerealPath))
	{
		Deserialize(cerealPath.string().c_str());
		loadedFromCache = !nodes.empty() || !meshes.empty();
	}

	std::string sourceExt = sourceFilepath.extension().string();
	std::transform(sourceExt.begin(), sourceExt.end(), sourceExt.begin(), ::tolower);
	const bool canImportSource = sourceExt != ".cereal";

	if (!loadedFromCache && canImportSource)
	{
		AssimpImporter importer(filename);

		importer.LoadMaterials(materials);

		importer.LoadNodes(nodes);

		importer.LoadMeshes(meshes, nodes);

		importer.LoadAnimations(animations, nodes);

		// Manual serializer build now owns .cereal output.
	}
	else if (!loadedFromCache)
	{
		LOG_WARN("[Model] Deserialize failed for '%s' and no source fallback is available.", cerealPath.string().c_str());
	}






	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)

	{

		Node& node = nodes.at(nodeIndex);




		node.parent = node.parentIndex >= 0 ? &nodes.at(node.parentIndex) : nullptr;

		if (node.parent != nullptr)

		{

			node.parent->children.emplace_back(&node);

		}

	}






	//for (Material& material : materials)

	//{

	//	if (material.diffuseTextureFileName.empty())

	//	{


	//		HRESULT hr = GpuResourceUtils::CreateDummyTexture(device, 0xFFFFFFFF,

	//										material.diffuseMap.GetAddressOf());

	//		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	//	}

	//	else

	//	{


	//	std::filesystem::path diffuseTexturePath(dirpath / material.diffuseTextureFileName);

	//	HRESULT hr = GpuResourceUtils::LoadTexture(device, diffuseTexturePath.string().c_str(),

	//												material.diffuseMap.GetAddressOf());

	//		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	//	}

	//	if (material.normalTextureFileName.empty())

	//	{


	//		HRESULT hr = GpuResourceUtils::CreateDummyTexture(device, 0xFFFF7F7F,

	//			material.normalMap.GetAddressOf());

	//		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	//	}

	//	else

	//	{


	//		std::filesystem::path texturePath(dirpath / material.normalTextureFileName);

	//		HRESULT hr = GpuResourceUtils::LoadTexture(device, texturePath.string().c_str(),

	//			material.normalMap.GetAddressOf());

	//		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	//	}

	//}


//	for (Material& m : materials)

//	{


//		if (m.albedoTextureFileName.empty())


//


//		auto SafeLoad = [&](const std::string& file,

//			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,

//			uint32_t dummyRGBA = 0xFFFFFFFF)

//			{

//				std::filesystem::path pLocal(file);

//				std::filesystem::path pModel = dirpath / pLocal;

//				std::filesystem::path use;

//

//				if (!file.empty() && std::filesystem::exists(pLocal))      use = pLocal;

//				else if (!file.empty() && std::filesystem::exists(pModel)) use = pModel;

//

//				HRESULT hr;

//				if (!use.empty())

//				{

//					hr = GpuResourceUtils::LoadTexture(device, use.string().c_str(), srv.GetAddressOf());

//				}

//				else

//				{

//					hr = GpuResourceUtils::CreateDummyTexture(device, dummyRGBA, srv.GetAddressOf());

//#ifdef _DEBUG

//					OutputDebugStringA(("? Missing tex, dummy used: " + pModel.string() + "\n").c_str());

//#endif

//				}

//				_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

//			};

//






//		SafeLoad(m.albedoTextureFileName, m.albedoMap);                   



//	}



	for (Material& material : materials)

	{

		if (sourceOnly)
		{
			material.diffuseMap = nullptr;
			material.normalMap = nullptr;
			material.metallicMap = nullptr;
			material.roughnessMap = nullptr;
			material.albedoMap = nullptr;
			material.occlusionMap = nullptr;
			material.emissiveMap = nullptr;
			continue;
		}

		auto& rm = ResourceManager::Instance();




		static bool s_loggedAlbedoLoad = false;

		auto GetSafeTexture = [&](const char* usage, const std::string& texName, uint32_t dummyColor) -> std::shared_ptr<ITexture> {

			std::filesystem::path fullPath;

			bool exists = false;

			if (!texName.empty()) {

				fullPath = dirpath / texName;

				exists = std::filesystem::exists(fullPath);

				auto tex = rm.GetTexture(fullPath.string());

				if (!s_loggedAlbedoLoad && std::strcmp(usage, "albedo") == 0) {

					LOG_INFO("[Model] %s tex='%s' resolved='%s' exists=%d texture=%p",

						usage,

						texName.c_str(),

						fullPath.string().c_str(),

						exists ? 1 : 0,

						tex.get());

					s_loggedAlbedoLoad = true;

				}

				if (tex) return tex;

			}



			if (!device) {

				DirectX::ScratchImage image;

				if (SUCCEEDED(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1))) {

					auto* pixels = reinterpret_cast<uint32_t*>(image.GetPixels());

					if (pixels) {

						pixels[0] = dummyColor;

						auto* resourceFactory = Graphics::Instance().GetResourceFactory();

						if (resourceFactory) {

							auto dummyTexture = resourceFactory->CreateTextureFromMemory(image, image.GetMetadata());

							if (dummyTexture) {

								if (!s_loggedAlbedoLoad && std::strcmp(usage, "albedo") == 0) {

									LOG_WARN("[Model] %s fallback dummy resolved='%s' exists=%d", usage, fullPath.string().c_str(), exists ? 1 : 0);

									s_loggedAlbedoLoad = true;

								}

								return std::shared_ptr<ITexture>(dummyTexture.release());

							}

						}

					}

				}

				if (!s_loggedAlbedoLoad && std::strcmp(usage, "albedo") == 0) {

					LOG_WARN("[Model] %s fallback dummy resolved='%s' exists=%d", usage, fullPath.string().c_str(), exists ? 1 : 0);

					s_loggedAlbedoLoad = true;

				}

				return nullptr;

			}



			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dummySRV;

			GpuResourceUtils::CreateDummyTexture(device, dummyColor, dummySRV.GetAddressOf());

			if (!s_loggedAlbedoLoad && std::strcmp(usage, "albedo") == 0) {

				LOG_WARN("[Model] %s fallback DX11 dummy resolved='%s' exists=%d", usage, fullPath.string().c_str(), exists ? 1 : 0);

				s_loggedAlbedoLoad = true;

			}

			return std::make_shared<DX11Texture>(dummySRV.Get());

		};



		material.diffuseMap = GetSafeTexture("diffuse", material.diffuseTextureFileName, 0xFFFFFFFF);

		material.normalMap = GetSafeTexture("normal", material.normalTextureFileName, 0xFFFF7F7F);

		material.metallicMap = GetSafeTexture("metallic", material.metallicTextureFileName, 0xFF000000);

		material.roughnessMap = GetSafeTexture("roughness", material.roughnessTextureFileName, 0xFFFFFFFF);



		std::string albedoName = material.albedoTextureFileName.empty() ? material.diffuseTextureFileName : material.albedoTextureFileName;

		material.albedoMap = GetSafeTexture("albedo", albedoName, 0xFFFFFFFF);



		material.occlusionMap = GetSafeTexture("occlusion", material.occlusionTextureFileName, 0xFFFFFFFF);

		material.emissiveMap = GetSafeTexture("emissive", material.emissiveTextureFileName, 0xFF000000);

	}




	// import 後に CPU 側の参照表を作り直し、描画用リソースも同期する。
	RefreshMeshBindingData();

	nodeCaches.resize(nodes.size());

	RebuildModelResource();

}



// ノード階層を更新し、最後に ModelResource へ描画用 transform を同期する。

void Model::UpdateTransform(const DirectX::XMFLOAT4X4& worldTransform)

{

	DirectX::XMMATRIX ParentWorldTransform = DirectX::XMLoadFloat4x4(&worldTransform);



// モデル座標系をエンジン座標系へ合わせる補正。

	DirectX::XMMATRIX CoordinateSystemTransform = DirectX::XMMatrixScaling(-scaling, scaling, scaling);



	for (Node& node : nodes)

	{


		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);

		DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));

		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(node.position.x, node.position.y, node.position.z);

		DirectX::XMMATRIX LocalTransform = S * R * T;




		DirectX::XMMATRIX ParentGlobalTransform;

		if (node.parent != nullptr)

		{

			ParentGlobalTransform = DirectX::XMLoadFloat4x4(&node.parent->globalTransform);

		}

		else

		{

			ParentGlobalTransform = DirectX::XMMatrixIdentity();

		}

		DirectX::XMMATRIX GlobalTransform = LocalTransform * ParentGlobalTransform;




		DirectX::XMMATRIX WorldTransform = GlobalTransform * CoordinateSystemTransform * ParentWorldTransform;




		DirectX::XMStoreFloat4x4(&node.localTransform, LocalTransform);

		DirectX::XMStoreFloat4x4(&node.globalTransform, GlobalTransform);

		DirectX::XMStoreFloat4x4(&node.worldTransform, WorldTransform);

	}

	ComputeWorldBounds();
	SyncModelResourceSceneData();

}








void Model::PlayAnimation(int index, bool loop, float blendSeconds)

{

	currentAnimationIndex = index;

	currentAnimationSeconds = 0;

	animationLoop = loop;

	animationPlaying = true;




	animationBlending = blendSeconds > 0.0f;

	currentAnimationBlendSeconds = 0.0f;

	animationBlendSecondsLength = blendSeconds;




	for (size_t i = 0; i < nodes.size(); ++i)

	{

		const Node& src = nodes.at(i);

		NodeCache& dst = nodeCaches.at(i);



		dst.position = src.position;

		dst.rotation = src.rotation;

		dst.scale = src.scale;

	}

}




bool Model::IsPlayAnimation() const

{

	if (currentAnimationIndex < 0) return false;

	if (currentAnimationIndex >=animations.size()) return false;

	return animationPlaying;

}




void Model::UpdateAnimation(float dt)

{

	ComputeAnimation(dt * animationSpeed);

	ComputeBlending(dt * animationSpeed);



}



const float Model::GetCurrentAnimLength() const

{

	if (currentAnimationIndex >= 0 && currentAnimationIndex < animations.size())

	{

		const auto& animation = animations.at(currentAnimationIndex);

		return animation.secondsLength;

	}



	return 0.0f;

}

void Model::RefreshMeshBindingData()

{
	meshBindingData.resize(meshes.size());
	for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		auto& binding = meshBindingData[meshIndex];
		const auto& mesh = meshes[meshIndex];
		binding.materialIndex = mesh.materialIndex;
		binding.nodeIndex = mesh.nodeIndex;
		binding.boneNodeIndices.clear();
		binding.boneNodeIndices.reserve(mesh.bones.size());
		for (const Bone& bone : mesh.bones)
		{
			binding.boneNodeIndices.push_back(bone.nodeIndex);
		}
	}
}

void Model::RebuildModelResource()

{
	IResourceFactory* factory = Graphics::Instance().GetResourceFactory();
	if (!modelResource)
	{
		modelResource = std::make_shared<ModelResource>();
	}
	if (modelResource && factory)
	{
		modelResource->RebuildFromModel(*this, factory);
	}
}

void Model::SyncModelResourceSceneData()

{
	if (modelResource)
	{
		modelResource->SyncSceneDataFromModel(*this);
	}
}

void Model::SyncModelResourceMeshBuffers(int subsetIndex)

{
	if (subsetIndex < 0 || subsetIndex >= meshes.size()) return;
	IResourceFactory* factory = Graphics::Instance().GetResourceFactory();
	if (!modelResource || !factory) return;

	Mesh& mesh = meshes[subsetIndex];
	auto vertexBuffer = std::shared_ptr<IBuffer>(
		factory->CreateBuffer(
			static_cast<uint32_t>(sizeof(Vertex) * mesh.vertices.size()),
			BufferType::Vertex,
			mesh.vertices.empty() ? nullptr : mesh.vertices.data()
		).release()
	);
	auto indexBuffer = std::shared_ptr<IBuffer>(
		factory->CreateBuffer(
			static_cast<uint32_t>(sizeof(uint32_t) * mesh.indices.size()),
			BufferType::Index,
			mesh.indices.empty() ? nullptr : mesh.indices.data()
		).release()
	);
	modelResource->SyncMeshBuffers(
		subsetIndex,
		vertexBuffer,
		indexBuffer,
		static_cast<uint32_t>(sizeof(Vertex)),
		static_cast<uint32_t>(mesh.indices.size()),
		mesh.materialIndex,
		mesh.nodeIndex);
}

int Model::GetMeshMaterialIndex(int meshIndex) const

{
	if (meshIndex < 0 || static_cast<size_t>(meshIndex) >= meshBindingData.size()) return -1;
	return meshBindingData[meshIndex].materialIndex;
}

int Model::GetMeshNodeIndex(int meshIndex) const

{
	if (meshIndex < 0 || static_cast<size_t>(meshIndex) >= meshBindingData.size()) return -1;
	return meshBindingData[meshIndex].nodeIndex;
}

int Model::GetMeshBoneNodeIndex(int meshIndex, int boneIndex) const

{
	if (meshIndex < 0 || static_cast<size_t>(meshIndex) >= meshBindingData.size()) return -1;
	const auto& boneIndices = meshBindingData[meshIndex].boneNodeIndices;
	if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= boneIndices.size()) return -1;
	return boneIndices[boneIndex];
}






void Model::ComputeAnimation(float dt)

{

	if (!IsPlayAnimation()) return;




	const Animation& animation = animations.at(currentAnimationIndex);




	for (size_t nodeIndex = 0; nodeIndex < animation.nodeAnims.size(); ++nodeIndex)

	{

		Node& node = nodes.at(nodeIndex);

		const Model::NodeAnim& nodeAnim = animation.nodeAnims.at(nodeIndex);




		for (size_t index = 0; index < nodeAnim.positionKeyframes.size() - 1; ++index)

		{


			const Model::VectorKeyframe& keyframe0 = nodeAnim.positionKeyframes.at(index);

			const Model::VectorKeyframe& keyframe1 = nodeAnim.positionKeyframes.at(index + 1);



			if (currentAnimationSeconds >= keyframe0.seconds && currentAnimationSeconds < keyframe1.seconds)

			{


				float rate = (currentAnimationSeconds - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);




				DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&keyframe0.value);

				DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&keyframe1.value);

				DirectX::XMVECTOR V = DirectX::XMVectorLerp(V0, V1, rate);




				DirectX::XMStoreFloat3(&node.position, V);

			}

		}




		for (size_t index = 0; index < nodeAnim.rotationKeyframes.size() - 1; ++index)

		{


			const Model::QuaternionKeyframe& keyframe0 = nodeAnim.rotationKeyframes.at(index);

			const Model::QuaternionKeyframe& keyframe1 = nodeAnim.rotationKeyframes.at(index + 1);



			if (currentAnimationSeconds >= keyframe0.seconds && currentAnimationSeconds < keyframe1.seconds)

			{


				float rate = (currentAnimationSeconds - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);




				DirectX::XMVECTOR Q0 = DirectX::XMLoadFloat4(&keyframe0.value);

				DirectX::XMVECTOR Q1 = DirectX::XMLoadFloat4(&keyframe1.value);

				DirectX::XMVECTOR Q = DirectX::XMQuaternionSlerp(Q0, Q1, rate);




				DirectX::XMStoreFloat4(&node.rotation, Q);

			}

		}




		for (size_t index = 0; index < nodeAnim.scaleKeyframes.size() - 1; ++index)

		{


			const Model::VectorKeyframe& keyframe0 = nodeAnim.scaleKeyframes.at(index);

			const Model::VectorKeyframe& keyframe1 = nodeAnim.scaleKeyframes.at(index + 1);



			if (currentAnimationSeconds >= keyframe0.seconds && currentAnimationSeconds < keyframe1.seconds)

			{


				float rate = (currentAnimationSeconds - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);




				DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&keyframe0.value);

				DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&keyframe1.value);

				DirectX::XMVECTOR V = DirectX::XMVectorLerp(V0, V1, rate);




				DirectX::XMStoreFloat3(&node.scale, V);

			}

		}

	}




	currentAnimationSeconds += dt;




	if (currentAnimationSeconds >= animation.secondsLength)

	{

		if (animationLoop)

		{


			currentAnimationSeconds -= animation.secondsLength;

		}

		else

		{


			currentAnimationSeconds = animation.secondsLength;

			animationPlaying = false;

		}

	}

}






void Model::ComputeBlending(float dt)

{

	if (!animationBlending) return;




	float rate = currentAnimationBlendSeconds / animationBlendSecondsLength;




	int count = static_cast<int>(nodes.size());

	for (int i = 0; i < count; ++i)

	{

		const NodeCache& cache = nodeCaches.at(i);

		Node& node = nodes.at(i);



		DirectX::XMVECTOR S0 = DirectX::XMLoadFloat3(&cache.scale);

		DirectX::XMVECTOR S1 = DirectX::XMLoadFloat3(&node.scale);

		DirectX::XMVECTOR R0 = DirectX::XMLoadFloat4(&cache.rotation);

		DirectX::XMVECTOR R1 = DirectX::XMLoadFloat4(&node.rotation);

		DirectX::XMVECTOR T0 = DirectX::XMLoadFloat3(&cache.position);

		DirectX::XMVECTOR T1 = DirectX::XMLoadFloat3(&node.position);



		DirectX::XMVECTOR S = DirectX::XMVectorLerp(S0, S1, rate);

		DirectX::XMVECTOR R = DirectX::XMQuaternionSlerp(R0, R1, rate);

		DirectX::XMVECTOR T = DirectX::XMVectorLerp(T0, T1, rate);



		DirectX::XMStoreFloat3(&node.scale, S);

		DirectX::XMStoreFloat4(&node.rotation, R);

		DirectX::XMStoreFloat3(&node.position, T);

	}




	currentAnimationBlendSeconds += dt;

	if (currentAnimationBlendSeconds >= animationBlendSecondsLength)

	{

		currentAnimationBlendSeconds = animationBlendSecondsLength;

		animationBlending = false;

	}

}




void Model::ComputeWorldBounds()

{


	bounds.Center = { 0, 0, 0 };

	bounds.Extents = { 0, 0, 0 };



	bool firstMerged = false;




	for (const Mesh& mesh : meshes)

	{


		DirectX::BoundingBox meshBounds;




		if (mesh.vertices.empty()) continue;




		DirectX::BoundingBox::CreateFromPoints(

			meshBounds,

			mesh.vertices.size(),

			&mesh.vertices[0].position,

			sizeof(Vertex)

		);




		if (mesh.nodeIndex < 0 || static_cast<size_t>(mesh.nodeIndex) >= nodes.size()) continue;

		DirectX::XMMATRIX worldMat = DirectX::XMLoadFloat4x4(&nodes[mesh.nodeIndex].worldTransform);

		meshBounds.Transform(meshBounds, worldMat);




		if (!firstMerged)

		{

			bounds = meshBounds;

			firstMerged = true;

		}

		else

		{

			DirectX::BoundingBox::CreateMerged(bounds, bounds, meshBounds);

		}

	}



}



bool Model::GetNearestVertex(

	const DirectX::XMFLOAT3& rayOrigin,

	const DirectX::XMFLOAT3& rayDir,

	DirectX::XMFLOAT3& outVertexPos)

{

	using namespace DirectX;




	XMVECTOR origin = XMLoadFloat3(&rayOrigin);

	XMVECTOR dir = XMLoadFloat3(&rayDir);

	float distToBox = 0.0f;




	if (!bounds.Intersects(origin, dir, distToBox))

	{

		return false;

	}




	float minDistanceSqr = FLT_MAX;

	bool found = false;

	XMVECTOR bestVertexPos = XMVectorZero();





	const float threshold = 0.5f;



	for (const Mesh& mesh : meshes)

	{

		if (mesh.nodeIndex < 0 || static_cast<size_t>(mesh.nodeIndex) >= nodes.size()) continue;

		XMMATRIX worldMat = XMLoadFloat4x4(&nodes[mesh.nodeIndex].worldTransform);



		for (const Vertex& v : mesh.vertices)

		{


			XMVECTOR localPos = XMLoadFloat3(&v.position);

			XMVECTOR worldPos = XMVector3Transform(localPos, worldMat);








			XMVECTOR vToP = XMVectorSubtract(worldPos, origin);

			float t = XMVectorGetX(XMVector3Dot(vToP, dir));




			if (t < 0.0f) continue;



			XMVECTOR proj = XMVectorScale(dir, t);

			XMVECTOR perp = XMVectorSubtract(vToP, proj);

			float distSqr = XMVectorGetX(XMVector3Dot(perp, perp));




			if (distSqr < threshold * threshold && distSqr < minDistanceSqr)

			{

				minDistanceSqr = distSqr;

				bestVertexPos = worldPos;

				found = true;

			}

		}

	}



	if (found)

	{

		XMStoreFloat3(&outVertexPos, bestVertexPos);

		return true;

	}



	return false;

}



bool Model::Raycast(

	const DirectX::XMFLOAT3& rayOrigin,

	const DirectX::XMFLOAT3& rayDir,

	RaycastHit& outHit) const

{

	using namespace DirectX;



	XMVECTOR origin = XMLoadFloat3(&rayOrigin);

	XMVECTOR dir = XMLoadFloat3(&rayDir);

	float distToBox = 0.0f;





	if (!bounds.Intersects(origin, dir, distToBox))

	{

		return false;

	}




	bool hasHit = false;

	float minT = FLT_MAX;

	XMVECTOR bestNormal = XMVectorZero();



	for (const Mesh& mesh : meshes)

	{


		if (mesh.nodeIndex < 0 || static_cast<size_t>(mesh.nodeIndex) >= nodes.size()) continue;

		XMMATRIX worldMat = XMLoadFloat4x4(&nodes[mesh.nodeIndex].worldTransform);





		const auto& indices = mesh.indices;

		const auto& vertices = mesh.vertices;




		for (size_t i = 0; i < indices.size(); i += 3)

		{


			uint32_t i0 = indices[i];

			uint32_t i1 = indices[i + 1];

			uint32_t i2 = indices[i + 2];




			XMVECTOR v0 = XMVector3Transform(XMLoadFloat3(&vertices[i0].position), worldMat);

			XMVECTOR v1 = XMVector3Transform(XMLoadFloat3(&vertices[i1].position), worldMat);

			XMVECTOR v2 = XMVector3Transform(XMLoadFloat3(&vertices[i2].position), worldMat);



			float t = 0.0f;


			if (TriangleTests::Intersects(origin, dir, v0, v1, v2, t))

			{


				if (t < minT)

				{

					minT = t;

					hasHit = true;




					XMVECTOR edge1 = v1 - v0;

					XMVECTOR edge2 = v2 - v0;

					bestNormal = XMVector3Normalize(XMVector3Cross(edge1, edge2));

				}

			}

		}

	}



	if (hasHit)

	{

		outHit.distance = minT;


		XMVECTOR hitPoint = XMVectorAdd(origin, XMVectorScale(dir, minT));

		XMStoreFloat3(&outHit.point, hitPoint);

		XMStoreFloat3(&outHit.normal, bestNormal);

		outHit.userPtr = (void*)this;

		return true;

	}



	return false;

}











Model::Node* Model::FindNode(const char* name)

{


	for (Node& node : nodes)

	{


		if (std::strcmp(node.name.c_str(), name) == 0)

		{


			return &node;

		}

	}




	return nullptr;

}




int Model::GetNodeIndex(const char* name) const

{

	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)

	{

		if (nodes.at(nodeIndex).name == name)

		{

			return static_cast<int>(nodeIndex);

		}

	}

	return -1;

}



void Model::ComputeAnimation(int animationIndex, int nodeIndex, float time, NodePose& nodePose) const

{

	const Animation& animation = animations.at(animationIndex);

	const NodeAnim& nodeAnim = animation.nodeAnims.at(nodeIndex);




	for (size_t index = 0; index < nodeAnim.positionKeyframes.size() - 1; ++index)

	{


		const VectorKeyframe& keyframe0 = nodeAnim.positionKeyframes.at(index);

		const VectorKeyframe& keyframe1 = nodeAnim.positionKeyframes.at(index + 1);

		if (time >= keyframe0.seconds && time <= keyframe1.seconds)

		{


			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);




			DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&keyframe0.value);

			DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&keyframe1.value);

			DirectX::XMVECTOR V = DirectX::XMVectorLerp(V0, V1, rate);


			DirectX::XMStoreFloat3(&nodePose.position, V);

		}

	}


	for (size_t index = 0; index < nodeAnim.rotationKeyframes.size() - 1; ++index)

	{


		const QuaternionKeyframe& keyframe0 = nodeAnim.rotationKeyframes.at(index);

		const QuaternionKeyframe& keyframe1 = nodeAnim.rotationKeyframes.at(index + 1);

		if (time >= keyframe0.seconds && time <= keyframe1.seconds)

		{


			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);




			DirectX::XMVECTOR Q0 = DirectX::XMLoadFloat4(&keyframe0.value);

			DirectX::XMVECTOR Q1 = DirectX::XMLoadFloat4(&keyframe1.value);

			DirectX::XMVECTOR Q = DirectX::XMQuaternionSlerp(Q0, Q1, rate);


			DirectX::XMStoreFloat4(&nodePose.rotation, Q);

		}

	}


	for (size_t index = 0; index < nodeAnim.scaleKeyframes.size() - 1; ++index)

	{


		const VectorKeyframe& keyframe0 = nodeAnim.scaleKeyframes.at(index);

		const VectorKeyframe& keyframe1 = nodeAnim.scaleKeyframes.at(index + 1);

		if (time >= keyframe0.seconds && time <= keyframe1.seconds)

		{


			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);




			DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&keyframe0.value);

			DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&keyframe1.value);

			DirectX::XMVECTOR V = DirectX::XMVectorLerp(V0, V1, rate);


			DirectX::XMStoreFloat3(&nodePose.scale, V);

		}

	}

}



void Model::ComputeAnimation(int animationIndex, float time, std::vector<NodePose>& nodePose)

{

	if (nodePose.size() != nodes.size())

	{

		nodePose.resize(nodes.size());

	}

	for (size_t nodeIndex = 0; nodeIndex < nodePose.size(); ++nodeIndex)

	{

		ComputeAnimation(animationIndex, static_cast<int>(nodeIndex), time, nodePose.at(nodeIndex));

	}





}




int Model::GetAnimationIndex(const char* name) const

{

	for (size_t animationIndex = 0; animationIndex < animations.size(); ++animationIndex)

	{

		if (animations.at(animationIndex).name == name)

		{

			return static_cast<int>(animationIndex);

		}

	}

	return -1;

}




void Model::SetNodePoses(const std::vector<NodePose>& nodePoses)

{

	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)

	{

		const NodePose& pose = nodePoses.at(nodeIndex);

		Node& node = nodes.at(nodeIndex);



		node.position = pose.position;

		node.rotation = pose.rotation;

		node.scale = pose.scale;

	}

}




void Model::GetNodePoses(std::vector<NodePose>& nodePoses) const

{

	if (nodePoses.size() != nodes.size())

	{

		nodePoses.resize(nodes.size());

	}

	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)

	{

		const Node& node = nodes.at(nodeIndex);

		NodePose& pose = nodePoses.at(nodeIndex);



		pose.position = node.position;

		pose.rotation = node.rotation;

		pose.scale = node.scale;

	}

}



int Model::GetSubsetCount() const

{

	return static_cast<int>(meshes.size());

}



// Model.cpp



Model::MeshData Model::GetMeshData(int subsetIndex)

{

	if (subsetIndex < 0 || subsetIndex >= meshes.size())

	{

		return { nullptr, nullptr };

	}




	return { &meshes[subsetIndex].vertices, &meshes[subsetIndex].indices };

}




void Model::UpdateMeshIndices(ID3D11Device* device, int subsetIndex, const std::vector<uint32_t>& newIndices)

{

	(void)device;

	if (subsetIndex < 0 || subsetIndex >= meshes.size()) return;

	Mesh& mesh = meshes[subsetIndex];

	mesh.indices = newIndices;

	SyncModelResourceMeshBuffers(subsetIndex);

	SyncModelResourceSceneData();

}




void Model::Serialize(const char* filename)

{

	std::ofstream ofstream(filename, std::ios::binary);

	if (ofstream.is_open())

	{

		cereal::BinaryOutputArchive archive(ofstream);



		try

		{

			archive(

				CEREAL_NVP(nodes),

				CEREAL_NVP(materials),

				CEREAL_NVP(meshes),

				CEREAL_NVP(animations)

			);

		}

		catch (...)

		{

			_ASSERT_EXPR_A(false, "Model serialize failed.");

		}

	}

}




void Model::Deserialize(const char* filename)

{
	nodes.clear();
	materials.clear();
	meshes.clear();
	animations.clear();

	std::ifstream istream(filename, std::ios::binary);

	if (istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(
				CEREAL_NVP(nodes),
				CEREAL_NVP(materials),
				CEREAL_NVP(meshes),
				CEREAL_NVP(animations)
			);
		}
		catch (const std::exception& e)
		{
			LOG_WARN("[Model] Deserialize failed path='%s' what='%s'", filename, e.what());
			nodes.clear();
			materials.clear();
			meshes.clear();
			animations.clear();
		}
		catch (...)
		{
			LOG_WARN("[Model] Deserialize failed path='%s'", filename);
			nodes.clear();
			materials.clear();
			meshes.clear();
			animations.clear();
		}
	}
	else
	{
		LOG_WARN("[Model] File not found path='%s'", filename);
	}

}




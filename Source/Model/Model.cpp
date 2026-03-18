#include "System/Misc.h"

#include "Model.h"

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

#include "System/ResourceManager.h" // GetTextureｿｽ用ｿｽｿｽｿｽ驍ｽｿｽｿｽ

#include "RHI/DX11/DX11Buffer.h"    // DX11ｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽbｿｽvｿｽｿｽｿｽ驍ｽｿｽｿｽ

#include "RHI/ICommandList.h"       // ｿｽoｿｽCｿｽｿｽｿｽhｿｽｿｽｿｽｿｽｿｽﾌゑｿｽｿｽｿｽ

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



Model::Model(const char* filename, float scaling)

	:scaling(scaling)

{

	IResourceFactory* factory = Graphics::Instance().GetResourceFactory();

	ID3D11Device* device = Graphics::Instance().GetDevice(); // DX12 ?? nullptr



	std::filesystem::path sourceFilepath(filename);

	std::filesystem::path dirpath(sourceFilepath.parent_path());

	std::filesystem::path cerealPath(sourceFilepath);



	cerealPath.replace_extension(".cereal");

	if (std::filesystem::exists(cerealPath))

	{

		Deserialize(cerealPath.string().c_str());

	}

	else

	{

		AssimpImporter importer(filename);

		importer.LoadMaterials(materials);

		importer.LoadNodes(nodes);

		importer.LoadMeshes(meshes, nodes);

		importer.LoadAnimations(animations, nodes);

		Serialize(cerealPath.string().c_str());

	}





	//ｿｽmｿｽ[ｿｽhｿｽ\ｿｽz

	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)

	{

		Node& node = nodes.at(nodeIndex);



		//ｿｽeｿｽqｿｽﾖ係ｿｽｿｽｿｽ\ｿｽz

		node.parent = node.parentIndex >= 0 ? &nodes.at(node.parentIndex) : nullptr;

		if (node.parent != nullptr)

		{

			node.parent->children.emplace_back(&node);

		}

	}





	//ｿｽ}ｿｽeｿｽｿｽｿｽAｿｽｿｽｿｽ\ｿｽz

	//for (Material& material : materials)

	//{

	//	if (material.diffuseTextureFileName.empty())

	//	{

	//		ｿｽ_ｿｽ~ｿｽ[ｿｽeｿｽNｿｽXｿｽ`ｿｽｿｽｿｽ成

	//		HRESULT hr = GpuResourceUtils::CreateDummyTexture(device, 0xFFFFFFFF,

	//										material.diffuseMap.GetAddressOf());

	//		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	//	}

	//	else

	//	{

	//	ｿｽfｿｽBｿｽtｿｽｿｽｿｽ[ｿｽYｿｽeｿｽNｿｽXｿｽ`ｿｽｿｽｿｽﾇみ搾ｿｽｿｽｿｽ

	//	std::filesystem::path diffuseTexturePath(dirpath / material.diffuseTextureFileName);

	//	HRESULT hr = GpuResourceUtils::LoadTexture(device, diffuseTexturePath.string().c_str(),

	//												material.diffuseMap.GetAddressOf());

	//		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	//	}

	//	if (material.normalTextureFileName.empty())

	//	{

	//		ｿｽ@ｿｽｿｽｿｽ_ｿｽ~ｿｽ[ｿｽeｿｽNｿｽXｿｽ`ｿｽｿｽｿｽ成

	//		HRESULT hr = GpuResourceUtils::CreateDummyTexture(device, 0xFFFF7F7F,

	//			material.normalMap.GetAddressOf());

	//		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	//	}

	//	else

	//	{

	//		ｿｽ@ｿｽｿｽｿｽeｿｽNｿｽXｿｽ`ｿｽｿｽｿｽﾇみ搾ｿｽｿｽｿｽ

	//		std::filesystem::path texturePath(dirpath / material.normalTextureFileName);

	//		HRESULT hr = GpuResourceUtils::LoadTexture(device, texturePath.string().c_str(),

	//			material.normalMap.GetAddressOf());

	//		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	//	}

	//}

//		// ｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽ ｿｽ}ｿｽeｿｽｿｽｿｽAｿｽｿｽｿｽ\ｿｽz ｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽ

//	for (Material& m : materials)

//	{

//		// ｿｽｿｽｿｽｿｽ 1) albedo ｿｽｿｽ diffuse ｿｽtｿｽHｿｽ[ｿｽｿｽｿｽoｿｽbｿｽNｿｽｿｽｿｽｿｽｿｽｿｽﾉ確ｿｽｿｽ ｿｽｿｽｿｽｿｽ

//		if (m.albedoTextureFileName.empty())

//			m.albedoTextureFileName = m.diffuseTextureFileName;   // ｿｽｿｽｿｽｿｽｿｽｿｽRｿｽsｿｽ[ｿｽﾌゑｿｽ

//

//		// ｿｽｿｽｿｽｿｽ 2) ｿｽｿｽｿｽﾊｿｽｿｽ[ｿｽ_ｿｽ[ ｿｽｿｽｿｽｿｽ

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

//		// ｿｽｿｽｿｽｿｽ 3) ｿｽeｿｽ}ｿｽbｿｽvｿｽﾇみ搾ｿｽｿｽｿｽ ｿｽｿｽｿｽｿｽ

//		SafeLoad(m.diffuseTextureFileName, m.diffuseMap);                  // ｿｽｿｽｿｽ_ｿｽ~ｿｽ[

//		SafeLoad(m.normalTextureFileName, m.normalMap, 0xFFFF7F7F);    // ｿｽtｿｽｿｽｿｽbｿｽgｿｽmｿｽ[ｿｽ}ｿｽｿｽ

//		SafeLoad(m.metallicTextureFileName, m.metallicMap, 0xFF000000);    // ｿｽｿｽ = 0.0

//		SafeLoad(m.roughnessTextureFileName, m.roughnessMap, 0xFFFFFFFF);    // ｿｽｿｽ = 1.0

//		SafeLoad(m.albedoTextureFileName, m.albedoMap);                   

//		SafeLoad(m.occlusionTextureFileName, m.occlusionMap, 0xFFFFFFFF);    // ｿｽｿｽｿｽx 1.0

//		SafeLoad(m.emissiveTextureFileName, m.emissiveMap, 0xFF000000);    // ｿｽｿｽ = 0.0

//	}



	for (Material& material : materials)

	{

		auto& rm = ResourceManager::Instance();



		// ｿｽｿｽ ｿｽCｿｽｿｽｿｽFｿｽeｿｽNｿｽXｿｽ`ｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽ鼾ｿｽﾍ、ｿｽwｿｽ閧ｳｿｽ黷ｽｿｽFｿｽﾌ「ｿｽ_ｿｽ~ｿｽ[ITextureｿｽvｿｽｶ撰ｿｽｿｽｿｽｿｽﾄ返ゑｿｽｿｽI

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





	for (Mesh& mesh : meshes)

	{

		mesh.node = &nodes.at(mesh.nodeIndex);

		mesh.material = &materials.at(mesh.materialIndex);



		for (Bone& bone : mesh.bones) {

			bone.node = &nodes.at(bone.nodeIndex);

		}



		// ｿｽｿｽ ｿｽCｿｽｿｽ: RHIｿｽﾅバｿｽbｿｽtｿｽ@ｿｽｿｽｿｽｿｽ (DX11Bufferｿｽｿｽｿｽgｿｽp)

		// IResourceFactory 邨檎罰縺ｧ繝舌ャ繝輔ぃ逕滓撰ｼDX11/DX12 蜈ｱ騾夲ｼ

		mesh.vertexBuffer = std::shared_ptr<IBuffer>(

			factory->CreateBuffer(

				static_cast<uint32_t>(sizeof(Vertex) * mesh.vertices.size()),

				BufferType::Vertex,

				mesh.vertices.data()

			).release()

		);



		mesh.indexBuffer = std::shared_ptr<IBuffer>(

			factory->CreateBuffer(

				static_cast<uint32_t>(sizeof(uint32_t) * mesh.indices.size()),

				BufferType::Index,

				mesh.indices.data()

			).release()

		);

	}





	//ｿｽmｿｽ[ｿｽhｿｽLｿｽｿｽｿｽbｿｽVｿｽｿｽ

	nodeCaches.resize(nodes.size());

}



//ｿｽgｿｽｿｽｿｽｿｽｿｽXｿｽtｿｽHｿｽ[ｿｽｿｽｿｽXｿｽVｿｽｿｽｿｽｿｽ

void Model::UpdateTransform(const DirectX::XMFLOAT4X4& worldTransform)

{

	DirectX::XMMATRIX ParentWorldTransform = DirectX::XMLoadFloat4x4(&worldTransform);



	//ｿｽEｿｽｿｽｿｽｿｽWｿｽnｿｽｿｽｿｽ迯ｶｿｽｿｽｿｽｿｽWｿｽnｿｽﾖ変奇ｿｽｿｽｿｽｿｽｿｽsｿｽｿｽ

	DirectX::XMMATRIX CoordinateSystemTransform = DirectX::XMMatrixScaling(-scaling, scaling, scaling);



	for (Node& node : nodes)

	{

		//ｿｽｿｽｿｽ[ｿｽJｿｽｿｽｿｽsｿｽｿｽZｿｽo

		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);

		DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));

		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(node.position.x, node.position.y, node.position.z);

		DirectX::XMMATRIX LocalTransform = S * R * T;



		//ｿｽOｿｽｿｽｿｽ[ｿｽoｿｽｿｽｿｽsｿｽｿｽZｿｽo

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



		//ｿｽｿｽｿｽ[ｿｽｿｽｿｽhｿｽsｿｽｿｽZｿｽo

		DirectX::XMMATRIX WorldTransform = GlobalTransform * CoordinateSystemTransform * ParentWorldTransform;



		//ｿｽvｿｽZｿｽｿｽｿｽﾊゑｿｽｿｽiｿｽ[

		DirectX::XMStoreFloat4x4(&node.localTransform, LocalTransform);

		DirectX::XMStoreFloat4x4(&node.globalTransform, GlobalTransform);

		DirectX::XMStoreFloat4x4(&node.worldTransform, WorldTransform);

	}

	ComputeWorldBounds();

}







//ｿｽAｿｽjｿｽｿｽｿｽ[ｿｽVｿｽｿｽｿｽｿｽｿｽﾄ撰ｿｽ

void Model::PlayAnimation(int index, bool loop, float blendSeconds)

{

	currentAnimationIndex = index;

	currentAnimationSeconds = 0;

	animationLoop = loop;

	animationPlaying = true;



	//ｿｽuｿｽｿｽｿｽｿｽｿｽhｿｽpｿｽｿｽｿｽｿｽｿｽ[ｿｽ^

	animationBlending = blendSeconds > 0.0f;

	currentAnimationBlendSeconds = 0.0f;

	animationBlendSecondsLength = blendSeconds;



	//ｿｽｿｽｿｽﾝの姿ｿｽｿｽｿｽｿｽｿｽLｿｽｿｽｿｽbｿｽVｿｽｿｽｿｽｿｽｿｽｿｽ

	for (size_t i = 0; i < nodes.size(); ++i)

	{

		const Node& src = nodes.at(i);

		NodeCache& dst = nodeCaches.at(i);



		dst.position = src.position;

		dst.rotation = src.rotation;

		dst.scale = src.scale;

	}

}



//ｿｽAｿｽjｿｽｿｽｿｽ[ｿｽVｿｽｿｽｿｽｿｽｿｽﾄ撰ｿｽｿｽｿｽｿｽｿｽ

bool Model::IsPlayAnimation() const

{

	if (currentAnimationIndex < 0) return false;

	if (currentAnimationIndex >=animations.size()) return false;

	return animationPlaying;

}



//ｿｽAｿｽjｿｽｿｽｿｽ[ｿｽVｿｽｿｽｿｽｿｽｿｽXｿｽVｿｽｿｽｿｽｿｽ

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





//ｿｽAｿｽjｿｽｿｽｿｽ[ｿｽVｿｽｿｽｿｽｿｽｿｽvｿｽZｿｽｿｽｿｽｿｽ

void Model::ComputeAnimation(float dt)

{

	if (!IsPlayAnimation()) return;



	//ｿｽwｿｽｿｽﾌアｿｽjｿｽｿｽｿｽ[ｿｽVｿｽｿｽｿｽｿｽｿｽfｿｽ[ｿｽ^ｿｽｿｽｿｽ謫ｾ

	const Animation& animation = animations.at(currentAnimationIndex);



	//ｿｽmｿｽ[ｿｽhｿｽｿｽｿｽﾌアｿｽjｿｽｿｽｿｽ[ｿｽVｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽ

	for (size_t nodeIndex = 0; nodeIndex < animation.nodeAnims.size(); ++nodeIndex)

	{

		Node& node = nodes.at(nodeIndex);

		const Model::NodeAnim& nodeAnim = animation.nodeAnims.at(nodeIndex);



		//ｿｽﾊ置

		for (size_t index = 0; index < nodeAnim.positionKeyframes.size() - 1; ++index)

		{

			//ｿｽｿｽｿｽﾝの趣ｿｽｿｽﾔゑｿｽｿｽﾇのキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ間にゑｿｽｿｽ驍ｩｿｽwｿｽ閧ｷｿｽｿｽ

			const Model::VectorKeyframe& keyframe0 = nodeAnim.positionKeyframes.at(index);

			const Model::VectorKeyframe& keyframe1 = nodeAnim.positionKeyframes.at(index + 1);



			if (currentAnimationSeconds >= keyframe0.seconds && currentAnimationSeconds < keyframe1.seconds)

			{

				//ｿｽﾄ撰ｿｽｿｽｿｽｿｽﾔとキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ趣ｿｽｿｽﾔゑｿｽｿｽｿｽ竓ｮｿｽｿｽｿｽｿｽｿｽZｿｽoｿｽｿｽｿｽｿｽ

				float rate = (currentAnimationSeconds - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);



				//ｿｽOｿｽﾌキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾆ趣ｿｽｿｽﾌキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ姿ｿｽｿｽｿｽｿｽ竓ｮ

				DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&keyframe0.value);

				DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&keyframe1.value);

				DirectX::XMVECTOR V = DirectX::XMVectorLerp(V0, V1, rate);



				//ｿｽvｿｽZｿｽｿｽｿｽﾊゑｿｽｿｽmｿｽ[ｿｽhｿｽﾉ格ｿｽ[

				DirectX::XMStoreFloat3(&node.position, V);

			}

		}



		//ｿｽｿｽ]

		for (size_t index = 0; index < nodeAnim.rotationKeyframes.size() - 1; ++index)

		{

			//ｿｽｿｽｿｽﾝの趣ｿｽｿｽﾔゑｿｽｿｽﾇのキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ間にゑｿｽｿｽ驍ｩｿｽｿｽｿｽ閧ｷｿｽｿｽ

			const Model::QuaternionKeyframe& keyframe0 = nodeAnim.rotationKeyframes.at(index);

			const Model::QuaternionKeyframe& keyframe1 = nodeAnim.rotationKeyframes.at(index + 1);



			if (currentAnimationSeconds >= keyframe0.seconds && currentAnimationSeconds < keyframe1.seconds)

			{

				//ｿｽﾄ撰ｿｽｿｽｿｽｿｽﾔとキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ趣ｿｽｿｽﾔゑｿｽｿｽｿｽ竓ｮｿｽｿｽｿｽｿｽｿｽZｿｽoｿｽｿｽｿｽｿｽ

				float rate = (currentAnimationSeconds - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);



				//ｿｽOｿｽﾌキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾆ趣ｿｽｿｽﾌキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ姿ｿｽｿｽｿｽｿｽ竓ｮ

				DirectX::XMVECTOR Q0 = DirectX::XMLoadFloat4(&keyframe0.value);

				DirectX::XMVECTOR Q1 = DirectX::XMLoadFloat4(&keyframe1.value);

				DirectX::XMVECTOR Q = DirectX::XMQuaternionSlerp(Q0, Q1, rate);



				//ｿｽvｿｽZｿｽｿｽｿｽﾊゑｿｽｿｽmｿｽ[ｿｽhｿｽﾉ格ｿｽ[

				DirectX::XMStoreFloat4(&node.rotation, Q);

			}

		}



		//ｿｽXｿｽPｿｽ[ｿｽｿｽ

		for (size_t index = 0; index < nodeAnim.scaleKeyframes.size() - 1; ++index)

		{

			//ｿｽｿｽｿｽﾝの趣ｿｽｿｽﾔゑｿｽｿｽﾇのキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ間にゑｿｽｿｽ驍ｩｿｽｿｽｿｽ閧ｷｿｽｿｽ

			const Model::VectorKeyframe& keyframe0 = nodeAnim.scaleKeyframes.at(index);

			const Model::VectorKeyframe& keyframe1 = nodeAnim.scaleKeyframes.at(index + 1);



			if (currentAnimationSeconds >= keyframe0.seconds && currentAnimationSeconds < keyframe1.seconds)

			{

				//ｿｽﾄ撰ｿｽｿｽｿｽｿｽﾔとキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ趣ｿｽｿｽﾔゑｿｽｿｽｿｽ竓ｮｿｽｿｽｿｽｿｽｿｽZｿｽoｿｽｿｽｿｽｿｽ

				float rate = (currentAnimationSeconds - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);



				//ｿｽOｿｽﾌキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾆ趣ｿｽｿｽﾌキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ姿ｿｽｿｽｿｽｿｽ竓ｮ

				DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&keyframe0.value);

				DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&keyframe1.value);

				DirectX::XMVECTOR V = DirectX::XMVectorLerp(V0, V1, rate);



				//ｿｽvｿｽZｿｽｿｽｿｽﾊゑｿｽｿｽmｿｽ[ｿｽhｿｽﾉ格ｿｽ[

				DirectX::XMStoreFloat3(&node.scale, V);

			}

		}

	}



	//ｿｽｿｽｿｽﾔ経ｿｽｿｽ

	currentAnimationSeconds += dt;



	//ｿｽﾄ撰ｿｽｿｽｿｽｿｽﾔゑｿｽｿｽIｿｽ[ｿｽｿｽｿｽﾔを超ゑｿｽｿｽｿｽｿｽｿｽ

	if (currentAnimationSeconds >= animation.secondsLength)

	{

		if (animationLoop)

		{

			//ｿｽﾄ撰ｿｽｿｽｿｽｿｽﾔゑｿｽｿｽｿｽｿｽｿｽｿｽﾟゑｿｽ

			currentAnimationSeconds -= animation.secondsLength;

		}

		else

		{

			//ｿｽﾄ撰ｿｽｿｽIｿｽｿｽｿｽｿｽｿｽﾔにゑｿｽｿｽｿｽ

			currentAnimationSeconds = animation.secondsLength;

			animationPlaying = false;

		}

	}

}





//ｿｽuｿｽｿｽｿｽｿｽｿｽfｿｽBｿｽｿｽｿｽOｿｽvｿｽZｿｽｿｽｿｽｿｽ

void Model::ComputeBlending(float dt)

{

	if (!animationBlending) return;



	//ｿｽuｿｽｿｽｿｽｿｽｿｽhｿｽｿｽｿｽﾌ計ｿｽZ

	float rate = currentAnimationBlendSeconds / animationBlendSecondsLength;



	//ｿｽuｿｽｿｽｿｽｿｽｿｽhｿｽvｿｽZ

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



	//ｿｽｿｽｿｽﾔ経ｿｽｿｽ

	currentAnimationBlendSeconds += dt;

	if (currentAnimationBlendSeconds >= animationBlendSecondsLength)

	{

		currentAnimationBlendSeconds = animationBlendSecondsLength;

		animationBlending = false;

	}

}



// ｿｽｿｽｿｽ[ｿｽｿｽｿｽhｿｽoｿｽEｿｽｿｽｿｽfｿｽBｿｽｿｽｿｽOｿｽ{ｿｽbｿｽNｿｽXｿｽvｿｽZ

void Model::ComputeWorldBounds()

{

	// ｿｽｿｽｿｽｿｽｿｽｿｽ (ｿｽｿｽｿｽ閧ｦｿｽﾈゑｿｽｿｽTｿｽCｿｽYｿｽﾉゑｿｽｿｽﾄゑｿｽｿｽｿｽ)

	bounds.Center = { 0, 0, 0 };

	bounds.Extents = { 0, 0, 0 };



	bool firstMerged = false;



	// ｿｽSｿｽｿｽｿｽbｿｽVｿｽｿｽｿｽﾌボｿｽbｿｽNｿｽXｿｽｿｽｿｽvｿｽZｿｽｿｽｿｽﾄ鯉ｿｽｿｽｿｽｿｽｿｽｿｽｿｽ

	for (const Mesh& mesh : meshes)

	{

		// ｿｽｿｽｿｽbｿｽVｿｽｿｽｿｽPｿｽﾌゑｿｽAABBｿｽｿｽｿｽ成 (ｿｽｿｽｿｽ_ｿｽfｿｽ[ｿｽ^ｿｽｿｽｿｽ逅ｶｿｽｿｽ)

		DirectX::BoundingBox meshBounds;



		// ｿｽｿｽｿｽ_ｿｽｿｽｿｽｿｽ0ｿｽﾈゑｿｽXｿｽLｿｽbｿｽv

		if (mesh.vertices.empty()) continue;



		// ｿｽｿｽｿｽ_ｿｽXｿｽgｿｽｿｽｿｽCｿｽh(ｿｽTｿｽCｿｽY)ｿｽｿｽｿｽwｿｽ閧ｵｿｽﾄボｿｽbｿｽNｿｽXｿｽ成

		DirectX::BoundingBox::CreateFromPoints(

			meshBounds,

			mesh.vertices.size(),

			&mesh.vertices[0].position,

			sizeof(Vertex)

		);



		// ｿｽｿｽｿｽﾌｿｽｿｽbｿｽVｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽmｿｽ[ｿｽhｿｽﾌｿｽｿｽ[ｿｽｿｽｿｽhｿｽsｿｽｿｽﾅ変奇ｿｽ (ｿｽｿｽ]ｿｽEｿｽﾚ難ｿｽｿｽEｿｽgｿｽkｿｽｽ映)

		DirectX::XMMATRIX worldMat = DirectX::XMLoadFloat4x4(&mesh.node->worldTransform);

		meshBounds.Transform(meshBounds, worldMat);



		// ｿｽSｿｽﾌのボｿｽbｿｽNｿｽXｿｽﾆ鯉ｿｽｿｽｿｽ

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



	// 1. ｿｽﾜゑｿｽｿｽﾍゑｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽﾏにボｿｽbｿｽNｿｽXｿｽｿｽｿｽｿｽ (ｿｽｿｽｿｽ黷ｪｿｽOｿｽ黷ｽｿｽ迺ｿｽgｿｽｿｽｿｽｿｽｿｽﾈゑｿｽ)

	XMVECTOR origin = XMLoadFloat3(&rayOrigin);

	XMVECTOR dir = XMLoadFloat3(&rayDir);

	float distToBox = 0.0f;



	// ｿｽ{ｿｽbｿｽNｿｽXｿｽﾉゑｿｽｿｽ逑厄ｿｽｿｽｿｽｿｽｿｽﾄなゑｿｽｿｽｿｽﾎ終ｿｽｿｽ

	if (!bounds.Intersects(origin, dir, distToBox))

	{

		return false;

	}



	// 2. ｿｽﾚ細費ｿｽｿｽｿｽ: ｿｽｿｽｿｽﾗての抵ｿｽｿｽ_ｿｽﾌ抵ｿｽｿｽｿｽｿｽ辜鯉ｿｽCｿｽﾉ茨ｿｽﾔ近ゑｿｽｿｽｿｽｿｽﾌゑｿｽTｿｽｿｽ

	float minDistanceSqr = FLT_MAX;

	bool found = false;

	XMVECTOR bestVertexPos = XMVectorZero();



	// ｿｽｿｽｿｽｿｽp閾値 (ｿｽｿｽｿｽCｿｽｿｽｿｽ轤ｱｿｽﾌ具ｿｽｿｽｿｽｿｽﾈ難ｿｽｿｽﾉゑｿｽｿｽ髓ｸｿｽ_ｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽﾉゑｿｽｿｽｿｽ)

	// ｿｽｿｽｿｽｿｽﾊ擾ｿｽﾅマｿｽEｿｽXｿｽJｿｽ[ｿｽ\ｿｽｿｽｿｽﾌ近ゑｿｽｿｽﾉゑｿｽｿｽｿｽ_ｿｽｿｽｿｽｿｽｿｽｿｽｿｽEｿｽｿｽｿｽｿｽｿｽｿｽ

	const float threshold = 0.5f; // ｿｽKｿｽvｿｽﾉ会ｿｽｿｽｿｽｿｽﾄ抵ｿｽｿｽｿｽ (0.5mｿｽﾈ難ｿｽ)



	for (const Mesh& mesh : meshes)

	{

		XMMATRIX worldMat = XMLoadFloat4x4(&mesh.node->worldTransform);



		for (const Vertex& v : mesh.vertices)

		{

			// ｿｽｿｽｿｽ_ｿｽｿｽｿｽｿｽｿｽ[ｿｽｿｽｿｽhｿｽｿｽｿｽWｿｽﾉ変奇ｿｽ

			XMVECTOR localPos = XMLoadFloat3(&v.position);

			XMVECTOR worldPos = XMVector3Transform(localPos, worldMat);



			// ｿｽｿｽｿｽC(ｿｽｿｽｿｽｿｽ)ｿｽﾆ点(ｿｽｿｽｿｽ_)ｿｽﾌ具ｿｽｿｽｿｽｿｽｿｽｿｽvｿｽZ

			// ｿｽ_Pｿｽｿｽｿｽ辜鯉ｿｽCｿｽﾖのベｿｽNｿｽgｿｽｿｽ V = P - Origin

			// ｿｽｿｽｿｽeｿｽxｿｽNｿｽgｿｽｿｽ Proj = (V dot Dir) * Dir

			// ｿｽｿｽｿｽｿｽｿｽxｿｽNｿｽgｿｽｿｽ Perp = V - Proj

			// ｿｽｿｽｿｽｿｽｿｽﾌ難ｿｽｿｽ = Perp dot Perp

			XMVECTOR vToP = XMVectorSubtract(worldPos, origin);

			float t = XMVectorGetX(XMVector3Dot(vToP, dir));



			// ｿｽJｿｽｿｽｿｽｿｽｿｽﾌ鯉ｿｽｿｽﾉゑｿｽｿｽ髓ｸｿｽ_ｿｽﾍ厄ｿｽｿｽｿｽ

			if (t < 0.0f) continue;



			XMVECTOR proj = XMVectorScale(dir, t);

			XMVECTOR perp = XMVectorSubtract(vToP, proj);

			float distSqr = XMVectorGetX(XMVector3Dot(perp, perp));



			// ｿｽｿｽﾔｿｽｿｽCｿｽﾉ近ゑｿｽ(ｿｽJｿｽ[ｿｽ\ｿｽｿｽｿｽﾉ近ゑｿｽ)ｿｽｿｽｿｽ_ｿｽｿｽｿｽLｿｽ^

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



	// 1. ｿｽyｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽzｿｽoｿｽEｿｽｿｽｿｽfｿｽBｿｽｿｽｿｽOｿｽ{ｿｽbｿｽNｿｽXｿｽﾉ難ｿｽｿｽｿｽｿｽｿｽｿｽﾄゑｿｽｿｽ驍ｩｿｽH

	// ｿｽｿｽｿｽｿｽﾉゑｿｽｿｽAｿｽｿｽﾊ外ｿｽﾌｿｽｿｽfｿｽｿｽｿｽ竕難ｿｽｿｽｿｽﾌｿｽｿｽfｿｽｿｽｿｽｿｽｿｽuｿｽｿｽｿｽﾉスｿｽLｿｽbｿｽvｿｽﾅゑｿｽｿｽｿｽ

	if (!bounds.Intersects(origin, dir, distToBox))

	{

		return false;

	}



	// 2. ｿｽyｿｽﾚ細費ｿｽｿｽｿｽzｿｽSｿｽｿｽｿｽbｿｽVｿｽｿｽｿｽﾌ全ｿｽ|ｿｽｿｽｿｽSｿｽｿｽｿｽｿｽｿｽ`ｿｽFｿｽbｿｽN

	bool hasHit = false;

	float minT = FLT_MAX;

	XMVECTOR bestNormal = XMVectorZero();



	for (const Mesh& mesh : meshes)

	{

		// ｿｽｿｽｿｽbｿｽVｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽmｿｽ[ｿｽhｿｽﾌｿｽｿｽ[ｿｽｿｽｿｽhｿｽsｿｽｿｽｿｽｿｽ謫ｾ

		XMMATRIX worldMat = XMLoadFloat4x4(&mesh.node->worldTransform);



		// ｿｽｿｽｿｽCｿｽｿｽｿｽｿｽｿｽ[ｿｽJｿｽｿｽｿｽｿｽﾔに変奇ｿｽｿｽｿｽｿｽｿｽｿｽﾙゑｿｽｿｽｿｽｿｽvｿｽZｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽA

		// ｿｽｿｽｿｽｿｽﾍ抵ｿｽｿｽｿｽｿｽIｿｽﾈ「ｿｽｿｽｿｽ_ｿｽｿｽｿｽｿｽｿｽ[ｿｽｿｽｿｽhｿｽｿｽﾔに変奇ｿｽｿｽｿｽｿｽﾄ費ｿｽｿｽｿｽvｿｽｿｽｿｽｿｽｿｽｿｽ@ｿｽﾅ行ｿｽｿｽ

		const auto& indices = mesh.indices;

		const auto& vertices = mesh.vertices;



		// ｿｽCｿｽｿｽｿｽfｿｽbｿｽNｿｽXｿｽoｿｽbｿｽtｿｽ@ｿｽｿｽ3ｿｽﾂゑｿｽｿｽﾂ（ｿｽOｿｽpｿｽ`ｿｽPｿｽﾊ）ｿｽﾅ会ｿｽ

		for (size_t i = 0; i < indices.size(); i += 3)

		{

			// 3ｿｽｿｽｿｽ_ｿｽﾌイｿｽｿｽｿｽfｿｽbｿｽNｿｽXｿｽ謫ｾ

			uint32_t i0 = indices[i];

			uint32_t i1 = indices[i + 1];

			uint32_t i2 = indices[i + 2];



			// ｿｽｿｽｿｽ_ｿｽｿｽｿｽWｿｽｿｽｿｽｿｽｿｽ[ｿｽｿｽｿｽhｿｽｿｽｿｽWｿｽﾉ変奇ｿｽ

			XMVECTOR v0 = XMVector3Transform(XMLoadFloat3(&vertices[i0].position), worldMat);

			XMVECTOR v1 = XMVector3Transform(XMLoadFloat3(&vertices[i1].position), worldMat);

			XMVECTOR v2 = XMVector3Transform(XMLoadFloat3(&vertices[i2].position), worldMat);



			float t = 0.0f;

			// DirectXｿｽWｿｽｿｽｿｽﾌｿｽｿｽC vs ｿｽOｿｽpｿｽ`ｿｽｿｽｿｽｿｽ

			if (TriangleTests::Intersects(origin, dir, v0, v1, v2, t))

			{

				// ｿｽｿｽｿｽｿｽOｿｽﾅ難ｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽ鼾ｿｽﾌみ記ｿｽ^

				if (t < minT)

				{

					minT = t;

					hasHit = true;



					// ｿｽ@ｿｽｿｽｿｽｿｽｿｽvｿｽZ (ｿｽﾊ法ｿｽｿｽ)

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

		// ｿｽﾕ突位置 = Origin + Dir * T

		XMVECTOR hitPoint = XMVectorAdd(origin, XMVectorScale(dir, minT));

		XMStoreFloat3(&outHit.point, hitPoint);

		XMStoreFloat3(&outHit.normal, bestNormal);

		outHit.userPtr = (void*)this; // ModelｿｽｿｽｿｽgｿｽﾖのポｿｽCｿｽｿｽｿｽ^ｿｽｿｽｿｽｿｽｿｽﾄゑｿｽｿｽｿｽ

		return true;

	}



	return false;

}











Model::Node* Model::FindNode(const char* name)

{

	// ｿｽｿｽｿｽﾗてのノｿｽ[ｿｽhｿｽ酷ｿｽｿｽｿｽｿｽｿｽﾅ厄ｿｽｿｽOｿｽｿｽrｿｽｿｽｿｽｿｽ

	for (Node& node : nodes)

	{

		// ｿｽｿｽｿｽｿｽｿｽﾌ厄ｿｽｿｽOｿｽﾆノｿｽ[ｿｽhｿｽﾌ厄ｿｽｿｽOｿｽｿｽｿｽrｿｽｿｽｿｽｿｽvｿｽｿｽｿｽ驍ｩｿｽ`ｿｽFｿｽbｿｽN

		if (std::strcmp(node.name.c_str(), name) == 0)

		{

			// ｿｽｿｽvｿｽｿｽｿｽｿｽｿｽ轤ｻｿｽﾌノｿｽ[ｿｽhｿｽﾌアｿｽhｿｽｿｽｿｽXｿｽｿｽｿｽｿｽｿｽ^ｿｽ[ｿｽｿｽ

			return &node;

		}

	}



	// ｿｽｿｽｿｽﾂゑｿｽｿｽｿｽﾈゑｿｽｿｽｿｽｿｽｿｽ

	return nullptr;

}



// ｿｽmｿｽ[ｿｽhｿｽCｿｽｿｽｿｽfｿｽbｿｽNｿｽXｿｽ謫ｾ

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



	// ｿｽﾊ置

	for (size_t index = 0; index < nodeAnim.positionKeyframes.size() - 1; ++index)

	{

		// ｿｽｿｽｿｽﾝの趣ｿｽｿｽﾔゑｿｽｿｽﾇのキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ間にゑｿｽｿｽ驍ｩｿｽｿｽｿｽ閧ｷｿｽｿｽ

		const VectorKeyframe& keyframe0 = nodeAnim.positionKeyframes.at(index);

		const VectorKeyframe& keyframe1 = nodeAnim.positionKeyframes.at(index + 1);

		if (time >= keyframe0.seconds && time <= keyframe1.seconds)

		{

			// ｿｽﾄ撰ｿｽｿｽｿｽｿｽﾔとキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ趣ｿｽｿｽﾔゑｿｽｿｽｿｽ竓ｮｿｽｿｽｿｽｿｽｿｽZｿｽoｿｽｿｽｿｽｿｽ

			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);



			// ｿｽOｿｽﾌキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾆ趣ｿｽｿｽﾌキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ姿ｿｽｿｽｿｽｿｽ竓ｮ

			DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&keyframe0.value);

			DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&keyframe1.value);

			DirectX::XMVECTOR V = DirectX::XMVectorLerp(V0, V1, rate);

			// ｿｽvｿｽZｿｽｿｽｿｽﾊゑｿｽｿｽmｿｽ[ｿｽhｿｽﾉ格ｿｽ[

			DirectX::XMStoreFloat3(&nodePose.position, V);

		}

	}

	// ｿｽｿｽ]

	for (size_t index = 0; index < nodeAnim.rotationKeyframes.size() - 1; ++index)

	{

		// ｿｽｿｽｿｽﾝの趣ｿｽｿｽﾔゑｿｽｿｽﾇのキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ間にゑｿｽｿｽ驍ｩｿｽｿｽｿｽ閧ｷｿｽｿｽ

		const QuaternionKeyframe& keyframe0 = nodeAnim.rotationKeyframes.at(index);

		const QuaternionKeyframe& keyframe1 = nodeAnim.rotationKeyframes.at(index + 1);

		if (time >= keyframe0.seconds && time <= keyframe1.seconds)

		{

			// ｿｽﾄ撰ｿｽｿｽｿｽｿｽﾔとキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ趣ｿｽｿｽﾔゑｿｽｿｽｿｽ竓ｮｿｽｿｽｿｽｿｽｿｽZｿｽoｿｽｿｽｿｽｿｽ

			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);



			// ｿｽOｿｽﾌキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾆ趣ｿｽｿｽﾌキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ姿ｿｽｿｽｿｽｿｽ竓ｮ

			DirectX::XMVECTOR Q0 = DirectX::XMLoadFloat4(&keyframe0.value);

			DirectX::XMVECTOR Q1 = DirectX::XMLoadFloat4(&keyframe1.value);

			DirectX::XMVECTOR Q = DirectX::XMQuaternionSlerp(Q0, Q1, rate);

			// ｿｽvｿｽZｿｽｿｽｿｽﾊゑｿｽｿｽmｿｽ[ｿｽhｿｽﾉ格ｿｽ[

			DirectX::XMStoreFloat4(&nodePose.rotation, Q);

		}

	}

	// ｿｽXｿｽPｿｽ[ｿｽｿｽ

	for (size_t index = 0; index < nodeAnim.scaleKeyframes.size() - 1; ++index)

	{

		// ｿｽｿｽｿｽﾝの趣ｿｽｿｽﾔゑｿｽｿｽﾇのキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ間にゑｿｽｿｽ驍ｩｿｽｿｽｿｽ閧ｷｿｽｿｽ

		const VectorKeyframe& keyframe0 = nodeAnim.scaleKeyframes.at(index);

		const VectorKeyframe& keyframe1 = nodeAnim.scaleKeyframes.at(index + 1);

		if (time >= keyframe0.seconds && time <= keyframe1.seconds)

		{

			// ｿｽﾄ撰ｿｽｿｽｿｽｿｽﾔとキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ趣ｿｽｿｽﾔゑｿｽｿｽｿｽ竓ｮｿｽｿｽｿｽｿｽｿｽZｿｽoｿｽｿｽｿｽｿｽ

			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);



			// ｿｽOｿｽﾌキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾆ趣ｿｽｿｽﾌキｿｽ[ｿｽtｿｽｿｽｿｽ[ｿｽｿｽｿｽﾌ姿ｿｽｿｽｿｽｿｽ竓ｮ

			DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&keyframe0.value);

			DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&keyframe1.value);

			DirectX::XMVECTOR V = DirectX::XMVectorLerp(V0, V1, rate);

			// ｿｽvｿｽZｿｽｿｽｿｽﾊゑｿｽｿｽmｿｽ[ｿｽhｿｽﾉ格ｿｽ[

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



// ｿｽAｿｽjｿｽｿｽｿｽ[ｿｽVｿｽｿｽｿｽｿｽｿｽCｿｽｿｽｿｽfｿｽbｿｽNｿｽXｿｽ謫ｾ

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



// ｿｽmｿｽ[ｿｽhｿｽ|ｿｽ[ｿｽYｿｽﾝ抵ｿｽ

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



// ｿｽmｿｽ[ｿｽhｿｽ|ｿｽ[ｿｽYｿｽ謫ｾ

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



void Model::BindBuffers(ICommandList* commandList, int meshIndex)

{

	if (meshes.empty() || meshIndex < 0 || meshIndex >= static_cast<int>(meshes.size())) return;



	const auto& mesh = meshes[meshIndex];



	// ｿｽｿｽｿｽ_ｿｽoｿｽbｿｽtｿｽ@ｿｽﾆイｿｽｿｽｿｽfｿｽbｿｽNｿｽXｿｽoｿｽbｿｽtｿｽ@ｿｽｿｽｿｽZｿｽbｿｽg

	commandList->SetVertexBuffer(0, mesh.vertexBuffer.get(), sizeof(Vertex), 0);

	commandList->SetIndexBuffer(mesh.indexBuffer.get(), IndexFormat::Uint32, 0);



	// ｿｽgｿｽ|ｿｽｿｽｿｽWｿｽ[ｿｽﾝ抵ｿｽｿｽ RHI ｿｽoｿｽRｿｽｿｽ

	commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);

}



// ｿｽｿｽｿｽbｿｽVｿｽｿｽｿｽfｿｽ[ｿｽ^ｿｽﾌポｿｽCｿｽｿｽｿｽ^ｿｽｿｽｿｽ謫ｾ

Model::MeshData Model::GetMeshData(int subsetIndex)

{

	if (subsetIndex < 0 || subsetIndex >= meshes.size())

	{

		return { nullptr, nullptr };

	}



	// constｿｽOｿｽｿｽ (ｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽｿｽUpdateMeshIndicesｿｽﾅ行ｿｽｿｽｿｽｿｽｿｽAｿｽﾇみ趣ｿｽｿｽﾌゑｿｽｿｽﾟに費ｿｽconstｿｽ|ｿｽCｿｽｿｽｿｽ^ｿｽｿｽﾔゑｿｽ)

	return { &meshes[subsetIndex].vertices, &meshes[subsetIndex].indices };

}



// ｿｽCｿｽｿｽｿｽfｿｽbｿｽNｿｽXｿｽoｿｽbｿｽtｿｽ@ｿｽﾌ更ｿｽVｿｽｿｽGPUｿｽoｿｽbｿｽtｿｽ@ｿｽﾌ再撰ｿｽｿｽｿｽ

void Model::UpdateMeshIndices(ID3D11Device* device, int subsetIndex, const std::vector<uint32_t>& newIndices)

{

	if (subsetIndex < 0 || subsetIndex >= meshes.size()) return;



	Mesh& mesh = meshes[subsetIndex];

	mesh.indices = newIndices;



	// IBufferｿｽﾆゑｿｽｿｽﾄ搾ｿｽ闥ｼｿｽｿｽ

	mesh.indexBuffer = std::make_shared<DX11Buffer>(

		device,

		static_cast<uint32_t>(sizeof(uint32_t) * mesh.indices.size()),

		BufferType::Index,

		mesh.indices.data()

	);

}



//ｿｽVｿｽｿｽｿｽAｿｽｿｽｿｽCｿｽY

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



//ｿｽfｿｽVｿｽｿｽｿｽAｿｽｿｽｿｽCｿｽY

void Model::Deserialize(const char* filename)

{

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

		catch (...)

		{

			_ASSERT_EXPR_A(false, "Model deserialize failed.");

		}

	}

	else

	{

		_ASSERT_EXPR_A(false, "Model File not found.");

	}

}




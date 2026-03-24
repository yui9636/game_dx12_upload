#include <fstream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include "System/Misc.h"
#include "AssimpImporter.h"
AssimpImporter::AssimpImporter(const char* filename)
	:filepath(filename)
{
	std::string extension = filepath.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

	if (extension == ".fbx")
	{
		aImporter.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
	}



	uint32_t aFlags = aiProcess_Triangulate
		| aiProcess_JoinIdenticalVertices
		| aiProcess_LimitBoneWeights
		| aiProcess_PopulateArmatureData
		| aiProcess_CalcTangentSpace;

	aScene = aImporter.ReadFile(filename, aFlags);
	_ASSERT_EXPR_A(aScene, "3D Model File not found");

}

void AssimpImporter::LoadMeshes(MeshList& meshes, const NodeList& nodes)
{
	if (!aScene || !aScene->mRootNode) return;
	LoadMeshes(meshes, nodes, aScene->mRootNode);
}


void AssimpImporter::LoadMeshes(MeshList& meshes, const NodeList& nodes, const aiNode* aNode)
{
	for (uint32_t aMeshIndex = 0; aMeshIndex < aNode->mNumMeshes; ++aMeshIndex)
	{

		const aiMesh* aMesh = aScene->mMeshes[aNode->mMeshes[aMeshIndex]];

		Model::Mesh& mesh = meshes.emplace_back();
		mesh.nodeIndex = nodeIndexMap[aNode];
		mesh.vertices.resize(aMesh->mNumVertices);
		mesh.indices.resize(aMesh->mNumFaces * 3);
		mesh.materialIndex = static_cast<int>(aMesh->mMaterialIndex);

		for (uint32_t aVertexIndex = 0; aVertexIndex < aMesh->mNumVertices; ++aVertexIndex)
		{
			Model::Vertex& vertex = mesh.vertices.at(aVertexIndex);
			if (aMesh->HasPositions())
			{
				vertex.position = aiVector3DToXMFLOAT3(aMesh->mVertices[aVertexIndex]);
			}
			if (aMesh->HasTextureCoords(0))
			{
				vertex.texcoord = aiVector3DToXMFLOAT2(aMesh->mTextureCoords[0][aVertexIndex]);
				vertex.texcoord.y = 1.0f - vertex.texcoord.y;
			}
			if (aMesh->HasNormals())
			{
				vertex.normal = aiVector3DToXMFLOAT3(aMesh->mNormals[aVertexIndex]);
			}
			if (aMesh->HasTangentsAndBitangents())
			{
				vertex.tangent = aiVector3DToXMFLOAT3(aMesh->mTangents[aVertexIndex]);
			}
		}

		for (uint32_t aFaceIndex = 0; aFaceIndex < aMesh->mNumFaces; ++aFaceIndex)
		{
			const aiFace& aFace = aMesh->mFaces[aFaceIndex];
			uint32_t index = aFaceIndex * 3;
			mesh.indices[index + 0] = aFace.mIndices[2];
			mesh.indices[index + 1] = aFace.mIndices[1];
			mesh.indices[index + 2] = aFace.mIndices[0];
		}

		if (aMesh->mNumBones > 0)
		{
			struct BoneInfluence
			{
				uint32_t	indices[4] = { 0, 0, 0, 0 };
				float		weights[4] = { 1, 0, 0, 0 };
				int			useCount = 0;

				void Add(uint32_t index, float weight)
				{
					if (useCount >= 4) return;
					for (int i = 0; i < useCount; ++i)
					{
						if (indices[i] == index)
						{
							return;
						}
					}
					indices[useCount] = index;
					weights[useCount] = weight;
					useCount++;
				}
			};
			std::vector<BoneInfluence> boneInfluences;
			boneInfluences.resize(aMesh->mNumVertices);

			for (uint32_t aBoneIndex = 0; aBoneIndex < aMesh->mNumBones; ++aBoneIndex)
			{
				const aiBone* aBone = aMesh->mBones[aBoneIndex];

				for (uint32_t aWightIndex = 0; aWightIndex < aBone->mNumWeights; ++aWightIndex)
				{
					const aiVertexWeight& aWeight = aBone->mWeights[aWightIndex];
					BoneInfluence& boneInfluence = boneInfluences.at(aWeight.mVertexId);
					boneInfluence.Add(aBoneIndex, aWeight.mWeight);
				}

				Model::Bone& bone = mesh.bones.emplace_back();
				bone.nodeIndex = nodeIndexMap[aBone->mNode];
				bone.offsetTransform = aiMatrix4x4ToXMFLOAT4X4(aBone->mOffsetMatrix);

			}

			for (size_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); ++vertexIndex)
			{
				Model::Vertex& vertex = mesh.vertices.at(vertexIndex);
				BoneInfluence& boneInfluence = boneInfluences.at(vertexIndex);
				vertex.boneWeight.x = boneInfluence.weights[0];
				vertex.boneWeight.y = boneInfluence.weights[1];
				vertex.boneWeight.z = boneInfluence.weights[2];
				vertex.boneWeight.w = boneInfluence.weights[3];
				vertex.boneIndex.x = boneInfluence.indices[0];
				vertex.boneIndex.y = boneInfluence.indices[1];
				vertex.boneIndex.z = boneInfluence.indices[2];
				vertex.boneIndex.w = boneInfluence.indices[3];
			}
		}


	}
	for (uint32_t aNodeIndex = 0; aNodeIndex < aNode->mNumChildren; ++aNodeIndex)
	{
		LoadMeshes(meshes, nodes, aNode->mChildren[aNodeIndex]);
	}

}

void AssimpImporter::LoadMaterials(MaterialList& materials)
{
	if (!aScene) return;

	std::filesystem::path dirpath(filepath.parent_path());

	materials.resize(aScene->mNumMaterials);
	for (uint32_t aMaterialIndex = 0; aMaterialIndex < aScene->mNumMaterials; ++aMaterialIndex)
	{
		const aiMaterial* aMaterial = aScene->mMaterials[aMaterialIndex];
		Model::Material& material = materials.at(aMaterialIndex);

		aiString aMaterialName;
		aMaterial->Get(AI_MATKEY_NAME, aMaterialName);
		material.name = aMaterialName.C_Str();

		aiColor3D aDiffuseColor;
		if (AI_SUCCESS == aMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aDiffuseColor))
		{
			material.color = aiColor3DToXMFLOAT4(aDiffuseColor);
		}

		auto loadTexture = [&](aiTextureType aTextureType, std::string& textureFilename)
			{
				aiString aTextureFilePath;
				if (AI_SUCCESS == aMaterial->GetTexture(aTextureType, 0, &aTextureFilePath))
				{
					const aiTexture* aTexture = aScene->GetEmbeddedTexture(aTextureFilePath.C_Str());
					if (aTexture != nullptr)
					{
						std::filesystem::path textureFilePath(aTextureFilePath.C_Str());
						if (textureFilePath == "*0")
						{
							textureFilePath = material.name + "_" + aiTextureTypeToString(aTextureType) + "." + aTexture->achFormatHint;
						}
						textureFilePath = "Textures" / textureFilePath.filename();

						std::filesystem::path outputDirPath(dirpath / textureFilePath.parent_path());
						if (!std::filesystem::exists(outputDirPath))
						{
							std::filesystem::create_directories(outputDirPath);
						}
						std::filesystem::path outputFilePath(dirpath / textureFilePath);
						if (!std::filesystem::exists(outputFilePath))
						{
							if (aTexture->mHeight == 0)
							{
								std::ofstream os(outputFilePath.string().c_str(), std::ios::binary);
								os.write(reinterpret_cast<char*>(aTexture->pcData), aTexture->mWidth);
							}
							else
							{
								outputFilePath.replace_extension(".png");
								stbi_write_png(
									outputFilePath.string().c_str(),
									static_cast<int>(aTexture->mWidth),
									static_cast<int>(aTexture->mHeight),
									static_cast<int>(sizeof(uint32_t)),
									aTexture->pcData, 0);
							}
						}
						textureFilename = textureFilePath.string();
					}
					else
					{
						textureFilename = aTextureFilePath.C_Str();
					}
				}
			};

		loadTexture(aiTextureType_DIFFUSE, material.diffuseTextureFileName);
		loadTexture(aiTextureType_NORMALS, material.normalTextureFileName);


		loadTexture(aiTextureType_METALNESS, material.metallicTextureFileName);
		float metallic = 0.0f;
		if (AI_SUCCESS == aMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metallic)) {
			material.metallicFactor = metallic;
		}

		loadTexture(aiTextureType_DIFFUSE_ROUGHNESS, material.roughnessTextureFileName);
		float roughness = 0.0f;
		if (AI_SUCCESS == aMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness)) {
			material.roughnessFactor = roughness;
		}

	

		loadTexture(aiTextureType_BASE_COLOR, material.albedoTextureFileName);
		if (material.albedoTextureFileName.empty()) {
			loadTexture(aiTextureType_DIFFUSE, material.albedoTextureFileName);
		}
		aiColor3D color;
		if (AI_SUCCESS == aMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
			material.color = aiColor3DToXMFLOAT4(color);
		}


		loadTexture(aiTextureType_LIGHTMAP, material.occlusionTextureFileName);

		float occlusionStrength;
		if (aMaterial->Get(AI_MATKEY_OPACITY, occlusionStrength) == AI_SUCCESS)
		{
			material.occlusionStrength = occlusionStrength;
		}


		loadTexture(aiTextureType_EMISSIVE, material.emissiveTextureFileName);




	}
}

void AssimpImporter::LoadNodes(NodeList& nodes)
{
	if (!aScene || !aScene->mRootNode) return;
	LoadNodes(nodes, aScene->mRootNode, -1);
}


void AssimpImporter::LoadNodes(NodeList& nodes, const aiNode* aNode, int parentIndex)
{
	std::map<const aiNode*, int>::iterator it = nodeIndexMap.find(aNode);
	if (it == nodeIndexMap.end())
	{
		nodeIndexMap[aNode] = static_cast<int>(nodes.size());
	}

	aiVector3D aScale, aPosition;
	aiQuaternion aRotation;
	aNode->mTransformation.Decompose(aScale, aRotation, aPosition);

	Model::Node& node = nodes.emplace_back();
	node.name = aNode->mName.C_Str();
	node.parentIndex = parentIndex;
	node.scale = aiVector3DToXMFLOAT3(aScale);
	node.rotation = aiQuaternionToXMFLOAT4(aRotation);
	node.position = aiVector3DToXMFLOAT3(aPosition);

	parentIndex = static_cast<int>(nodes.size() - 1);

	for (uint32_t aNodeIndex = 0; aNodeIndex < aNode->mNumChildren; ++aNodeIndex)
	{
		LoadNodes(nodes, aNode->mChildren[aNodeIndex], parentIndex);
	}
}

int AssimpImporter::GetNodeIndex(const NodeList& nodes, const char* name)
{
	int index = 0;
	for (const Model::Node& node : nodes)
	{
		if (node.name == name)
		{
			return index;
		}
		index++;
	}
	return -1;
}

void AssimpImporter::LoadAnimations(AnimationList& animations, const NodeList& nodes)
{
	if (!aScene) return;
	for (uint32_t aAnimationIndex = 0; aAnimationIndex < aScene->mNumAnimations; ++aAnimationIndex)
	{
		const aiAnimation* aAnimation = aScene->mAnimations[aAnimationIndex];
		Model::Animation& animation = animations.emplace_back();

		animation.name = aAnimation->mName.C_Str();
		animation.secondsLength = static_cast<float>(aAnimation->mDuration / aAnimation->mTicksPerSecond);

		animation.nodeAnims.resize(nodes.size());
		for (uint32_t aChannelIndex = 0; aChannelIndex < aAnimation->mNumChannels; ++aChannelIndex)
		{
			const aiNodeAnim* aNodeAnim = aAnimation->mChannels[aChannelIndex];
			int nodeIndex = GetNodeIndex(nodes, aNodeAnim->mNodeName.C_Str());
			if (nodeIndex < 0) continue;

			const Model::Node& node = nodes.at(nodeIndex);
			Model::NodeAnim& nodeAnim = animation.nodeAnims.at(nodeIndex);

			for (uint32_t aPositionIndex = 0; aPositionIndex < aNodeAnim->mNumPositionKeys; ++aPositionIndex)
			{
				const aiVectorKey& aKey = aNodeAnim->mPositionKeys[aPositionIndex];
				if (fabs(std::round(aKey.mTime) - aKey.mTime) > 0.001) continue;
				Model::VectorKeyframe& keyframe = nodeAnim.positionKeyframes.emplace_back();
				keyframe.seconds = static_cast<float>(aKey.mTime / aAnimation->mTicksPerSecond);
				keyframe.value = aiVector3DToXMFLOAT3(aKey.mValue);
			}
			for (uint32_t aRotationIndex = 0; aRotationIndex < aNodeAnim->mNumRotationKeys; ++aRotationIndex)
			{
				const aiQuatKey& aKey = aNodeAnim->mRotationKeys[aRotationIndex];
				if (fabs(std::round(aKey.mTime) - aKey.mTime) > 0.001) continue;
				Model::QuaternionKeyframe& keyframe = nodeAnim.rotationKeyframes.emplace_back();
				keyframe.seconds = static_cast<float>(aKey.mTime / aAnimation->mTicksPerSecond);
				keyframe.value = aiQuaternionToXMFLOAT4(aKey.mValue);
			}
			for (uint32_t aScalingIndex = 0; aScalingIndex < aNodeAnim->mNumScalingKeys; ++aScalingIndex)
			{
				const aiVectorKey& aKey = aNodeAnim->mScalingKeys[aScalingIndex];
				if (fabs(std::round(aKey.mTime) - aKey.mTime) > 0.001) continue;
				Model::VectorKeyframe& keyframe = nodeAnim.scaleKeyframes.emplace_back();
				keyframe.seconds = static_cast<float>(aKey.mTime / aAnimation->mTicksPerSecond);
				keyframe.value = aiVector3DToXMFLOAT3(aKey.mValue);
			}

			DirectX::XMVECTOR Epsilon = DirectX::XMVectorReplicate(0.00001f);
			{
				bool result = true;
				DirectX::XMVECTOR A = DirectX::XMLoadFloat3(&nodeAnim.positionKeyframes.at(0).value);
				for (size_t i = 1; i < nodeAnim.positionKeyframes.size(); ++i)
				{
					DirectX::XMVECTOR B = DirectX::XMLoadFloat3(&nodeAnim.positionKeyframes.at(i).value);
					if (!DirectX::XMVector3NearEqual(A, B, Epsilon))
					{
						result = false;
						break;
					}
				}
				if (result)
				{
					nodeAnim.positionKeyframes.resize(1);
				}
			}
			{
				bool result = true;
				DirectX::XMVECTOR A = DirectX::XMLoadFloat4(&nodeAnim.rotationKeyframes.at(0).value);
				for (size_t i = 1; i < nodeAnim.rotationKeyframes.size(); ++i)
				{
					DirectX::XMVECTOR B = DirectX::XMLoadFloat4(&nodeAnim.rotationKeyframes.at(i).value);
					if (!DirectX::XMVector4NearEqual(A, B, Epsilon))
					{
						result = false;
						break;
					}
				}
				if (result)
				{
					nodeAnim.rotationKeyframes.resize(1);
				}
			}
			{
				bool result = true;
				DirectX::XMVECTOR A = DirectX::XMLoadFloat3(&nodeAnim.scaleKeyframes.at(0).value);
				for (size_t i = 1; i < nodeAnim.scaleKeyframes.size(); ++i)
				{
					DirectX::XMVECTOR B = DirectX::XMLoadFloat3(&nodeAnim.scaleKeyframes.at(i).value);
					if (!DirectX::XMVector3NearEqual(A, B, Epsilon))
					{
						result = false;
						break;
					}
				}
				if (result)
				{
					nodeAnim.scaleKeyframes.resize(1);
				}
			}
		}
		for (size_t nodeIndex = 0; nodeIndex < animation.nodeAnims.size(); ++nodeIndex)
		{
			const Model::Node& node = nodes.at(nodeIndex);
			Model::NodeAnim& nodeAnim = animation.nodeAnims.at(nodeIndex);
			if (nodeAnim.positionKeyframes.size() == 0)
			{
				Model::VectorKeyframe& keyframe = nodeAnim.positionKeyframes.emplace_back();
				keyframe.seconds = 0.0f;
				keyframe.value = node.position;
			}
			if (nodeAnim.positionKeyframes.size() == 1)
			{
				Model::VectorKeyframe& keyframe = nodeAnim.positionKeyframes.emplace_back();
				keyframe.seconds = animation.secondsLength;
				keyframe.value = nodeAnim.positionKeyframes.at(0).value;
			}
			if (nodeAnim.rotationKeyframes.size() == 0)
			{
				Model::QuaternionKeyframe& keyframe = nodeAnim.rotationKeyframes.emplace_back();
				keyframe.seconds = 0.0f;
				keyframe.value = node.rotation;
			}
			if (nodeAnim.rotationKeyframes.size() == 1)
			{
				Model::QuaternionKeyframe& keyframe = nodeAnim.rotationKeyframes.emplace_back();
				keyframe.seconds = animation.secondsLength;
				keyframe.value = nodeAnim.rotationKeyframes.at(0).value;
			}
			if (nodeAnim.scaleKeyframes.size() == 0)
			{
				Model::VectorKeyframe& keyframe = nodeAnim.scaleKeyframes.emplace_back();
				keyframe.seconds = 0.0f;
				keyframe.value = node.scale;
			}
			if (nodeAnim.scaleKeyframes.size() == 1)
			{
				Model::VectorKeyframe& keyframe = nodeAnim.scaleKeyframes.emplace_back();
				keyframe.seconds = animation.secondsLength;
				keyframe.value = nodeAnim.scaleKeyframes.at(0).value;
			}
		}
	}
}







DirectX::XMFLOAT3 AssimpImporter::aiVector3DToXMFLOAT3(const aiVector3D& aValue)
{
	return DirectX::XMFLOAT3(
	static_cast<float>(aValue.x),
	static_cast<float>(aValue.y),
	static_cast<float>(aValue.z)
);
}


DirectX::XMFLOAT4 AssimpImporter::aiColor3DToXMFLOAT4(const aiColor3D& aValue)
{
	return DirectX::XMFLOAT4(
	static_cast<float>(aValue.r),
	static_cast<float>(aValue.g),
	static_cast<float>(aValue.b),
	1.0f
	);
}


DirectX::XMFLOAT2 AssimpImporter::aiVector3DToXMFLOAT2(const aiVector3D& aValue)
{
	return DirectX::XMFLOAT2(
	static_cast<float>(aValue.x),
	static_cast<float>(aValue.y)
	);
}

DirectX::XMFLOAT4 AssimpImporter::aiQuaternionToXMFLOAT4(const aiQuaternion& aValue)
{
	return DirectX::XMFLOAT4(
		static_cast<float>(aValue.x),
		static_cast<float>(aValue.y),
		static_cast<float>(aValue.z),
		static_cast<float>(aValue.w)
	);
}



DirectX::XMFLOAT4X4 AssimpImporter::aiMatrix4x4ToXMFLOAT4X4(const aiMatrix4x4& aValue)
{
	return DirectX::XMFLOAT4X4(
		static_cast<float>(aValue.a1),
		static_cast<float>(aValue.b1),
		static_cast<float>(aValue.c1),
		static_cast<float>(aValue.d1),
		static_cast<float>(aValue.a2),
		static_cast<float>(aValue.b2),
		static_cast<float>(aValue.c2),
		static_cast<float>(aValue.d2),
		static_cast<float>(aValue.a3),
		static_cast<float>(aValue.b3),
		static_cast<float>(aValue.c3),
		static_cast<float>(aValue.d3),
		static_cast<float>(aValue.a4),
		static_cast<float>(aValue.b4),
		static_cast<float>(aValue.c4),
		static_cast<float>(aValue.d4)
	);
}



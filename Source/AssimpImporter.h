
#pragma once

#include <map>
#include <filesystem>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include "Model/Model.h"

class AssimpImporter
{
private:
	using MeshList = std::vector<Model::Mesh>;
	using MaterialList = std::vector<Model::Material>;
	using NodeList = std::vector<Model::Node>;
	using AnimationList = std::vector<Model::Animation>;
	//using GeometryList = std::vector<Model::Geometry>;
public:
	AssimpImporter(const char* filename);


	void LoadMeshes(MeshList& meshes,const NodeList&nodes);

	void LoadMaterials(MaterialList& materials);
	
	void LoadNodes(NodeList& nodes);

	void LoadAnimations(AnimationList& animations, const NodeList& nodes);


private:
	static DirectX::XMFLOAT3 AssimpImporter::aiVector3DToXMFLOAT3(const aiVector3D& aValue);
	static DirectX::XMFLOAT4 AssimpImporter::aiColor3DToXMFLOAT4(const aiColor3D& aValue);
	static DirectX::XMFLOAT2 AssimpImporter::aiVector3DToXMFLOAT2(const aiVector3D& aValue);
	static DirectX::XMFLOAT4 aiQuaternionToXMFLOAT4(const aiQuaternion& aValue);
	static DirectX::XMFLOAT4X4 aiMatrix4x4ToXMFLOAT4X4(const aiMatrix4x4& aValue);

	static int GetNodeIndex(const NodeList& nodes, const char* name);


	void LoadNodes(NodeList& nodes, const aiNode* aNode, int parentIndex);
	void LoadMeshes(MeshList& meshes, const NodeList& nodes, const aiNode* aNode);


private:
	std::filesystem::path			filepath;
	std::map<const aiNode*, int>	nodeIndexMap;
	Assimp::Importer				aImporter;
	const aiScene* aScene = nullptr;
};

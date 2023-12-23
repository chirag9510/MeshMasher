﻿// MeshMasher.h : Include file for standard system include files,
// or project specific include files.
#pragma once
#include "CQueue.h"

class CQueue;

class MeshMasher{
public:
	MeshMasher();
	void run();
	void loadMaterial(const aiMaterial* aiMat, Material& meshMat);
	void loadTexture(Material& mat, const aiMaterial* aiMat, const aiTextureType textureType, const int stbVersion);
	void loadMesh(const aiMesh* aimesh, Mesh& mesh);
	void writeLoaderData();
	void writeVBufferData();
	void writeEBufferData();
	void writeMaterialData();
	void writeTextureData();

private:
	CQueue cqueue;
	unsigned int currBaseInstance;
	std::map<std::string, unsigned int> modelBaseInstances;
	std::map<MaterialType, std::vector<Mesh>> meshes;										// opaque material meshes are always last to render
	std::map<std::string, std::vector<Material>> materials;									// get material using model name as key for each mesh
	std::map<std::string, Texture> textures;												// use texture filename to access texture

	size_t sizeVbf, sizeEbf, primCount;														// size in bytes of data to be read by geometry loaders

};

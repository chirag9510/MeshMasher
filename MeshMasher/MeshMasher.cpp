// MeshMasher.cpp : Defines the entry point for the application.
//
#include "MeshMasher.h"
#include <fstream> 
#include <iostream>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <latch>
#include <memory>
#include <thread>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "meshoptimizer.h"

MeshMasher::MeshMasher(Settings settings) : settings(settings),  currBaseInstance(0), sizeEbf(0), sizeVbf(0), primCount(0) {}

void MeshMasher::run() {
	std::ifstream fileContents("contents.txt", std::ios::in);
	if (!fileContents.good()) {
		std::cout << "Error: Failed to open contexts.txt file. Closing application." << std::endl;
		return;
	}

	// init worker threads with thier func
	std::unique_ptr<std::latch> latchThreads;
	std::vector<std::jthread> threads;
	for (int i = 0; i < settings.numWorkerThreads; i++) {
		threads.emplace_back([&]() {
			while (true) {
				Command* com = cqueue.pop();
				com->execute();
				delete com;
				latchThreads->count_down();
			}
			});
	};

	// Read each file name and start assigning threads work to store all that data into model struct vectors and other members
	std::string modelName;
	while (std::getline(fileContents, modelName)) {
		Assimp::Importer importer;
		
		int processPreset = settings.preTransformVertices ? 
			aiProcessPreset_TargetRealtime_Quality | aiProcess_PreTransformVertices :
			aiProcessPreset_TargetRealtime_Quality;

		const auto scene = importer.ReadFile(("input/" + modelName).c_str(), processPreset);
		if (scene != nullptr) {

			//remove extension (.obj, gltf) from filename to get modelname
			modelName = modelName.substr(0, modelName.find('.'));
			modelBaseInstances[modelName] = currBaseInstance++;
			std::cout << "-------------------------------\n" << modelName << std::endl;

			// process materials first so that we can identify which meshes are of what type
			materials[modelName].resize(scene->mNumMaterials);
			latchThreads = std::make_unique<std::latch>(scene->mNumMaterials);
			for (unsigned int i = 0; i < scene->mNumMaterials; i++)
				cqueue.push(new CModelInt(this, &MeshMasher::loadMaterial, scene->mMaterials[i], materials[modelName][i]));
			latchThreads->wait();
			latchThreads.reset();
			std::cout << "Materials processed." << std::endl;

			// process meshes
			latchThreads = std::make_unique<std::latch>(scene->mNumMeshes);

			// reserve space for meshes which can then be copied to the member "meshes" std::map
			std::map<MaterialType, std::vector<Mesh>> tempMeshes;
			std::map<MaterialType, unsigned int> perMatIndex;														//num meshes per material. Used for generating indexes
			for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
				auto matType = materials[modelName][scene->mMeshes[i]->mMaterialIndex].type;
				tempMeshes[matType].emplace_back(Mesh(modelName));
			}

			// now send them to threads to load up data in order, mantain the original order of meshes
			for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
				auto matType = materials[modelName][scene->mMeshes[i]->mMaterialIndex].type;
				cqueue.push(new CAIMeshMesh(this, &MeshMasher::loadMesh, scene->mMeshes[i], tempMeshes[matType][perMatIndex[matType]++]));
			}
			latchThreads->wait();
			latchThreads.reset();

			// now combine it to the member "meshes" std::map
			for (auto& m : tempMeshes) {
				meshes[m.first].insert(meshes[m.first].end(), m.second.begin(), m.second.end());
			}

			std::cout << "Meshes processed." << std::endl;

		}
		else {
			std::cout << "Error: '" << modelName << "' not found. Skipping......." << std::endl;
		}
	}

	std::cout << "\n-------------------------------\n";
	std::cout << "Finished mashing all meshes, writing to files...." << std::endl;
	
	//start writing to files
	latchThreads = std::make_unique<std::latch>(4);
	cqueue.push(new CVoid(this, &MeshMasher::writeVBufferData));
	cqueue.push(new CVoid(this, &MeshMasher::writeEBufferData));
	cqueue.push(new CVoid(this, &MeshMasher::writeMaterialData));
	cqueue.push(new CVoid(this, &MeshMasher::writeTextureData));
	latchThreads->wait();

	// must come after writing other files 
	writeLoaderData();

	std::cout << "Finished writing to files. You can close this application now...." << std::endl;
}

void MeshMasher::loadMaterial(const aiMaterial* aiMat, Material& meshMat) {
	aiString aistr;
	if (aiMat->Get(AI_MATKEY_BLEND_FUNC, aistr) == aiReturn_SUCCESS && aistr.C_Str() == "BLEND") {
		meshMat.type = MaterialType::Opa;
		if (aiMat->GetTexture(aiTextureType_OPACITY, 0, &aistr) == aiReturn_SUCCESS) {
			meshMat.useAlphaTex = true;
			meshMat.useDiffuseAlpha = false;
			loadTexture(meshMat, aiMat, aiTextureType_OPACITY, STBI_rgb);
		}
		else if (aiMat->Get(AI_MATKEY_OPACITY, meshMat.opacity) == aiReturn_SUCCESS && meshMat.opacity != 1.f)
			meshMat.useDiffuseAlpha = false;
		else {
			loadTexture(meshMat, aiMat, aiTextureType_DIFFUSE, STBI_rgb);
		}
	}
	else {
		meshMat.type = MaterialType::Tex;
		loadTexture(meshMat, aiMat, aiTextureType_DIFFUSE, STBI_rgb);
		loadTexture(meshMat, aiMat, aiTextureType_NORMALS, STBI_rgb);
		loadTexture(meshMat, aiMat, aiTextureType_UNKNOWN, STBI_rgb);
	}

	if (aiMat->GetTexture(aiTextureType_EMISSIVE, 0, &aistr) == aiReturn_SUCCESS) {
		loadTexture(meshMat, aiMat, aiTextureType_EMISSIVE, STBI_rgb);
	}
}

void MeshMasher::loadTexture(Material& mat, const aiMaterial* aiMat, const aiTextureType textureType, const int stbVersion) {
	aiString aistr;
	if (aiMat->GetTexture(textureType, 0, &aistr) == aiReturn_SUCCESS) {
		mat.textureNames[textureType] = aistr.C_Str();

		if (textures.find(aistr.C_Str()) == textures.end()) {
			Texture& texture = textures[aistr.C_Str()];
			texture.type = textureType;
			texture.name = aistr.C_Str();
			texture.rgbType = stbVersion;
			texture.data = stbi_load(("input/" + std::string(aistr.C_Str())).c_str(), &texture.width, &texture.height, &texture.nrChannels, stbVersion);
		
			if (texture.data == nullptr)
				std::cerr << "Error: Texture of type " << textureType << " at location " << ("input" + std::string(aistr.C_Str())) << " not found." << std::endl;
		}
	}
}

void MeshMasher::loadMesh(const aiMesh* aimesh, Mesh& mesh) {
	mesh.materialIndex = aimesh->mMaterialIndex;

	if (settings.useMeshOptimizer) {
		// run through meshoptimizer
		// each vertex v/t/n with size float * (3 + 2 + 3) 
		std::vector<Vertex> unindexedVertices;
		unindexedVertices.reserve(aimesh->mNumVertices * 8);
		for (unsigned int v = 0; v < aimesh->mNumVertices; v++) {
			unindexedVertices.emplace_back(aimesh->mVertices[v], aiVector2D(aimesh->mTextureCoords[0][v].x, aimesh->mTextureCoords[0][v].y), aimesh->mNormals[v]);
		}

		std::vector<unsigned int> srcIndices;
		srcIndices.reserve(aimesh->mNumFaces * 3);
		for (unsigned int f = 0; f < aimesh->mNumFaces; f++) {
			srcIndices.emplace_back(aimesh->mFaces[f].mIndices[0]);
			srcIndices.emplace_back(aimesh->mFaces[f].mIndices[1]);
			srcIndices.emplace_back(aimesh->mFaces[f].mIndices[2]);
		}

		mesh.indices.resize(srcIndices.size());
		mesh.vertices.resize(unindexedVertices.size());

		//meshoptimizer
		size_t sizeVertex = sizeof(float) * 8;
		std::vector<unsigned int> remap(srcIndices.size());
		auto vertexCount = meshopt_generateVertexRemap(&remap[0], &srcIndices[0], srcIndices.size(), &unindexedVertices[0], srcIndices.size(), sizeVertex);
		meshopt_remapIndexBuffer(&mesh.indices[0], &srcIndices[0], srcIndices.size(), &remap[0]);
		meshopt_remapVertexBuffer(&mesh.vertices[0], &unindexedVertices[0], srcIndices.size(), sizeVertex, &remap[0]);
		meshopt_optimizeVertexCache(&mesh.indices[0], &mesh.indices[0], mesh.indices.size(), vertexCount);
		meshopt_optimizeOverdraw(&mesh.indices[0], &mesh.indices[0], mesh.indices.size(), &mesh.vertices[0].pos.x, vertexCount, sizeVertex, 1.05f);
		meshopt_optimizeVertexFetch(&mesh.vertices[0], &mesh.indices[0], mesh.indices.size(), &mesh.vertices[0], vertexCount, sizeVertex);
	}
	else {
		mesh.vertices.reserve(aimesh->mNumVertices * 8);
		for (unsigned int v = 0; v < aimesh->mNumVertices; v++) {
			mesh.vertices.emplace_back(aimesh->mVertices[v], aiVector2D(aimesh->mTextureCoords[0][v].x, aimesh->mTextureCoords[0][v].y), aimesh->mNormals[v]);
		}

		mesh.indices.reserve(aimesh->mNumFaces * 3);
		for (unsigned int f = 0; f < aimesh->mNumFaces; f++) {
			mesh.indices.emplace_back(aimesh->mFaces[f].mIndices[0]);
			mesh.indices.emplace_back(aimesh->mFaces[f].mIndices[1]);
			mesh.indices.emplace_back(aimesh->mFaces[f].mIndices[2]);
		}
	}
	
}

void MeshMasher::writeLoaderData() {
	// write about meshes
	std::ofstream ofile("output/dat.ldr", std::fstream::out | std::fstream::binary);
	if (ofile.is_open()) {
		unsigned int baseVertex = 0, firstIndex = 0;
		
		// write the size of data in raw bytes to be read from other files by loaders like size of dat.vbf / dat.ebf files
		// also primCount of all the total number of meshes to be rendered
		ofile << sizeVbf << " " << sizeEbf << " " << primCount << std::endl;

		// opaque materials need to be last and this order must match in other writefunx()
		std::vector<MaterialType> matTypes{ MaterialType::Tex, MaterialType::Opa };
		for (auto it = matTypes.begin(); it != matTypes.end(); it++) {
			for (auto& m : meshes[*it])
			{
				ofile << m.modelName << " "
					<< m.materialIndex << " "
					<< m.indices.size() << " "						//count
					<< baseVertex << " "
					<< firstIndex << " "
					<< modelBaseInstances[m.modelName] << " "		//baseInstance
					<< std::endl;
				
				//baseVertex += (m.vertices.size()) / 8;				//each vertex is 8 v/t/n
				baseVertex += m.vertices.size();
				firstIndex += m.indices.size();
			}
		}
		ofile.flush();
	}
	else
		std::cout << "Error: " << "ldr file failed on creation." << std::endl;

}

void MeshMasher::writeVBufferData() {
	//write vertex buffer dat
	std::ofstream ofile("output/dat.vbf", std::fstream::out | std::fstream::binary);
	if (ofile.is_open()) {
		size_t sizeVertices = 0;

		// opaque materials need to be last and this order must match in other writefunx()
		std::vector<MaterialType> matTypes{ MaterialType::Tex, MaterialType::Opa };
		for (auto it = matTypes.begin(); it != matTypes.end(); it++) {
			// primCount for indirect draw
			primCount += meshes[*it].size();

			for (auto& m : meshes[*it]){
				sizeVertices = sizeof(Vertex) * m.vertices.size();
				//sizeVertices = sizeof(float) * m.vertices.size();
				ofile.write(reinterpret_cast<char*>(&m.vertices[0]), sizeVertices);
				sizeVbf += sizeVertices;
			}
		}
		ofile.flush();
	}
	else
		std::cout << "Error: " << "vbf file failed on creation." << std::endl;

}

void MeshMasher::writeEBufferData() {
	//write elements buffer dat
	std::ofstream ofile("output/dat.ebf", std::fstream::out | std::fstream::binary);
	if (ofile.is_open()) {
		size_t sizeEle = 0;

		// opaque materials need to be last and this order must match in other writefunx()
		std::vector<MaterialType> matTypes{ MaterialType::Tex, MaterialType::Opa };
		for (auto it = matTypes.begin(); it != matTypes.end(); it++) {
			for (auto& m : meshes[*it])
			{
				sizeEle = sizeof(unsigned int) * m.indices.size();
				ofile.write(reinterpret_cast<char*>(m.indices.data()), sizeEle);
				sizeEbf += sizeEle;
			}
		}
		ofile.flush();
	} 
	else 
		std::cout << "Error: " << "ebf file failed on creation." << std::endl;
}

void MeshMasher::writeMaterialData() {
	// NOTE: for now only exporting albedo texture. will export other texture and material types later
	std::ofstream ofile("output/dat.mtr", std::fstream::out | std::fstream::binary);
	if (ofile.is_open()) {
		for (auto it = materials.begin(); it != materials.end(); it++) {
			// write num materials for each model type first
			ofile << it->first << " " << it->second.size() << std::endl;
			
			//write material properties, but writing albedo texture only for now
			for (auto itMat = it->second.begin(); itMat != it->second.end(); itMat++) {
				ofile << itMat->textureNames[aiTextureType_DIFFUSE] << std::endl;
			}
		}

		ofile.flush();
	}
	else
		std::cout << "Error: " << "mtr file failed on creation." << std::endl;

}

void MeshMasher::writeTextureData() {
	std::ofstream ofileRgb("output/dat.rgb", std::fstream::out | std::fstream::binary);				//store raw binary texture image data, GL_RGB interal format based name 
	std::ofstream ofileTxr("output/dat.txr", std::fstream::out);									//store properties of the texture 
	if (ofileTxr.is_open() && ofileRgb.is_open()) {
		for (auto it = textures.begin(); it != textures.end(); it++) {

			// again for now only working with diffuse texture
			if (it->second.type == aiTextureType_DIFFUSE) {
				size_t sizeData = strlen(reinterpret_cast<char*>(it->second.data));
				ofileTxr << it->first << " " << it->second.width << " " << it->second.height << " " << sizeData << std::endl;
				ofileRgb.write(reinterpret_cast<char*>(it->second.data), sizeData);
				stbi_image_free(it->second.data);
			}
		}

		ofileTxr.flush();
		ofileRgb.flush();
	}
	else
		std::cerr << "Error: " << "txr & img file failed on creation." << std::endl;
}

void DisplayInvalidArgsMsg() {
	std::cerr << "Error: Invalid arguments. Arguments should be in the following format:\n";
	std::cerr << "meshmasher.exe -wt <numWorkerThreads> -ptv <bool 0 / 1> -mo <bool 0 / 1>\n";
	std::cerr << "-wt = number of worker threads (1 to 6, default 2)\n";
	std::cerr << "-ptv = pre transform vertices (aiProcess_PreTransformVertices flag, default 1)\n";
	std::cerr << "-mo = Use meshoptimizer lib (0 / 1, default 1)\n";
}

int main(int argc, char** argv) {
	// args = meshmasher.exe -wt <numWorkerThreads> -ptv <bool 0, 1> -mo <bool 0, 1>
	Settings settings;
	if (argc == 7) {
		if ((strcmp(argv[1], "-wt") == 0) &&
			(std::stoi(argv[2]) >= 1 && std::stoi(argv[2]) <= 6) &&
			(strcmp(argv[3],"-ptv") == 0) &&
			(std::stoi(argv[4]) <= 1) &&
			(strcmp(argv[5], "-mo") == 0) &&
			(std::stoi(argv[6]) <= 1)) {
			settings.numWorkerThreads = std::stoi(argv[2]);
			settings.preTransformVertices = std::stoi(argv[4]);
			settings.useMeshOptimizer = std::stoi(argv[6]);
		}
		else  {
			DisplayInvalidArgsMsg();
			return 1;
		} 
	}
	else if (argc != 1) {
		DisplayInvalidArgsMsg();
		return 1;
	}

	std::cout << std::boolalpha << "-----****************-----\nMeshMasher Settings :-\nNum Worker Threads : " << settings.numWorkerThreads <<
		"\nPre Transform Vertices : " << settings.preTransformVertices <<
		"\nUse MeshOptimizer Lib : " << settings.useMeshOptimizer << "\n//chirag\n------****************------\n";

	MeshMasher masher(settings);
	masher.run();	
	return 0;
}

//chirag 2023


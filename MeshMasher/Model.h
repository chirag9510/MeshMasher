#pragma once
#include <vector>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <string>
#include <map>

struct Texture {
	aiTextureType type;																	//directly using assimp types
	std::string name;
	unsigned char* data;
	int width, height, nrChannels, rgbType;												// either GL_RGB8 or GL_RGB8_ALPHA8 for diffuse maps with opacity values based on STBI_rgb/STBI_rgba
	Texture() : width(0), height(0), data(nullptr) {}
};

enum class MaterialType {
	Tex,
	Opa
};

struct Material {
	float opacity;
	float color[4];
	MaterialType type;
	std::map<aiTextureType, std::string> textureNames;
	bool useAlphaTex;
	bool useDiffuseAlpha;																// true means using diffuse texture w parameter for opacity value, otherwise use opacity variable with color
	Material() : useAlphaTex(false), useDiffuseAlpha(true) {}
};

struct Mesh {
	std::vector<float> vertices;
	std::vector<unsigned int> indices;
	unsigned int materialIndex;
	std::string modelName;																//parent model filename used to identify material from maps as key
	Mesh() = default;
	Mesh(std::string modelName) : modelName(modelName) {}
};

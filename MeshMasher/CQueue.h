#pragma once
#include <queue>
#include <condition_variable>
#include <mutex>
#include <iostream>

#include "Model.h"

struct Model;
class MeshMasher;

// Task are all about function pointers, parameters and return values thats it
// Two different functions but with same parameters and return values will initialize the same Task derived class
class Command {
public:
	virtual ~Command() = default;
	virtual void execute() = 0;
};

class CModelInt : public Command {
public:
	CModelInt(MeshMasher* meshMasher, void(MeshMasher::* action)(const aiMaterial*, Material&), const aiMaterial* aiMat, Material& meshMat) :
		meshMasher(meshMasher), action(action), aiMat(aiMat), meshMat(meshMat) {}

	void execute() override { (meshMasher->*action)(aiMat, meshMat); }

private:
	MeshMasher* meshMasher;
	void (MeshMasher::* action)(const aiMaterial*, Material&);
	const aiMaterial* aiMat;
	Material& meshMat;
};

class CAIMeshMesh : public Command {
public:
	CAIMeshMesh(MeshMasher* meshMasher, void(MeshMasher::*action)(const aiMesh*, Mesh& mesh), const aiMesh* aimesh, Mesh& mesh) :
		meshMasher(meshMasher), action(action), aimesh(aimesh), mesh(mesh) {}

	void execute() override { (meshMasher->*action)(aimesh, mesh); }

private:
	MeshMasher* meshMasher;
	void (MeshMasher::* action)(const aiMesh*, Mesh& mesh);
	const aiMesh* aimesh;
	Mesh& mesh;
};

class CVoid : public Command {
public:
	CVoid(MeshMasher* meshMasher, void(MeshMasher::* action)()) :
		meshMasher(meshMasher), action(action) {}

	void execute() override { (meshMasher->*action)(); }

private:
	MeshMasher* meshMasher;
	void (MeshMasher::* action)();
};

class CQueue {
public:
	void push(Command* com);
	Command* pop();

private:
	std::queue<Command*> commands;
	std::condition_variable cv;
	std::mutex mut;
};
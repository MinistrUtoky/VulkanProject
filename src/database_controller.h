// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once
#include <types.h>
#include <ctime>
#include <iostream>
#include <fstream>     
#include "filesystem"
#include <sqlite3.h>

class ConverterToSpirv {
public:
	ConverterToSpirv(std::string glslFilePath) {
		this->glslFilePath = glslFilePath;
	}
	void convertShaderToSpirv(std::string sourceFilePath, std::string resultFilePath);
	void convertAllApplicableShaders(std::string sourceFolderPath, std::string resultFolderPath);
private:
	std::string glslFilePath;
	const std::string applicableExtensions[12]{
		".vert",
		".frag",
		".comp",
		".geom",
		".tesc",
		".tese",
		".mesh",
		".task",
		".rgen",
		".rchit",
		".rmiss",
	};
};

class DataController {
private:
	const char* writeModelsFileAdress = "..\\assets\\";
	const char* writeShadersFileAdress = "..\\shaders\\";
	const char* dataAddress = "C:\\Users\\Viqtop\\PycharmProjects\\botstuff2\\data\\graphics_database.db";
public:
	void test();
	void retrieve_model_blobs();
	void retrieve_shader_blobs();
	void retrieve_blobs(const char* writeDirectory, const char* tableName, int filenameColumnIndex, int blobColumnIndex);
	void retrieve_all_blobs();
};
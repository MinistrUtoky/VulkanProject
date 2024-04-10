// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once
#include <types.h>
#include <ctime>
#include <iostream>


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
	const char* gszFile = "C:\\test.db";
public:
	void test();
};
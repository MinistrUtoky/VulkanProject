﻿#pragma once
#include "types.h"
#include "unordered_map"
#include "filesystem"

struct GeoSurface {
	uint32_t startIndex;
	uint32_t count;
};
struct MeshAsset {
	std::string name;
	std::vector<GeoSurface> surfaces;
	GPUMeshBuffers meshBuffers;
};
class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGLTFMeshes(VulkanEngine* vulkanEngine, std::filesystem::path filePath);
#pragma once
#include "types.h"
#include "descriptors.h"
#include "unordered_map"
#include "filesystem"
#include "fastgltf/tools.hpp"

struct GLTFMaterial {
	RenderableMaterial materialData;
};
struct CubicBounds {
	glm::vec3 center;
	float radius;
	glm::vec3 size;
};
struct GeoSurface {
	uint32_t startIndex;
	uint32_t count;
	CubicBounds cubicBounds;
	std::shared_ptr<GLTFMaterial> gltfMaterial;
};
struct MeshAsset {
	std::string name;
	std::vector<GeoSurface> surfaces;
	GPUMeshBuffers meshBuffers;
};
class VulkanEngine;

VkFilter openGL_filter_to_vk_filter(fastgltf::Filter glFilter);
VkSamplerMipmapMode openGL_filter_to_vk_mipmap_mode(fastgltf::Filter glFilter);

struct GLTFSceneInstance : public IRenderable {
public:
	std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshAssets;
	std::unordered_map<std::string, std::shared_ptr<HierarchyNode>> hierarchyNodes;
	std::unordered_map<std::string, AllocatedImage> allocatedImages;
	std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> gltfMaterials;
	std::vector<std::shared_ptr<HierarchyNode>> hierarchyTopNodes;
	std::vector<VkSampler> vulkanSamplers;
	ScalableDescriptorAllocator scalableDescriptorPool;
	AllocatedBuffer materialDataBuffer;
	VulkanEngine* creator;

	~GLTFSceneInstance() { clearScene(); };
	virtual void Draw(const glm::mat4& topMatrix, DrawContext& drawContext);
private:
	void clearScene();
};


std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGLTFMeshes(VulkanEngine* vulkanEngine, std::filesystem::path filePath);

std::optional<std::shared_ptr<GLTFSceneInstance>> loadGLTFScene(VulkanEngine* engine, std::string_view filePath);

std::optional<AllocatedImage> loadTexture(VulkanEngine* vulkanEngine, fastgltf::Asset& gltfAsset, fastgltf::Image& gltfImage);

void loadModel(VulkanEngine* engine, std::string_view filePath);
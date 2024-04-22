#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "iostream"
#include <loader.h>
#include "engine.h"
#include "images.h"
#include "initializers.h"
#include "types.h"
#include "glm/gtx/quaternion.hpp"
#include "fastgltf/glm_element_traits.hpp"
#include "fastgltf/parser.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/util.hpp"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>

#pragma region Loaders
std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGLTFMeshes(VulkanEngine* vulkanEngine, std::filesystem::path filePath) {
	std::cout << "Loading GLTF: " << filePath << std::endl;
	fastgltf::GltfDataBuffer dataBuffer;
	dataBuffer.loadFromFile(filePath);
	constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
	fastgltf::Asset GLTF;
	fastgltf::Parser parser{};
	auto load = parser.loadBinaryGLTF(&dataBuffer, filePath.parent_path(), gltfOptions);
	if (load) {
		GLTF = std::move(load.get());
	}
	else {
		fmt::print("Failed to load glTF: {}\n", fastgltf::to_underlying(load.error()));
		return{};
	}
	std::vector<std::shared_ptr<MeshAsset>> meshes;
	std::vector<uint32_t> indices;
	std::vector<Vertex3D> vertices;
	for (fastgltf::Mesh& mesh : GLTF.meshes) {
		MeshAsset newMesh;
		newMesh.name = mesh.name;
		indices.clear();
		vertices.clear();
		for (auto&& p : mesh.primitives) {
			GeoSurface newSurface;
			newSurface.startIndex = (uint32_t)indices.size();
			newSurface.count = (uint32_t)GLTF.accessors[p.indicesAccessor.value()].count;
			size_t initial_vtx = vertices.size();
			{
				fastgltf::Accessor& indexAccessor = GLTF.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size()+indexAccessor.count);
				fastgltf::iterateAccessor<std::uint32_t>(GLTF, indexAccessor,
					[&](std::uint32_t indx) {
						indices.push_back(indx + initial_vtx);
					});
			}

			{
				fastgltf::Accessor& posAccessor = GLTF.accessors[p.findAttribute("POSITION")->second];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(GLTF, posAccessor,
					[&](glm::vec3 v, size_t index) {
						Vertex3D newVtx;
						newVtx.position = v;
						newVtx.normal = { 1, 0, 0 };
						newVtx.color = glm::vec4{ 1.f };
						newVtx.uv_x = 0;
						newVtx.uv_y = 0;
						vertices[initial_vtx + index] = newVtx;
					});
			}

			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec3>(GLTF, GLTF.accessors[(*normals).second],
					[&](glm::vec3 v, size_t index) {
						vertices[initial_vtx + index].normal = v;
					});
			}
			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec2>(GLTF, GLTF.accessors[(*uv).second], 
					[&](glm::vec2 v, size_t index) {
						vertices[initial_vtx + index].uv_x = v.x;
						vertices[initial_vtx + index].uv_y = v.y;
					});
			}
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec4>(GLTF, GLTF.accessors[(*colors).second],
					[&](glm::vec4 v, size_t index) {
						vertices[initial_vtx + index].color = v;
					});
			}
			newMesh.surfaces.push_back(newSurface);
		}
		constexpr bool OVERRIDE_COLORS = false;//true;
		if (OVERRIDE_COLORS) 
			for (Vertex3D & vertex : vertices) 
				vertex.color = glm::vec4(vertex.normal, 1.f);
		newMesh.meshBuffers = vulkanEngine->upload_mesh_to_GPU(indices, vertices);
		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
	}
	return meshes;
}

std::optional<std::shared_ptr<GLTFSceneInstance>> loadGLTFScene(VulkanEngine* vulkanEngine, std::string_view filePath) {
	fmt::print("Loading GLTF from {} \n", filePath);
	std::shared_ptr<GLTFSceneInstance> scene = std::make_shared<GLTFSceneInstance>();
	scene->creator = vulkanEngine;
	GLTFSceneInstance& sceneReference = *scene.get();
	fastgltf::Parser parser{};
	constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | 
								 fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
	fastgltf::GltfDataBuffer dataBuffer;
	dataBuffer.loadFromFile(filePath);
	fastgltf::Asset gltfAsset;
	std::filesystem::path path = filePath;
	auto fileType = fastgltf::determineGltfFileType(&dataBuffer);
	if (fileType == fastgltf::GltfType::glTF) {
		auto loadedAsset = parser.loadGLTF(&dataBuffer, path.parent_path(), gltfOptions);
		if (loadedAsset) {
			gltfAsset = std::move(loadedAsset.get());
		}
		else {
			std::cerr << "Asset loading went wrong, cause: " << fastgltf::to_underlying(loadedAsset.error()) << std::endl;
			return{ };
		}
	}
	else if (fileType == fastgltf::GltfType::GLB) {
		auto loadedAsset = parser.loadBinaryGLTF(&dataBuffer, path.parent_path(), gltfOptions);
		if (loadedAsset) {
			gltfAsset = std::move(loadedAsset.get());
		}
		else {
			std::cerr << "Asset loading went wrong, cause: " << fastgltf::to_underlying(loadedAsset.error()) << std::endl;
			return{ };
		}
	}
	else {
		std::cerr << "Inappropriate file type" << std::endl;
		return{};
	}
	std::vector<ScalableDescriptorAllocator::PoolSizeRatio2> sizes = {
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,3},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,3},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}
	};
	sceneReference.scalableDescriptorPool.initialize_pools(vulkanEngine->_vulkanDevice, gltfAsset.materials.size(), sizes);

	for (fastgltf::Sampler& gltfSampler : gltfAsset.samplers) {
		VkSamplerCreateInfo samplerCreateInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
		samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
		samplerCreateInfo.minLod = 0;
		samplerCreateInfo.magFilter = openGL_filter_to_vk_filter(gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
		samplerCreateInfo.minFilter = openGL_filter_to_vk_filter(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));
		samplerCreateInfo.mipmapMode = openGL_filter_to_vk_mipmap_mode(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(vulkanEngine->_selectedGPU, &properties);
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
		samplerCreateInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		VkSampler newVulkanSampler;
		vkCreateSampler(vulkanEngine->_vulkanDevice, &samplerCreateInfo, nullptr, &newVulkanSampler);
		sceneReference.vulkanSamplers.push_back(newVulkanSampler);
	}

	std::vector<std::shared_ptr<MeshAsset>> tempMeshesArray;
	std::vector<std::shared_ptr<HierarchyNode>> tempHierarchyNodesArray;
	std::vector<AllocatedImage> tempAllocatedImagesArray;
	std::vector<std::shared_ptr<GLTFMaterial>> tempGLTFMaterialsArray;

	for (fastgltf::Image& gltfImage : gltfAsset.images) {
		std::optional<AllocatedImage> allocatedImage = loadTexture(vulkanEngine, gltfAsset, gltfImage);
		if (allocatedImage.has_value()) {
			tempAllocatedImagesArray.push_back(*allocatedImage);
			sceneReference.allocatedImages[gltfImage.name.c_str()] = *allocatedImage;
		}
		else {
			tempAllocatedImagesArray.push_back(vulkanEngine->_errorCheckboardImage);
			std::cerr << "Loading went wrong on texture: " << gltfImage.name << std::endl;
		}
	}

	if (gltfAsset.materials.size() != 0) {
		sceneReference.materialDataBuffer = vulkanEngine->create_allocated_buffer(sizeof(GLTFMetalRoughness::MaterialConstants) * gltfAsset.materials.size(),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		int dataIndex = 0;
		GLTFMetalRoughness::MaterialConstants* sceneMaterialConstants = (GLTFMetalRoughness::MaterialConstants*)sceneReference.materialDataBuffer.vulkanMemoryAllocationInfo.pMappedData;
		for (fastgltf::Material& gltfMaterial : gltfAsset.materials) {
			std::shared_ptr<GLTFMaterial> newMaterial = std::make_shared<GLTFMaterial>();
			tempGLTFMaterialsArray.push_back(newMaterial);
			sceneReference.gltfMaterials[gltfMaterial.name.c_str()] = newMaterial;
			GLTFMetalRoughness::MaterialConstants materialConstants;
			materialConstants.colorFactors.x = gltfMaterial.pbrData.baseColorFactor[0];
			materialConstants.colorFactors.y = gltfMaterial.pbrData.baseColorFactor[1];
			materialConstants.colorFactors.z = gltfMaterial.pbrData.baseColorFactor[2];
			materialConstants.colorFactors.w = gltfMaterial.pbrData.baseColorFactor[3];
			materialConstants.metalRoughnessFactors.x = gltfMaterial.pbrData.metallicFactor;
			materialConstants.metalRoughnessFactors.y = gltfMaterial.pbrData.roughnessFactor;
			sceneMaterialConstants[dataIndex] = materialConstants;
			MaterialType materialType = MaterialType::MainColor;
			if (gltfMaterial.alphaMode == fastgltf::AlphaMode::Blend)
				materialType = MaterialType::Transparent;
			GLTFMetalRoughness::MaterialResources materialResources;
			materialResources.colorImage = vulkanEngine->_whiteImage;
			materialResources.colorSampler = vulkanEngine->_defaultSamplerLinear;
			materialResources.metalRoughnessImage = vulkanEngine->_whiteImage;
			materialResources.metalRoughnessSampler = vulkanEngine->_defaultSamplerLinear;
			materialResources.dataBuffer = sceneReference.materialDataBuffer.vulkanBuffer;
			materialResources.dataBufferOffset = dataIndex * sizeof(GLTFMetalRoughness::MaterialConstants);
			if (gltfMaterial.pbrData.baseColorTexture.has_value()) {
				size_t imageIndex = gltfAsset.textures[gltfMaterial.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
				size_t samplerIndex = gltfAsset.textures[gltfMaterial.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();
				materialResources.colorImage = tempAllocatedImagesArray[imageIndex];
				materialResources.colorSampler = sceneReference.vulkanSamplers[samplerIndex];
			}
			newMaterial->materialData = vulkanEngine->metalRoughnessMaterial.write_material(vulkanEngine->_vulkanDevice, materialType,
				materialResources, sceneReference.scalableDescriptorPool);
			dataIndex++;
		}
	}
	std::vector<uint32_t>indices;
	std::vector<Vertex3D> vertices;
	for (fastgltf::Mesh& gltfMesh : gltfAsset.meshes) {
		std::shared_ptr<MeshAsset> newMeshAsset = std::make_shared<MeshAsset>();
		tempMeshesArray.push_back(newMeshAsset);
		sceneReference.meshAssets[gltfMesh.name.c_str()] = newMeshAsset;
		newMeshAsset->name = gltfMesh.name;
		indices.clear();
		vertices.clear();
		for (fastgltf::Primitive& primitive : gltfMesh.primitives) {
			GeoSurface newGeoSurface;
			newGeoSurface.startIndex = (uint32_t)indices.size();
			newGeoSurface.count = (uint32_t)gltfAsset.accessors[primitive.indicesAccessor.value()].count;
			size_t initialVertex = vertices.size();
			{
				fastgltf::Accessor& indexAccessor = gltfAsset.accessors[primitive.indicesAccessor.value()];
				indices.reserve(indices.size() + indexAccessor.count);
				fastgltf::iterateAccessor<std::uint32_t>(gltfAsset, indexAccessor,
														 [&](std::uint32_t index) {
															 indices.push_back(index+initialVertex);
														 });
			}
			{
				fastgltf::Accessor& positionAccessor = gltfAsset.accessors[primitive.findAttribute("POSITION")->second];
				vertices.resize(vertices.size() + positionAccessor.count);
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltfAsset, positionAccessor,
															  [&](glm::vec3 vertexPosition, size_t index){
																 Vertex3D newVertex;
																 newVertex.position = vertexPosition;
																 newVertex.normal = { 1,0,0 };
																 newVertex.color = glm::vec4{ 1.f };
																 newVertex.uv_x = 0;
																 newVertex.uv_y = 0;
																 vertices[initialVertex + index] = newVertex;
															  });
			}
			fastgltf::Primitive::attribute_type* normals = primitive.findAttribute("NORMAL");
			if (normals != primitive.attributes.end())
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltfAsset, gltfAsset.accessors[(*normals).second],
															  [&](glm::vec3 normalVector, size_t index){
																  vertices[initialVertex+index].normal=normalVector;
															  });

			fastgltf::Primitive::attribute_type* uv = primitive.findAttribute("TEXCOORD_0");
			if (uv != primitive.attributes.end())
				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltfAsset, gltfAsset.accessors[(*uv).second],
															  [&](glm::vec2 uvVector, size_t index) {
																  vertices[initialVertex + index].uv_x = uvVector.x;
																  vertices[initialVertex + index].uv_y = uvVector.y;
															  });

			fastgltf::Primitive::attribute_type* colors = primitive.findAttribute("COLOR_0");
			if (colors != primitive.attributes.end())
				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltfAsset, gltfAsset.accessors[(*colors).second],
															[&](glm::vec4 colorVector, size_t index) {
																vertices[initialVertex + index].color = colorVector;
															});
			if (gltfAsset.materials.size() != 0) {
				if (primitive.materialIndex.has_value())
					newGeoSurface.gltfMaterial = tempGLTFMaterialsArray[primitive.materialIndex.value()];
				else
					newGeoSurface.gltfMaterial = tempGLTFMaterialsArray[0];
			}
			else 
				newGeoSurface.gltfMaterial = std::make_shared<GLTFMaterial>(vulkanEngine->defaultMaterialData);
			glm::vec3 minPosition = vertices[initialVertex].position;
			glm::vec3 maxPosition = vertices[initialVertex].position;
			for (int i = initialVertex; i < vertices.size(); i++) {
				minPosition = glm::min(minPosition, vertices[i].position);
				maxPosition = glm::max(maxPosition, vertices[i].position);
			}
			newGeoSurface.cubicBounds.center = (maxPosition + minPosition) * 0.5f;
			newGeoSurface.cubicBounds.size = (maxPosition - minPosition) * 0.5f;
			newGeoSurface.cubicBounds.radius = glm::length(newGeoSurface.cubicBounds.size);
			newMeshAsset->surfaces.push_back(newGeoSurface);
		}
		newMeshAsset->meshBuffers = vulkanEngine->upload_mesh_to_GPU(indices, vertices);
	}

	for (fastgltf::Node& gltfNode : gltfAsset.nodes) {
		std::shared_ptr<HierarchyNode> newHierarchyNode;
		if (gltfNode.meshIndex.has_value()) {
			newHierarchyNode = std::make_shared<MeshNode>();
			static_cast<MeshNode*>(newHierarchyNode.get())->meshAsset = tempMeshesArray[*gltfNode.meshIndex];
		}
		else
			newHierarchyNode = std::make_shared<HierarchyNode>();
		
		tempHierarchyNodesArray.push_back(newHierarchyNode);
		sceneReference.hierarchyNodes[gltfNode.name.c_str()];
		std::visit(fastgltf::visitor{
				[&](fastgltf::Node::TransformMatrix transformationMatrix) {
					memcpy(&newHierarchyNode->localTransform, transformationMatrix.data(), sizeof(transformationMatrix));
				},
				[&](fastgltf::Node::TRS transform) {
					glm::vec3 translation(transform.translation[0],transform.translation[1],transform.translation[2]);
					glm::quat rotation(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
					glm::vec3 scale(transform.scale[0], transform.scale[1], transform.scale[2]);
					glm::mat4 translationMatrix = glm::translate(glm::mat4(1.f), translation);
					glm::mat4 rotationMatrix = glm::toMat4(rotation);
					glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.f), scale);
					newHierarchyNode->localTransform = translationMatrix * rotationMatrix * scaleMatrix;
				} 
			},
			gltfNode.transform);
	}

	for (int i = 0; i < gltfAsset.nodes.size(); i++) {
		fastgltf::Node& gltfNode = gltfAsset.nodes[i];
		std::shared_ptr<HierarchyNode>& sceneHierarchyNode = tempHierarchyNodesArray[i];
		for (size_t& child : gltfNode.children) {
			sceneHierarchyNode->childrenNodes.push_back(tempHierarchyNodesArray[child]);
			tempHierarchyNodesArray[child]->parentalNode = sceneHierarchyNode;
		}
	}
	for (std::shared_ptr<HierarchyNode>& hierarchyNode : tempHierarchyNodesArray) 
		if (hierarchyNode->parentalNode.lock() == nullptr) {
			sceneReference.hierarchyTopNodes.push_back(hierarchyNode);
			hierarchyNode->refreshWorldTransform(glm::mat4{ 1.f });
		}

	return scene;
}

std::optional<AllocatedImage> loadTexture(VulkanEngine* vulkanEngine, fastgltf::Asset& gltfAsset, fastgltf::Image& gltfImage) {
	AllocatedImage newTexture{};
	int textureWidth, textureHeight, channels;
	std::visit(
		fastgltf::visitor{
			[](auto& arguments) {},
			[&](fastgltf::sources::URI& filePath) {
				assert(filePath.fileByteOffset == 0);
				assert(filePath.uri.isLocalPath());
				const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());
				unsigned char* data = stbi_load(path.c_str(), &textureWidth, &textureHeight, &channels, 4);
				if (data) {
					VkExtent3D textureSize;
					textureSize.width = textureWidth;
					textureSize.height = textureHeight;
					textureSize.depth = 1;
					newTexture = vulkanEngine->create_allocated_image_with_data(data, textureSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
					stbi_image_free(data);
				}
			},
			[&](fastgltf::sources::Vector& bufferVector) {
				unsigned char* data = stbi_load_from_memory(bufferVector.bytes.data(), static_cast<int>(bufferVector.bytes.size()), 
															&textureWidth, &textureHeight, &channels, 4);
				if (data) {
					VkExtent3D textureSize;
					textureSize.width = textureWidth;
					textureSize.height = textureHeight;
					textureSize.depth = 1;
					newTexture = vulkanEngine->create_allocated_image_with_data(data, textureSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
					stbi_image_free(data);
				}
			},
			[&](fastgltf::sources::BufferView& bufferViewReference) {
				fastgltf::BufferView& bufferView = gltfAsset.bufferViews[bufferViewReference.bufferViewIndex];
				fastgltf::Buffer& buffer = gltfAsset.buffers[bufferView.bufferIndex];
				std::visit(fastgltf::visitor {
						[](auto& arguments) {},
						[&](fastgltf::sources::Vector& bufferVector) {
							unsigned char* data = stbi_load_from_memory(bufferVector.bytes.data() + bufferView.byteOffset, static_cast<int>(bufferView.byteLength), 
																		&textureWidth, &textureHeight, &channels, 4);
							if (data) {
								VkExtent3D textureSize;
								textureSize.width = textureWidth;
								textureSize.height = textureHeight;
								textureSize.depth = 1;
								newTexture = vulkanEngine->create_allocated_image_with_data(data, textureSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
								stbi_image_free(data);
							}
						}
					},
					buffer.data
				);
			},
		},
		gltfImage.data
	);
	if (newTexture.vulkanImage == VK_NULL_HANDLE) {
		return {};
	}
	else {
		return newTexture;
	}
}
#pragma endregion

VkFilter openGL_filter_to_vk_filter(fastgltf::Filter glFilter) {
	if (glFilter == fastgltf::Filter::Nearest ||
		glFilter == fastgltf::Filter::NearestMipMapNearest ||
		glFilter == fastgltf::Filter::NearestMipMapLinear)
		return VK_FILTER_NEAREST;
	else
		return VK_FILTER_LINEAR;
}
VkSamplerMipmapMode openGL_filter_to_vk_mipmap_mode(fastgltf::Filter glFilter) {
	if (glFilter == fastgltf::Filter::NearestMipMapNearest ||
		glFilter == fastgltf::Filter::LinearMipMapNearest)
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;
	else
		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

void GLTFSceneInstance::Draw(const glm::mat4& topMatrix, DrawContext& drawContext) {
	for (std::shared_ptr<HierarchyNode>& hierarchyNode : hierarchyTopNodes) 
		hierarchyNode->Draw(topMatrix, drawContext);
}

void GLTFSceneInstance::clearScene() {
	VkDevice vulkanDevice = creator->_vulkanDevice;
	scalableDescriptorPool.destroy_pools(vulkanDevice);
	creator->destroy_allocated_buffer(materialDataBuffer);
	for (auto& [name,  meshAsset] : meshAssets) {
		creator->destroy_allocated_buffer(meshAsset->meshBuffers.indexBuffer);
		creator->destroy_allocated_buffer(meshAsset->meshBuffers.vertexBuffer);
	}

	for (auto& [name, allocatedImage] : allocatedImages)
		if (allocatedImage.vulkanImage == creator->_errorCheckboardImage.vulkanImage)
			continue;
		else
			creator->destroy_allocated_image(allocatedImage);

	for (VkSampler& vulkanSampler : vulkanSamplers) 
		vkDestroySampler(vulkanDevice, vulkanSampler, nullptr);
	
}
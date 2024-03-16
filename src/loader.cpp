#include "stb_image.h"
#include "iostream"
#include <loader.h>
#include "engine.h"
#include "initializers.h"
#include "types.h"
#include "glm/gtx/quaternion.hpp"
#include "fastgltf/glm_element_traits.hpp"
#include "fastgltf/parser.hpp"
#include "fastgltf/tools.hpp"


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
		constexpr bool OVERRIDE_COLORS = true;
		if (OVERRIDE_COLORS) 
			for (Vertex3D & vertex : vertices) 
				vertex.color = glm::vec4(vertex.normal, 1.f);
		newMesh.meshBuffers = vulkanEngine->upload_mesh_to_GPU(indices, vertices);
		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
	}
	return meshes;
}
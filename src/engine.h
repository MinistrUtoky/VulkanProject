// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once
#include <types.h>
#include <descriptors.h>
#include <loader.h>
#include "camera.h"
#include <glm/gtx/transform.hpp>

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};
struct ComputeEffect {
	const char* name;
	VkPipeline pipeline;
	VkPipelineLayout layout;
	ComputePushConstants data;
};

struct GLTFMetalRoughness {
public:
	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metalRoughnessFactors;
		glm::vec4 extraPadding[14];
	};
	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughnessImage;
		VkSampler metalRoughnessSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	MaterialPipeline opaqueObjectsPipeline;
	MaterialPipeline transparentObjectsPipeline;
	VkDescriptorSetLayout materialDescriptorSetLayout;

	DescriptorWriter descriptorWriter;

	void build_pipelines(VulkanEngine* vulkanEngine);
	void clear_resources(VkDevice vulkanDevice);

	RenderableMaterial write_material(VkDevice vulkanDevice, MaterialType type, const MaterialResources& resources, ScalableDescriptorAllocator& descriptorAllocator);
};

struct RenderableObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	RenderableMaterial* renderableMaterial;
	CubicBounds cubicBounds;
	glm::mat4 objectTransform;
	VkDeviceAddress vertexBufferAddress;
};
struct DrawContext {
	std::vector<RenderableObject> OpaqueObjects;
    std::vector<RenderableObject> TransparentObjects;
};
struct MeshNode : public HierarchyNode {
	std::shared_ptr<MeshAsset> meshAsset;
	virtual void Draw(const glm::mat4& topMatrix, DrawContext& drawContext) override;
};

struct DeletionQueue {
	// It's better having arrays of vulkan handles of different types and then delete them from a loop, but this will do for now
	std::deque<std::function<void()>> deletingFunctions;

	void push_back_deleting_function(std::function<void()>&& function) {
		deletingFunctions.push_back(function);
	}

	void flushAll() {
		for (auto functor = deletingFunctions.rbegin(); functor != deletingFunctions.rend(); functor++) {
			(*functor)();
		}
		deletingFunctions.clear();
	}
};
struct FrameInfo {
	ScalableDescriptorAllocator _frameDescriptors;
	VkCommandPool _vulkanCommandPool;
	VkCommandBuffer _mainVulkanCommandBuffer;
	VkSemaphore _vulkanSwapchainSemaphore, _vulkanRenderingSemaphore;
	VkFence _vulkanRenderingFence;
	DeletionQueue _deletionQueue;
};

struct EngineInfo {
	float fps;
	int triangleCount;
	int drawcallCount;
	float sceneUpdateTime;
	float meshDrawTime;
};

//non-engine logic
struct Transformations {
protected:
	float millisecondsPerIteration;
	int currentIndex = 0;
	std::vector<glm::vec3> movements;
public:
	std::chrono::system_clock::time_point movementStart;
	std::chrono::system_clock::time_point iStart;

	Transformations() = default;
	Transformations(std::vector<glm::vec3> movements, float iterationMilliseconds = 1000.f) {
		this->movements = movements;
		millisecondsPerIteration = iterationMilliseconds;
		movementStart = std::chrono::system_clock::now();
		iStart = std::chrono::system_clock::now();
	};
	~Transformations() { movements.clear(); }
	virtual glm::mat4 getCurrentMatrix() {  return glm::mat4{ 1.f }; }
};

// grows until the set scale
struct Scales : public Transformations {
	glm::vec3 currentIterationStartScale;
	Scales() = default;
	Scales(glm::vec3 startScale, std::vector<glm::vec3> movements, float iterationMilliseconds = 1000.f) : Transformations(movements, iterationMilliseconds) {
		currentIterationStartScale = startScale;
	}
	glm::mat4 getCurrentMatrix() override;
};

// once iteration rotation makes full circle cycle
struct Rotations : public Transformations {
	Rotations() = default;
	Rotations(std::vector<glm::vec3> movements, float iterationMilliseconds = 1000.f) : Transformations(movements, iterationMilliseconds) { }
	glm::mat4 getCurrentMatrix() override;
};

// iteration means passing set vector distance
struct Translations : public Transformations {
	Translations() = default;
	Translations(std::vector<glm::vec3> movements, float iterationMilliseconds = 1000.f) : Transformations(movements, iterationMilliseconds) { }
	glm::mat4 getCurrentMatrix() override;
};

struct Movement {
private:
	Scales scales;
	Rotations rotations;
	Translations translations;
public:
	Movement() = default;
	Movement(std::vector<glm::vec3> scales, std::vector<glm::vec3> rotations, std::vector<glm::vec3> translations, 
		float scaleIterationMilliseconds = 1000.f, float rotationIterationMilliseconds = 1000.f, float translationIterationMilliseconds = 1000.f, 
		glm::vec3 startScale=glm::vec3{1.f}) {// since 1.f is default scale for any object's draw call and we are resizing exactly it's matrix
		this->scales = Scales{ startScale, scales, scaleIterationMilliseconds };
		this->rotations = Rotations{ rotations, rotationIterationMilliseconds };
		this->translations = Translations{ translations, translationIterationMilliseconds };
	}
	glm::mat4 getCurrentMatrix();
	void resetTimers();
};

//non-engine logic

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:
	FrameInfo _frames[FRAME_OVERLAP];
	VkQueue _vulkanGraphicsQueue;
	uint32_t _vulkanGraphicsQueueFamily;
	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stopRendering{ false };
	VkExtent2D _windowExtent{ 1280 , 720 };
	struct SDL_Window* _window{ nullptr };
	static VulkanEngine& Get();

	VkInstance _vulkanInstance;
	VkDebugUtilsMessengerEXT _debugMessenger;
	VkPhysicalDevice _selectedGPU;
	VkDevice _vulkanDevice;
	VkSurfaceKHR _windowSurface;

	VkSwapchainKHR _vulkanSwapchain;
	VkFormat _vulkanSwapchainImageFormat;
	std::vector<VkImage> _vulkanSwapchainImages;
	std::vector<VkImageView> _vulkanSwapchainImageViews;
	VkExtent2D _vulkanSwapchainExtent;

	DeletionQueue _mainDeletionQueue;
	VmaAllocator _vulkanMemoryAllocator;

	AllocatedImage _allocatedImage;
	AllocatedImage _depthImage;
	VkExtent2D _vulkanImageExtent2D;

	ScalableDescriptorAllocator globalDescriptorAllocator;
	VkDescriptorSet _vulkanImageDescriptorSet;
	VkDescriptorSetLayout _vulkanImageDescriptorSetLayout;
	
	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;

	VkFence _immediateVulkanFence;
	VkCommandBuffer _immediateVulkanCommandBuffer;
	VkCommandPool _immediateVulkanCommandPool;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };

	VkPipelineLayout _vulkanTrianglePipelineLayout;
	VkPipeline _vulkanTrainglePipeline;

	VkPipelineLayout _vulkanMeshPipelineLayout;
	VkPipeline _vulkanMeshPipeline;
	GPUMeshBuffers rectangle;

	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	bool resizeRequested{ false };
	VkExtent2D _vulkanDrawExtent;
	float renderScale = 1.f;

	GPUSceneData sceneData;
	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckboardImage;

	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	VkDescriptorSetLayout _singleImageDescriptorLayout;

	RenderableMaterial defaultMaterialData;
	GLTFMetalRoughness metalRoughnessMaterial;

	DrawContext mainDrawContext;
	std::unordered_map<std::string, std::shared_ptr<HierarchyNode>> loadedNodes;


	EngineInfo engineInfo;

	// non-engine logic
	Camera mainEngineBuiltinCamera;
	std::unordered_map<std::string, std::shared_ptr<GLTFSceneInstance>> existingScenes;
	std::unordered_map < std::string, glm::mat4 > existingScenesStartMatrices;
	const std::string assetExtensions[2]{
		".glb",
		".gltf"
	};
	const std::string assetsPath{ "..\\assets\\" };
	std::string selectedScene{ "" };
	glm::mat4 selectedSceneMovedMatrix{ 1.f };
	std::unordered_map <std::string, Movement> allStandardMovements;
	std::string selectedMovementName{ "Staying Still" };
	Movement currentMovement;
	//

	void init();
	void cleanup();
	void draw();
	void run();
	AllocatedBuffer create_allocated_buffer(size_t allocationSize, VkBufferUsageFlags bufferUsageFlags, VmaMemoryUsage allocationMemoryUsage);
	void destroy_allocated_buffer(const AllocatedBuffer& buffer);
	AllocatedImage create_allocated_image(VkExtent3D size, VkFormat format, VkImageUsageFlags imageUsageFlags, bool mipmapped = false);
	AllocatedImage create_allocated_image_with_data(void*data, VkExtent3D size, VkFormat format, VkImageUsageFlags imageUsageFlags, bool mipmapped = false);
	void destroy_allocated_image(const AllocatedImage& image);
	GPUMeshBuffers upload_mesh_to_GPU(std::span<uint32_t> indices, std::span<Vertex3D> vertices);

private:
	void vulkan_init();
	void swapchain_init();
	void commands_init();
	void sync_structs_init();
	void descriptors_init();
	void pipelines_init();
	void background_pipelines_init();
	void triangle_pipeline_init();
	void mesh_pipeline_init();
	void imgui_init();
	void default_data_init();
	void upload_scenes();
	void upload_2D_rectangle_to_GPU();
	void swapchain_create(uint32_t swapchainWidth, uint32_t swapchainHeight);
	void swapchain_resize();
	void immediate_command_submit(std::function<void(VkCommandBuffer vulkanCommandBuffer)>&& function);
	void prepare_imgui();
	void draw_background(VkCommandBuffer vulkanCommandBuffer);
	void draw_imgui(VkCommandBuffer vulkanCommandBuffer, VkImageView targetVulkanImageView);
	void draw_geometry(VkCommandBuffer vulkanCommandBuffer);
	void update_scene();
	FrameInfo& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };
	void swapchain_destroy();
	void create_scene(std::string sceneName, std::string filePath);
	void loadTestMeshes();
};


bool isVisible(const RenderableObject& renderableObject, const glm::mat4& viewportToProjectionMatrix);


//non-engine logics

bool HorizontallyCentralizedButton(const char* buttonText);
void rescaleSceneToTheSize(GLTFSceneInstance& scene, glm::vec3 newSize);
void moveSceneTo(GLTFSceneInstance& scene, glm::vec3 position);
void normalizeScene(GLTFSceneInstance& scene);



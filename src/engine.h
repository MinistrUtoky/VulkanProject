// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once
#include <types.h>
#include <descriptors.h>
#include <loader.h>
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

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:
	FrameInfo _frames[FRAME_OVERLAP];
	VkQueue _vulkanGraphicsQueue;
	uint32_t _vulkanGraphicsQueueFamily;
	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stopRendering{ false };
	VkExtent2D _windowExtent{ 800 , 600 };
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

	DescriptorAllocator globalDescriptorAllocator;
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

	void init();
	void cleanup();
	void draw();
	void run();
	FrameInfo& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };
	void immediate_command_submit(std::function<void(VkCommandBuffer vulkanCommandBuffer)>&& function);
	AllocatedBuffer create_allocated_buffer(size_t allocationSize, VkBufferUsageFlags bufferUsageFlags, VmaMemoryUsage allocationMemoryUsage);
	GPUMeshBuffers upload_mesh_to_GPU(std::span<uint32_t> indices, std::span<Vertex3D> vertices);
private:
	void vulkan_init();
	void swapchain_init();
	void commands_init();
	void sync_structs_init();
	void swapchain_create(uint32_t swapchainWidth, uint32_t swapchainHeight);
	void swapchain_destroy();
	void swapchain_resize();
	void descriptors_init();
	void pipelines_init();
	void background_pipelines_init();
	void triangle_pipeline_init();
	void mesh_pipeline_init();
	void imgui_init();
	void draw_background(VkCommandBuffer vulkanCommandBuffer);
	void draw_imgui(VkCommandBuffer vulkanCommandBuffer, VkImageView targetVulkanImageView);
	void draw_geometry(VkCommandBuffer vulkanCommandBuffer);
	void default_data_init();
	void destroy_allocated_buffer(const AllocatedBuffer& buffer);
};


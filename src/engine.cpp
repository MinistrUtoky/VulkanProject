//> includes
#include "engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <initializers.h>
#include <types.h>
#include <chrono>
#include <thread>
#include "VkBootstrap.h" // fast startup library
#include "images.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <glm/gtx/transform.hpp>

#include "pipelines.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

VulkanEngine* loadedEngine = nullptr;
VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
constexpr bool bUseValidationLayers = false;

#pragma region Initialization
void VulkanEngine::init()
{
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    _window = SDL_CreateWindow(
        "Geometric Shapes",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

    vulkan_init();
    swapchain_init();
    commands_init();
    sync_structs_init();
    descriptors_init();
    pipelines_init();
    imgui_init();
    default_data_init();

    _isInitialized = true;
}

void VulkanEngine::vulkan_init() {
    // Creating vulkan instance
    vkb::InstanceBuilder builder;  
    vkb::Result<vkb::Instance> instance = builder.set_app_name("Some application")
                               .request_validation_layers(bUseValidationLayers)
                               .use_default_debug_messenger()
                               .require_api_version(1, 3, 0)
                               .build();
    vkb::Instance vkBootstrap = instance.value();
    _vulkanInstance = vkBootstrap.instance;
    _debugMessenger = vkBootstrap.debug_messenger;

    // Creating vulkan device
    SDL_Vulkan_CreateSurface(_window, _vulkanInstance, &_windowSurface);
    VkPhysicalDeviceVulkan13Features features13{};
    VkPhysicalDeviceVulkan12Features features12{};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;
    vkb::PhysicalDeviceSelector selector{ vkBootstrap };
    vkb::PhysicalDevice physDevice = selector.set_minimum_version(1, 3)
                                             .set_required_features_13(features13)
                                             .set_required_features_12(features12)
                                             .set_surface(_windowSurface)
                                             .select()
                                             .value();
    vkb::DeviceBuilder deviceBuilder{ physDevice };
    vkb::Device vkBootstrapDevice = deviceBuilder.build().value();
    _vulkanDevice = vkBootstrapDevice.device;
    _selectedGPU = physDevice.physical_device;

    //Getting grpahics queue
    _vulkanGraphicsQueue = vkBootstrapDevice.get_queue(vkb::QueueType::graphics).value();
    _vulkanGraphicsQueueFamily = vkBootstrapDevice.get_queue_index(vkb::QueueType::graphics).value();

    // initializing memory allocator
    VmaAllocatorCreateInfo vulkanMemoryAllocatorInfo = {};
    vulkanMemoryAllocatorInfo.physicalDevice = _selectedGPU;
    vulkanMemoryAllocatorInfo.device = _vulkanDevice;
    vulkanMemoryAllocatorInfo.instance = _vulkanInstance;
    vulkanMemoryAllocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&vulkanMemoryAllocatorInfo, &_vulkanMemoryAllocator);
    _mainDeletionQueue.push_back_deleting_function([&]() {
        fmt::println("{}", VK_MAX_MEMORY_HEAPS);
        for (int i = 0; i < VK_MAX_MEMORY_HEAPS; i++) {
            uint32_t heapIndex = 0;
            VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
            vmaGetHeapBudgets(_vulkanMemoryAllocator, budgets);
            printf("My heap currently has %u allocations taking %llu B,\n",
                budgets[heapIndex].statistics.allocationCount,
                budgets[heapIndex].statistics.allocationBytes);
            printf("allocated out of %u Vulkan device memory blocks taking %llu B,\n",
                budgets[heapIndex].statistics.blockCount,
                budgets[heapIndex].statistics.blockBytes);
            printf("Vulkan reports total usage %llu B with budget %llu B.\n",
                budgets[heapIndex].usage,
                budgets[heapIndex].budget);
        }
        vmaDestroyAllocator(_vulkanMemoryAllocator);
        });
}

void VulkanEngine::swapchain_init() {
    swapchain_create(_windowExtent.width, _windowExtent.height);
    VkExtent3D vulkanImageExtent3D = { _windowExtent.width, _windowExtent.height, 1 };
    
    _allocatedImage.vulkanImageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _allocatedImage.vulkanImageExtent3D = vulkanImageExtent3D;
        
    VkImageUsageFlags vulkanImageUsageFlags{};
    vulkanImageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    vulkanImageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    vulkanImageUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    vulkanImageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo vulkanImageCreateInfo = vkinit::image_create_info(_allocatedImage.vulkanImageFormat, vulkanImageUsageFlags, vulkanImageExtent3D);

    VmaAllocationCreateInfo vulkanMemoryAllocationCreateInfo = {};
    vulkanMemoryAllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vulkanMemoryAllocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(_vulkanMemoryAllocator, &vulkanImageCreateInfo, &vulkanMemoryAllocationCreateInfo, &_allocatedImage.vulkanImage, &_allocatedImage.vulkanMemoryAllocation, nullptr);
    
    VkImageViewCreateInfo vulkanImageViewCreateInfo = vkinit::imageview_create_info(_allocatedImage.vulkanImageFormat, _allocatedImage.vulkanImage, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(_vulkanDevice, &vulkanImageViewCreateInfo, nullptr, &_allocatedImage.vulkanImageView));

    //z-buffering
    _depthImage.vulkanImageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage.vulkanImageExtent3D = vulkanImageExtent3D;
    VkImageUsageFlags vulkanDepthImageUsageFlags{};
    vulkanDepthImageUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkImageCreateInfo vulkanDepthImageCreateInfo = vkinit::image_create_info(_depthImage.vulkanImageFormat, vulkanDepthImageUsageFlags, vulkanImageExtent3D);
    vmaCreateImage(_vulkanMemoryAllocator, &vulkanDepthImageCreateInfo, &vulkanMemoryAllocationCreateInfo, &_depthImage.vulkanImage, &_depthImage.vulkanMemoryAllocation, nullptr);

    VkImageViewCreateInfo depthImageViewCreateInfo = vkinit::imageview_create_info(_depthImage.vulkanImageFormat, _depthImage.vulkanImage, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_vulkanDevice, &depthImageViewCreateInfo, nullptr, &_depthImage.vulkanImageView));

    _mainDeletionQueue.push_back_deleting_function([=]() {
        vkDestroyImageView(_vulkanDevice, _allocatedImage.vulkanImageView, nullptr);
        vmaDestroyImage(_vulkanMemoryAllocator, _allocatedImage.vulkanImage, _allocatedImage.vulkanMemoryAllocation); 

        vkDestroyImageView(_vulkanDevice, _depthImage.vulkanImageView, nullptr);
        vmaDestroyImage(_vulkanMemoryAllocator, _depthImage.vulkanImage, _depthImage.vulkanMemoryAllocation);
        });
}
void VulkanEngine::swapchain_create(uint32_t swapchainWidth, uint32_t swapchainHeight) {
    vkb::SwapchainBuilder vulkanSwapchainBuilder{ _selectedGPU,_vulkanDevice,_windowSurface };
    _vulkanSwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM; // fragment shaders produced values are directly interpreted in linear RGB (UNORM) 
    vkb::Swapchain vkBootstrapSwapchain = vulkanSwapchainBuilder.set_desired_format(VkSurfaceFormatKHR{.format=_vulkanSwapchainImageFormat,
                                                                                    .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
                                                                .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // hard vsync
                                                                .set_desired_extent(swapchainWidth, swapchainHeight)
                                                                .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                                                .build()
                                                                .value();
    _vulkanSwapchainExtent = vkBootstrapSwapchain.extent;
    _vulkanSwapchain = vkBootstrapSwapchain.swapchain;
    _vulkanSwapchainImages = vkBootstrapSwapchain.get_images().value();
    _vulkanSwapchainImageViews = vkBootstrapSwapchain.get_image_views().value();
}
void VulkanEngine::swapchain_destroy() {
    vkDestroySwapchainKHR(_vulkanDevice, _vulkanSwapchain, nullptr);
    for (int i = 0; i < _vulkanSwapchainImageViews.size(); i++)
        vkDestroyImageView(_vulkanDevice, _vulkanSwapchainImageViews[i], nullptr);
}
void VulkanEngine::swapchain_resize() {
    vkDeviceWaitIdle(_vulkanDevice);
    swapchain_destroy();
    int width, height;
    SDL_GetWindowSize(_window, &width, &height);
    _windowExtent.width = width;
    _windowExtent.height = height;
    swapchain_create(_windowExtent.width, _windowExtent.height);
    resizeRequested = false;
}

void VulkanEngine::commands_init() {
    // creating pool and buffer
    VkCommandPoolCreateInfo vulkanCommandPoolInfo = {};
    vulkanCommandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    vulkanCommandPoolInfo.pNext = nullptr;
    vulkanCommandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vulkanCommandPoolInfo.queueFamilyIndex = _vulkanGraphicsQueueFamily;
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateCommandPool(_vulkanDevice, &vulkanCommandPoolInfo, nullptr, &_frames[i]._vulkanCommandPool));
        VkCommandBufferAllocateInfo commandAllocationInfo = vkinit::command_buffer_allocate_info(_frames[i]._vulkanCommandPool, 1); // we've put the above statement into initializers;
        VK_CHECK(vkAllocateCommandBuffers(_vulkanDevice, &commandAllocationInfo, &_frames[i]._mainVulkanCommandBuffer));
    }
    //immediate commands
    VK_CHECK(vkCreateCommandPool(_vulkanDevice,&vulkanCommandPoolInfo,nullptr,&_immediateVulkanCommandPool));
    VkCommandBufferAllocateInfo vulkanCommandBufferAllocateInfo = vkinit::command_buffer_allocate_info(_immediateVulkanCommandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(_vulkanDevice,&vulkanCommandBufferAllocateInfo, &_immediateVulkanCommandBuffer));
    _mainDeletionQueue.push_back_deleting_function([=]() {
        vkDestroyCommandPool(_vulkanDevice, _immediateVulkanCommandPool, nullptr);
        });
}

void VulkanEngine::sync_structs_init() {
    VkFenceCreateInfo vulkanFenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT); // without it cant call waitfences in the first frame
    VkSemaphoreCreateInfo vulkanSemaphoreCreateInfo = vkinit::semaphore_create_info();
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(_vulkanDevice, &vulkanFenceCreateInfo, nullptr, &_frames[i]._vulkanRenderingFence));
        VK_CHECK(vkCreateSemaphore(_vulkanDevice, &vulkanSemaphoreCreateInfo, nullptr, &_frames[i]._vulkanSwapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_vulkanDevice, &vulkanSemaphoreCreateInfo, nullptr, &_frames[i]._vulkanRenderingSemaphore));
    }

    VK_CHECK(vkCreateFence(_vulkanDevice, &vulkanFenceCreateInfo, nullptr, &_immediateVulkanFence));
    _mainDeletionQueue.push_back_deleting_function([=]() {
        vkDestroyFence(_vulkanDevice, _immediateVulkanFence, nullptr);
        });
}

void VulkanEngine::descriptors_init() {
    std::vector<ScalableDescriptorAllocator::PoolSizeRatio2> poolSizeRatios = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1},
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
    };
    globalDescriptorAllocator.initialize_pools(_vulkanDevice, 10, poolSizeRatios);

    { 
        DescriptorLayoutBuilder descriptorLayoutBuilder; 
        descriptorLayoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); 
        _vulkanImageDescriptorSetLayout = descriptorLayoutBuilder.build(_vulkanDevice, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    {
        DescriptorLayoutBuilder descriptorLayoutBuilder;
        descriptorLayoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        _gpuSceneDataDescriptorLayout = descriptorLayoutBuilder.build(_vulkanDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    {
        DescriptorLayoutBuilder descriptorLayoutBuilder;
        descriptorLayoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        _singleImageDescriptorLayout = descriptorLayoutBuilder.build(_vulkanDevice, VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    _vulkanImageDescriptorSet = globalDescriptorAllocator.allocate(_vulkanDevice, _vulkanImageDescriptorSetLayout);
    // old descriptor with no growth possible

    DescriptorWriter descriptorWriter;
    descriptorWriter.write_image(0, _allocatedImage.vulkanImageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    descriptorWriter.update_set(_vulkanDevice, _vulkanImageDescriptorSet); 
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        std::vector<ScalableDescriptorAllocator::PoolSizeRatio2> frameSizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 }
        };
        _frames[i]._frameDescriptors = ScalableDescriptorAllocator{};
        _frames[i]._frameDescriptors.initialize_pools(_vulkanDevice, 1e+3, frameSizes);
        _mainDeletionQueue.push_back_deleting_function([&, i]() {
            _frames[i]._frameDescriptors.destroy_pools(_vulkanDevice);
            });
    }
}

void VulkanEngine::pipelines_init() {
    // computes
    background_pipelines_init();
    // graphics
    //triangle_pipeline_init();
    mesh_pipeline_init();
    metalRoughnessMaterial.build_pipelines(this);
}

void VulkanEngine::triangle_pipeline_init() {
    VkShaderModule vulkanTriangleFragShaderModule;
    if (!vkutil::load_shader_module("../../vulkan-base/shaders/colored_triangle.frag.spv", _vulkanDevice, &vulkanTriangleFragShaderModule)) // only for windows + msvc folders
        fmt::print("Error during fragment shaders build \n");

    VkShaderModule vulkanTriangleVertShaderModule;
    if (!vkutil::load_shader_module("../../vulkan-base/shaders/colored_triangle.vert.spv", _vulkanDevice, &vulkanTriangleVertShaderModule)) // only for windows + msvc folders
        fmt::print("Error during vertex shaders build \n");

    VkPipelineLayoutCreateInfo vulkanPipelineLayoutCreateInfo = vkinit::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(_vulkanDevice, &vulkanPipelineLayoutCreateInfo, nullptr, &_vulkanTrianglePipelineLayout));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder._vulkanPipelineLayout = _vulkanTrianglePipelineLayout;
    pipelineBuilder.set_shaders(vulkanTriangleVertShaderModule, vulkanTriangleFragShaderModule);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.disable_depth_test();
    pipelineBuilder.set_color_attachment_format(_allocatedImage.vulkanImageFormat);
    pipelineBuilder.set_depth_format(VK_FORMAT_UNDEFINED);

    _vulkanTrainglePipeline = pipelineBuilder.build_pipeline(_vulkanDevice);

    vkDestroyShaderModule(_vulkanDevice, vulkanTriangleFragShaderModule, nullptr);
    vkDestroyShaderModule(_vulkanDevice, vulkanTriangleVertShaderModule, nullptr);

    _mainDeletionQueue.push_back_deleting_function([&]() {
        vkDestroyPipelineLayout(_vulkanDevice, _vulkanTrianglePipelineLayout, nullptr);
        vkDestroyPipeline(_vulkanDevice, _vulkanTrainglePipeline, nullptr);
        });
}

void VulkanEngine::mesh_pipeline_init() {

    VkShaderModule vulkanTriangleFragShaderModule;
    if (!vkutil::load_shader_module("../../vulkan-base/shaders/tex_image.frag.spv", _vulkanDevice, &vulkanTriangleFragShaderModule)) // only for windows + msvc folders
        fmt::print("Error during fragment shaders build \n");

    VkShaderModule vulkanTriangleVertShaderModule;
    if (!vkutil::load_shader_module("../../vulkan-base/shaders/colored_triangle_mesh.vert.spv", _vulkanDevice, &vulkanTriangleVertShaderModule)) // only for windows + msvc folders
        fmt::print("Error during vertex shaders build \n");

    VkPushConstantRange vulkanPushConstantBufferRange{};
    vulkanPushConstantBufferRange.offset = 0;
    vulkanPushConstantBufferRange.size = sizeof(GPUDrawingPushConstants);
    vulkanPushConstantBufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo vulkanPipelineLayoutCreateInfo = vkinit::pipeline_layout_create_info();
    vulkanPipelineLayoutCreateInfo.pPushConstantRanges = &vulkanPushConstantBufferRange;
    vulkanPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    vulkanPipelineLayoutCreateInfo.pSetLayouts = &_singleImageDescriptorLayout;
    vulkanPipelineLayoutCreateInfo.setLayoutCount = 1;
    VK_CHECK(vkCreatePipelineLayout(_vulkanDevice, &vulkanPipelineLayoutCreateInfo, nullptr, &_vulkanMeshPipelineLayout));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder._vulkanPipelineLayout = _vulkanMeshPipelineLayout;
    pipelineBuilder.set_shaders(vulkanTriangleVertShaderModule, vulkanTriangleFragShaderModule);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    //pipelineBuilder.enable_blending_alphablend();
    //pipelineBuilder.enable_blending_additive();
    //pipelineBuilder.disable_depth_test();
    pipelineBuilder.enable_depth_test(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.set_color_attachment_format(_allocatedImage.vulkanImageFormat);
    pipelineBuilder.set_depth_format(_depthImage.vulkanImageFormat);

    _vulkanMeshPipeline = pipelineBuilder.build_pipeline(_vulkanDevice);

    vkDestroyShaderModule(_vulkanDevice, vulkanTriangleFragShaderModule, nullptr);
    vkDestroyShaderModule(_vulkanDevice, vulkanTriangleVertShaderModule, nullptr);

    _mainDeletionQueue.push_back_deleting_function([&]() {
        vkDestroyPipelineLayout(_vulkanDevice, _vulkanMeshPipelineLayout, nullptr);
        vkDestroyPipeline(_vulkanDevice, _vulkanMeshPipeline, nullptr);
        });
}

void VulkanEngine::background_pipelines_init() {
    VkPipelineLayoutCreateInfo vulkanComputePipelineLayout{};
    vulkanComputePipelineLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    vulkanComputePipelineLayout.pNext = nullptr;
    vulkanComputePipelineLayout.pSetLayouts = &_vulkanImageDescriptorSetLayout;
    vulkanComputePipelineLayout.setLayoutCount = 1;
    VkPushConstantRange vulkanPushConstantRange {};
    vulkanPushConstantRange.offset = 0;
    vulkanPushConstantRange.size = sizeof(ComputePushConstants);
    vulkanPushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    vulkanComputePipelineLayout.pPushConstantRanges = &vulkanPushConstantRange;
    vulkanComputePipelineLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_vulkanDevice, &vulkanComputePipelineLayout, nullptr, &_gradientPipelineLayout));
    
    VkShaderModule vulkanComputeGradientShaderModule;
    if (!vkutil::load_shader_module("../../vulkan-base/shaders/gradient_color.comp.spv", _vulkanDevice, &vulkanComputeGradientShaderModule)) // only for windows + msvc folders
        fmt::print("Error during compute shaders build \n");
    
    VkShaderModule vulkanComputeNightSkyShaderModule;
    if (!vkutil::load_shader_module("../../vulkan-base/shaders/sky.comp.spv", _vulkanDevice, &vulkanComputeNightSkyShaderModule)) // only for windows + msvc folders
        fmt::print("Error during compute shaders build \n");


    VkPipelineShaderStageCreateInfo vulkanPipelineShaderStageCreateInfo{};
    vulkanPipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vulkanPipelineShaderStageCreateInfo.pNext = nullptr;
    vulkanPipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    vulkanPipelineShaderStageCreateInfo.module = vulkanComputeGradientShaderModule;
    vulkanPipelineShaderStageCreateInfo.pName = "main";

    VkComputePipelineCreateInfo vulkanComputePipelineCreateInfo{};
    vulkanComputePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    vulkanComputePipelineCreateInfo.pNext = nullptr;
    vulkanComputePipelineCreateInfo.layout = _gradientPipelineLayout;
    vulkanComputePipelineCreateInfo.stage = vulkanPipelineShaderStageCreateInfo;

    ComputeEffect gradientEffect;
    gradientEffect.layout = _gradientPipelineLayout;
    gradientEffect.name = "gradient";
    gradientEffect.data = {};
    gradientEffect.data.data1 = glm::vec4(1, 0, 0, 1);
    gradientEffect.data.data2 = glm::vec4(0, 0, 1, 1);

    VK_CHECK(vkCreateComputePipelines(_vulkanDevice, VK_NULL_HANDLE,1,&vulkanComputePipelineCreateInfo,nullptr,&gradientEffect.pipeline));
    vulkanComputePipelineCreateInfo.stage.module = vulkanComputeNightSkyShaderModule;   

    ComputeEffect nightSkyEffect;
    nightSkyEffect.layout = _gradientPipelineLayout;
    nightSkyEffect.name = "night_sky";
    nightSkyEffect.data = {};
    nightSkyEffect.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    VK_CHECK(vkCreateComputePipelines(_vulkanDevice, VK_NULL_HANDLE, 1, &vulkanComputePipelineCreateInfo, nullptr, &nightSkyEffect.pipeline));

    backgroundEffects.push_back(gradientEffect); 
    backgroundEffects.push_back(nightSkyEffect);

    vkDestroyShaderModule(_vulkanDevice, vulkanComputeGradientShaderModule, nullptr);
    vkDestroyShaderModule(_vulkanDevice, vulkanComputeNightSkyShaderModule, nullptr);
    _mainDeletionQueue.push_back_deleting_function([&]() {
        vkDestroyPipelineLayout(_vulkanDevice, _gradientPipelineLayout, nullptr);
        for (auto effect = backgroundEffects.rbegin(); effect != backgroundEffects.rend(); effect++) 
            vkDestroyPipeline(_vulkanDevice, effect->pipeline, nullptr);
        });
}

void VulkanEngine::imgui_init() {
    VkDescriptorPoolSize vulkanDescriptorPoolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER,1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,1000}
    };
    VkDescriptorPoolCreateInfo vulkanDescriptorPoolCreateInfo = {};
    vulkanDescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    vulkanDescriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    vulkanDescriptorPoolCreateInfo.maxSets = 1000;
    vulkanDescriptorPoolCreateInfo.poolSizeCount = (uint32_t)std::size(vulkanDescriptorPoolSizes);
    vulkanDescriptorPoolCreateInfo.pPoolSizes = vulkanDescriptorPoolSizes;    
    VkDescriptorPool vulkanImguiDescriptorPool;
    VK_CHECK(vkCreateDescriptorPool(_vulkanDevice, &vulkanDescriptorPoolCreateInfo, nullptr, &vulkanImguiDescriptorPool));

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(_window);

    ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
    imguiInitInfo.Instance = _vulkanInstance;
    imguiInitInfo.PhysicalDevice = _selectedGPU;
    imguiInitInfo.Device = _vulkanDevice;
    imguiInitInfo.Queue = _vulkanGraphicsQueue;
    imguiInitInfo.DescriptorPool = vulkanImguiDescriptorPool;
    imguiInitInfo.MinImageCount = 3;
    imguiInitInfo.ImageCount = 3;
    imguiInitInfo.UseDynamicRendering = true;
    imguiInitInfo.ColorAttachmentFormat = _vulkanSwapchainImageFormat;
    imguiInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&imguiInitInfo, VK_NULL_HANDLE);
    immediate_command_submit(
        [&](VkCommandBuffer vulkanCommandBuffer) { 
            ImGui_ImplVulkan_CreateFontsTexture(vulkanCommandBuffer); 
        });
    ImGui_ImplVulkan_DestroyFontUploadObjects();
    _mainDeletionQueue.push_back_deleting_function([=]() {
        vkDestroyDescriptorPool(_vulkanDevice, vulkanImguiDescriptorPool, nullptr);
        ImGui_ImplVulkan_Shutdown();
        });
}

AllocatedBuffer VulkanEngine::create_allocated_buffer(size_t allocationSize, VkBufferUsageFlags bufferUsageFlags, VmaMemoryUsage allocationMemoryUsage) {
    VkBufferCreateInfo vulkanBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vulkanBufferCreateInfo.pNext = nullptr;
    vulkanBufferCreateInfo.size = allocationSize;
    vulkanBufferCreateInfo.usage = bufferUsageFlags;

    VmaAllocationCreateInfo vulkanMemoryAllocationCreateInfo = {};
    vulkanMemoryAllocationCreateInfo.usage = allocationMemoryUsage;
    vulkanMemoryAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    AllocatedBuffer newAllocatedBuffer;
    VK_CHECK(vmaCreateBuffer(_vulkanMemoryAllocator, &vulkanBufferCreateInfo, &vulkanMemoryAllocationCreateInfo,
        &newAllocatedBuffer.vulkanBuffer, &newAllocatedBuffer.vulkanMemoryAllocation, &newAllocatedBuffer.vulkanMemoryAllocationInfo));
    return newAllocatedBuffer;
}

void VulkanEngine::default_data_init() {
    //upload_2D_rectangle_to_GPU();
    uint32_t white = 0xFFFFFFFF;
    _whiteImage = create_allocated_image_with_data((void*)&white, VkExtent3D{ 1,1,1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    uint32_t grey = 0xAAAAAAFF;
    _greyImage = create_allocated_image_with_data((void*)&grey, VkExtent3D{ 1,1,1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    uint32_t black = 0x000000FF;
    _blackImage = create_allocated_image_with_data((void*)&black, VkExtent3D{ 1,1,1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t magneta = 0xFF00FFFF;
    std::array<uint32_t, 16 * 16> pixels;
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magneta : black;
        }
    }
    _errorCheckboardImage = create_allocated_image_with_data(pixels.data(), VkExtent3D{ 16,16,1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    VkSamplerCreateInfo samplerCreateInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(_vulkanDevice, &samplerCreateInfo, nullptr, &_defaultSamplerNearest);

    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(_vulkanDevice, &samplerCreateInfo, nullptr, &_defaultSamplerLinear);
    testMeshes = loadGLTFMeshes(this, "../../vulkan-base/assets/basicmesh.glb").value();

    GLTFMetalRoughness::MaterialResources materialResources;
    materialResources.colorImage = _whiteImage;
    materialResources.colorSampler = _defaultSamplerLinear;
    materialResources.metalRoughnessImage = _whiteImage;
    materialResources.metalRoughnessSampler = _defaultSamplerLinear;

    AllocatedBuffer materialConstants = create_allocated_buffer(sizeof(GLTFMetalRoughness::MaterialConstants),
                                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    GLTFMetalRoughness::MaterialConstants* sceneUniformData = (GLTFMetalRoughness::MaterialConstants*)materialConstants.vulkanMemoryAllocation->GetMappedData();
    sceneUniformData->colorFactors = glm::vec4{ 1, 1, 1, 1 };
    sceneUniformData->metalRoughnessFactors = glm::vec4{ 1,0.5,0,0 };
    _mainDeletionQueue.push_back_deleting_function([=, this]() {
        destroy_allocated_buffer(materialConstants);
        });
    materialResources.dataBuffer = materialConstants.vulkanBuffer;
    materialResources.dataBufferOffset = 0;
    defaultMaterialData = metalRoughnessMaterial.write_material(_vulkanDevice, MaterialType::MainColor, materialResources, globalDescriptorAllocator);

    for (auto& mesh : testMeshes) {
        std::shared_ptr<MeshNode> newMeshNode = std::make_shared<MeshNode>();
        newMeshNode->meshAsset = mesh;
        newMeshNode->localTransform = glm::mat4{ 1.f };
        newMeshNode->worldTransform = glm::mat4{ 1.f };
        for (auto& face : newMeshNode->meshAsset->surfaces) {
            face.gltfMaterial = std::make_shared<GLTFMaterial>(defaultMaterialData);
        }
        loadedNodes[mesh->name] = std::move(newMeshNode);
    }
}

void VulkanEngine::upload_2D_rectangle_to_GPU() {
    std::array<Vertex3D, 4> rectangle_vertices;
    rectangle_vertices[0].position = { 0.5,-0.5,0 };
    rectangle_vertices[1].position = { 0.5,0.5,0 };
    rectangle_vertices[2].position = { -0.5,-0.5,0 };
    rectangle_vertices[3].position = { -0.5,0.5,0 };
    rectangle_vertices[0].color = { 0,0,0,1 };
    rectangle_vertices[1].color = { 0.5,0.5,0.5,1 };
    rectangle_vertices[2].color = { 1,0,0,1 };
    rectangle_vertices[3].color = { 0,1,0,1 };
    std::array<uint32_t, 6> rectangle_indices;
    rectangle_indices[0] = 0;
    rectangle_indices[1] = 1;
    rectangle_indices[2] = 2;
    rectangle_indices[3] = 2;
    rectangle_indices[4] = 1;
    rectangle_indices[5] = 3;
    rectangle = upload_mesh_to_GPU(rectangle_indices, rectangle_vertices);
}

AllocatedImage VulkanEngine::create_allocated_image(VkExtent3D size, VkFormat format, VkImageUsageFlags imageUsageFlags, bool mipmapped)
{
    AllocatedImage newAllocatedImage;
    newAllocatedImage.vulkanImageFormat = format;
    newAllocatedImage.vulkanImageExtent3D = size;
    VkImageCreateInfo vulkanImageCreateInfo = vkinit::image_create_info(format, imageUsageFlags, size);
    if (mipmapped)
        vulkanImageCreateInfo.mipLevels = static_cast<uint32_t>(std::log2(std::max(size.width, size.height))) + 1;
    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vmaCreateImage(_vulkanMemoryAllocator, &vulkanImageCreateInfo, &allocationCreateInfo, 
                            &newAllocatedImage.vulkanImage, &newAllocatedImage.vulkanMemoryAllocation, nullptr));
    VkImageAspectFlags imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT)
        imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    VkImageViewCreateInfo imageViewCreateInfo = vkinit::imageview_create_info(format, newAllocatedImage.vulkanImage, imageAspectFlags);
    imageViewCreateInfo.subresourceRange.levelCount = vulkanImageCreateInfo.mipLevels;
    VK_CHECK(vkCreateImageView(_vulkanDevice, &imageViewCreateInfo, nullptr, &newAllocatedImage.vulkanImageView));
    return newAllocatedImage;
}

AllocatedImage VulkanEngine::create_allocated_image_with_data(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags imageUsageFlags, bool mipmapped)
{
    size_t dataSize = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadBuffer = create_allocated_buffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    memcpy(uploadBuffer.vulkanMemoryAllocationInfo.pMappedData, data, dataSize);
    AllocatedImage newImage = create_allocated_image(size, format, 
                                                     imageUsageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                     mipmapped);
    immediate_command_submit([&](VkCommandBuffer vulkanCommandBuffer) {
        vkutil::image_transition(vulkanCommandBuffer, newImage.vulkanImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy bufferImageRegionCopy = {};
        bufferImageRegionCopy.bufferOffset = 0;
        bufferImageRegionCopy.bufferRowLength = 0;
        bufferImageRegionCopy.bufferImageHeight = 0;
        bufferImageRegionCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferImageRegionCopy.imageSubresource.mipLevel = 0;
        bufferImageRegionCopy.imageSubresource.baseArrayLayer = 0;
        bufferImageRegionCopy.imageSubresource.layerCount = 1;
        bufferImageRegionCopy.imageExtent = size;
        vkCmdCopyBufferToImage(vulkanCommandBuffer, uploadBuffer.vulkanBuffer, newImage.vulkanImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageRegionCopy);
        vkutil::image_transition(vulkanCommandBuffer, newImage.vulkanImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });
    destroy_allocated_buffer(uploadBuffer);
    return newImage;
}
#pragma endregion 
#pragma region Cleanup
void VulkanEngine::destroy_allocated_buffer(const AllocatedBuffer& buffer) {
    vmaDestroyBuffer(_vulkanMemoryAllocator, buffer.vulkanBuffer, buffer.vulkanMemoryAllocation);
}

void VulkanEngine::destroy_allocated_image(const AllocatedImage& image) {
    vkDestroyImageView(_vulkanDevice, image.vulkanImageView, nullptr);
    vmaDestroyImage(_vulkanMemoryAllocator, image.vulkanImage, image.vulkanMemoryAllocation);
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) {
        vkDeviceWaitIdle(_vulkanDevice);

        for (int i = 0; i < FRAME_OVERLAP; i++) {
            _frames[i]._deletionQueue.flushAll();
        }
        for (int i = 0; i < FRAME_OVERLAP; i++) {
            vkDestroyCommandPool(_vulkanDevice, _frames[i]._vulkanCommandPool, nullptr);
            vkDestroyFence(_vulkanDevice, _frames[i]._vulkanRenderingFence, nullptr);
            vkDestroySemaphore(_vulkanDevice, _frames[i]._vulkanRenderingSemaphore, nullptr);
            vkDestroySemaphore(_vulkanDevice, _frames[i]._vulkanSwapchainSemaphore, nullptr);
        }

        swapchain_destroy();
        vkDestroySurfaceKHR(_vulkanInstance, _windowSurface, nullptr);
        vkDestroyDevice(_vulkanDevice, nullptr);
        vkb::destroy_debug_utils_messenger(_vulkanInstance, _debugMessenger);
        vkDestroyInstance(_vulkanInstance, nullptr);
        SDL_DestroyWindow(_window);
    }
    loadedEngine = nullptr;
}
#pragma endregion
#pragma region Runtime
void VulkanEngine::draw()
{
    update_scene();
    VK_CHECK(vkWaitForFences(_vulkanDevice, 1, &get_current_frame()._vulkanRenderingFence, true, 1e+9));

    _vulkanDrawExtent.height = std::min(_vulkanSwapchainExtent.height, _allocatedImage.vulkanImageExtent3D.height) * renderScale;
    _vulkanDrawExtent.width = std::min(_vulkanSwapchainExtent.width, _allocatedImage.vulkanImageExtent3D.width) * renderScale;
    // we wait for previous frame
    VK_CHECK(vkWaitForFences(_vulkanDevice, 1, &get_current_frame()._vulkanRenderingFence, true, 1e+9));
    //deleting previous frame data
    get_current_frame()._deletionQueue.flushAll();
    get_current_frame()._frameDescriptors.clear_pools(_vulkanDevice);

    VK_CHECK(vkResetFences(_vulkanDevice, 1, &get_current_frame()._vulkanRenderingFence));
    // we get image index
    uint32_t vulkanSwapchainImgIndex;
    //VK_CHECK(vkAcquireNextImageKHR(_vulkanDevice, _vulkanSwapchain, 1e+9, get_current_frame()._vulkanSwapchainSemaphore,
                                    //nullptr, &vulkanSwapchainImgIndex));
    VkResult result = vkAcquireNextImageKHR(_vulkanDevice, _vulkanSwapchain, 1e+9, get_current_frame()._vulkanSwapchainSemaphore,
        nullptr, &vulkanSwapchainImgIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        resizeRequested = true;
        return;
    }
    // resetting command buffer
    VkCommandBuffer vulkanCommandBuffer = get_current_frame()._mainVulkanCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(vulkanCommandBuffer, 0));
    VkCommandBufferBeginInfo vulkanCommandBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); // we use buffer only once per frame
    
    // flashing screen
    _vulkanImageExtent2D.width = _allocatedImage.vulkanImageExtent3D.width;
    _vulkanImageExtent2D.height = _allocatedImage.vulkanImageExtent3D.height;
    VK_CHECK(vkBeginCommandBuffer(vulkanCommandBuffer, &vulkanCommandBeginInfo));

    vkutil::image_transition(vulkanCommandBuffer, _allocatedImage.vulkanImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    draw_background(vulkanCommandBuffer);

    vkutil::image_transition(vulkanCommandBuffer, _allocatedImage.vulkanImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::image_transition(vulkanCommandBuffer, _depthImage.vulkanImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    draw_geometry(vulkanCommandBuffer);

    vkutil::image_transition(vulkanCommandBuffer, _allocatedImage.vulkanImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::image_transition(vulkanCommandBuffer, _vulkanSwapchainImages[vulkanSwapchainImgIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL); // general is not the best for rendering, its just good for writing from compute shader
    vkutil::copy_image_to_image(vulkanCommandBuffer, _allocatedImage.vulkanImage, _vulkanSwapchainImages[vulkanSwapchainImgIndex], _vulkanImageExtent2D, _vulkanSwapchainExtent);
    vkutil::image_transition(vulkanCommandBuffer, _vulkanSwapchainImages[vulkanSwapchainImgIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    draw_imgui(vulkanCommandBuffer, _vulkanSwapchainImageViews[vulkanSwapchainImgIndex]);
    vkutil::image_transition(vulkanCommandBuffer, _vulkanSwapchainImages[vulkanSwapchainImgIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    VK_CHECK(vkEndCommandBuffer(vulkanCommandBuffer));

    // syncronizing swapchain and command queue
    VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::command_buffer_submit_info(vulkanCommandBuffer);
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._vulkanSwapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._vulkanRenderingSemaphore);
    VkSubmitInfo2 submitInfo = vkinit::submit_info(&commandBufferSubmitInfo, &signalInfo, &waitInfo);
    VK_CHECK(vkQueueSubmit2(_vulkanGraphicsQueue, 1, &submitInfo, get_current_frame()._vulkanRenderingFence)); 

    // drawing
    VkPresentInfoKHR vulkanPresentInfo = {};
    vulkanPresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    vulkanPresentInfo.pNext = nullptr;
    vulkanPresentInfo.pSwapchains = &_vulkanSwapchain;
    vulkanPresentInfo.swapchainCount = 1;
    vulkanPresentInfo.pWaitSemaphores = &get_current_frame()._vulkanRenderingSemaphore;
    vulkanPresentInfo.waitSemaphoreCount = 1;
    vulkanPresentInfo.pImageIndices = &vulkanSwapchainImgIndex;
    //VK_CHECK(vkQueuePresentKHR(_vulkanGraphicsQueue, &vulkanPresentInfo));
    VkResult presentResult = vkQueuePresentKHR(_vulkanGraphicsQueue, &vulkanPresentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
        resizeRequested = true;

    _frameNumber++;
}

void VulkanEngine::draw_background(VkCommandBuffer vulkanCommandBuffer) {
    /*VkClearColorValue clearValue;
    float flash = abs(sin(_frameNumber / 60.f));
    clearValue = { { 0.f, 0.f, flash, 1.f} };
    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(vulkanCommandBuffer, _allocatedImage.vulkanImage, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);*/
    /*
    vkCmdBindPipeline(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipeline); // binding pipeline 
    vkCmdBindDescriptorSets(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_vulkanImageDescriptorSet, 0, nullptr); // binding descriptor
    vkCmdDispatch(vulkanCommandBuffer, std::ceil(_vulkanImageExtent2D.width / 16.0), std::ceil(_vulkanImageExtent2D.height / 16.0), 1); //exeuting pipeline dispatch
    */
    /*
    vkCmdBindPipeline(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipeline);
    vkCmdBindDescriptorSets(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_vulkanImageDescriptorSet, 0, nullptr);
    ComputePushConstants pushConstants;
    pushConstants.data1 = glm::vec4(1, 0, 0, 1);
    pushConstants.data2 = glm::vec4(0, 0, 1, 1);

    vkCmdPushConstants(vulkanCommandBuffer, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &pushConstants);
    vkCmdDispatch(vulkanCommandBuffer, std::ceil(_vulkanImageExtent2D.width / 16.0), std::ceil(_vulkanImageExtent2D.height / 16.0), 1);*/
    ComputeEffect& currentEffect = backgroundEffects[currentBackgroundEffect];
   
    vkCmdBindPipeline(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, currentEffect.pipeline);
    vkCmdBindDescriptorSets(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_vulkanImageDescriptorSet, 0, nullptr);
    vkCmdPushConstants(vulkanCommandBuffer, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &currentEffect.data);
    vkCmdDispatch(vulkanCommandBuffer, std::ceil(_vulkanImageExtent2D.width / 16.0), std::ceil(_vulkanImageExtent2D.height / 16.0), 1); 
}

void VulkanEngine::draw_imgui(VkCommandBuffer vulkanCommandBuffer, VkImageView targetVulkanImageView) {
    VkRenderingAttachmentInfo vulkanColorRenderingAttachmentInfo = vkinit::attachment_info(targetVulkanImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingInfo vulkanRenderingInfo = vkinit::rendering_info(_vulkanSwapchainExtent, &vulkanColorRenderingAttachmentInfo, nullptr);
    vkCmdBeginRendering(vulkanCommandBuffer, &vulkanRenderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vulkanCommandBuffer);
    vkCmdEndRendering(vulkanCommandBuffer);
}

void VulkanEngine::draw_geometry(VkCommandBuffer vulkanCommandBuffer) {
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_allocatedImage.vulkanImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.vulkanImageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    VkRenderingInfo vulkanRenderingInfo = vkinit::rendering_info(_vulkanImageExtent2D, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(vulkanCommandBuffer, &vulkanRenderingInfo);

    // drawing more complex shapes
    vkCmdBindPipeline(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _vulkanMeshPipeline);

    VkViewport vulkanViewport = {};
    vulkanViewport.x = 0;
    vulkanViewport.y = 0;
    vulkanViewport.width = _vulkanImageExtent2D.width;
    vulkanViewport.height = _vulkanImageExtent2D.height;
    vulkanViewport.minDepth = 0.f;
    vulkanViewport.maxDepth = 1.f;
    vkCmdSetViewport(vulkanCommandBuffer, 0, 1, &vulkanViewport);
    VkRect2D vulkanScissor = {};
    vulkanScissor.offset.x = 0;
    vulkanScissor.offset.y = 0;
    vulkanScissor.extent.width = _vulkanImageExtent2D.width;
    vulkanScissor.extent.height = _vulkanImageExtent2D.height;
    vkCmdSetScissor(vulkanCommandBuffer, 0, 1, &vulkanScissor);

    /*
    VkDescriptorSet imageDescriptorSet = get_current_frame()._frameDescriptors.allocate(_vulkanDevice, _singleImageDescriptorLayout);
    {
        DescriptorWriter descriptorWriter;
        descriptorWriter.write_image(0, _errorCheckboardImage.vulkanImageView, _defaultSamplerNearest, 
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        descriptorWriter.update_set(_vulkanDevice, imageDescriptorSet);
    }

    vkCmdBindDescriptorSets(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _vulkanMeshPipelineLayout, 0, 1, &imageDescriptorSet, 0, nullptr);
        
    // flipping viewport because Y axis in Vulkan look down by default
    glm::mat4 viewMatrix = glm::translate(glm::vec3{ 0, 0, -5 });
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(70.f), (float)_vulkanImageExtent2D.width / (float)_vulkanImageExtent2D.height, 10000.f, 0.1f);
    projectionMatrix[1][1] *= -1;

    GPUDrawingPushConstants gpuDrawPushConstants;
    gpuDrawPushConstants.worldMatrix = projectionMatrix * viewMatrix;
    // drawing a test monke mesh
    gpuDrawPushConstants.vertexBuffer = testMeshes[2]->meshBuffers.vertexBufferAdress;
    vkCmdPushConstants(vulkanCommandBuffer, _vulkanMeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawingPushConstants), &gpuDrawPushConstants);
    vkCmdBindIndexBuffer(vulkanCommandBuffer, testMeshes[2]->meshBuffers.indexBuffer.vulkanBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(vulkanCommandBuffer, testMeshes[2]->surfaces[0].count, 1, testMeshes[2]->surfaces[0].startIndex, 0, 0);*/
    
    AllocatedBuffer gpuSceneDataBuffer = create_allocated_buffer(sizeof(GPUSceneData),
                                                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
    get_current_frame()._deletionQueue.push_back_deleting_function([=, this]() {
        destroy_allocated_buffer(gpuSceneDataBuffer);
        });

    GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.vulkanMemoryAllocation->GetMappedData();
    *sceneUniformData = sceneData;
    VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_vulkanDevice, _gpuSceneDataDescriptorLayout);
    DescriptorWriter descriptorWriter;
    descriptorWriter.write_buffer(0, gpuSceneDataBuffer.vulkanBuffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptorWriter.update_set(_vulkanDevice, globalDescriptor);

    for (const RenderableObject& renderableObject : mainDrawContext.OpaqueSurfaces)
    {
        vkCmdBindPipeline(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderableObject.renderableMaterial->pipeline->pipeline);
        vkCmdBindDescriptorSets(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderableObject.renderableMaterial->pipeline->pipelineLayout, 
                                0, 1, &globalDescriptor, 0, nullptr);
        vkCmdBindDescriptorSets(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderableObject.renderableMaterial->pipeline->pipelineLayout, 
                                1, 1, &renderableObject.renderableMaterial->materialSet, 0, nullptr);
        
        vkCmdBindIndexBuffer(vulkanCommandBuffer, renderableObject.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        GPUDrawingPushConstants gpuDrawingPushConstants;
        gpuDrawingPushConstants.vertexBuffer = renderableObject.vertexBufferAddress;
        gpuDrawingPushConstants.worldMatrix = renderableObject.objectTransform;
        vkCmdPushConstants(vulkanCommandBuffer, renderableObject.renderableMaterial->pipeline->pipelineLayout, 
                            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawingPushConstants), &gpuDrawingPushConstants);
        
        vkCmdDrawIndexed(vulkanCommandBuffer, renderableObject.indexCount, 1, renderableObject.firstIndex, 0, 0);
    }

    vkCmdEndRendering(vulkanCommandBuffer);
}

void VulkanEngine::update_scene() {
    mainDrawContext.OpaqueSurfaces.clear();
    loadedNodes["Suzanne"]->Draw(glm::mat4{ 1.f }, mainDrawContext); // Suzanne = monke
    for (int x = -3; x < 4; x++) {
        glm::mat4 cubeScale = glm::scale(glm::vec3{ 0.2 });
        glm::mat4 cubeTranslation = glm::translate(glm::vec3{ x,1,0 });
        loadedNodes["Cube"]->Draw(cubeTranslation * cubeScale, mainDrawContext);
    }
    sceneData.viewMatrix = glm::translate(glm::vec3{ 0,0,-5 });
    sceneData.projectionMatrix = glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 1e+4f, 0.1f);
    sceneData.projectionMatrix[1][1] *= -1;
    sceneData.viewToProjectionMatrix = sceneData.projectionMatrix * sceneData.viewMatrix;
    sceneData.ambientColor = glm::vec4(.1f);
    sceneData.sunlightColor = glm::vec4(1.f);
    sceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;
    while (!bQuit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) bQuit = true;
            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) 
                    stopRendering = true;
                if (e.window.event == SDL_WINDOWEVENT_RESTORED)
                    stopRendering = false;
            }
            ImGui_ImplSDL2_ProcessEvent(&e);
        }
        if (stopRendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if (resizeRequested)
            swapchain_resize();
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(_window);
        ImGui::NewFrame();
        if (ImGui::Begin("background")) {
            ImGui::SliderFloat("Render Scale", &renderScale, 0.3f, 1.f);
            ComputeEffect& selectedEffect = backgroundEffects[currentBackgroundEffect];
            ImGui::Text("Selected effect: ", selectedEffect.name);
            ImGui::SliderInt("(Effect Index)", &currentBackgroundEffect, 0, backgroundEffects.size()-1);
            ImGui::InputFloat4("parameter 1", (float*)&selectedEffect.data.data1);
            ImGui::InputFloat4("parameter 2", (float*)&selectedEffect.data.data2);
            ImGui::InputFloat4("parameter 3", (float*)&selectedEffect.data.data3);
            ImGui::InputFloat4("parameter 4", (float*)&selectedEffect.data.data4);
            ImGui::End();
        }
        //ImGui::ShowDemoWindow(); - demo window
        ImGui::Render();
        draw();
    }
}
#pragma endregion

void VulkanEngine::immediate_command_submit(std::function<void(VkCommandBuffer vulkanCommandBuffer)>&& function) {
    VK_CHECK(vkResetFences(_vulkanDevice, 1, &_immediateVulkanFence));
    VK_CHECK(vkResetCommandBuffer(_immediateVulkanCommandBuffer, 0));
    VkCommandBuffer vulkanCommandBuffer = _immediateVulkanCommandBuffer;
    VkCommandBufferBeginInfo vulkanCommandBufferBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(vulkanCommandBuffer, &vulkanCommandBufferBeginInfo));
    function(vulkanCommandBuffer);
    VK_CHECK(vkEndCommandBuffer(vulkanCommandBuffer));
    VkCommandBufferSubmitInfo vulkanCommandBufferSubmitInfo = vkinit::command_buffer_submit_info(vulkanCommandBuffer);
    VkSubmitInfo2 vulkanSubmitInfo = vkinit::submit_info(&vulkanCommandBufferSubmitInfo, nullptr, nullptr);
    VK_CHECK(vkQueueSubmit2(_vulkanGraphicsQueue, 1, &vulkanSubmitInfo, _immediateVulkanFence));
    VK_CHECK(vkWaitForFences(_vulkanDevice, 1, &_immediateVulkanFence, true, 1e+9 - 1));
}

GPUMeshBuffers VulkanEngine::upload_mesh_to_GPU(std::span<uint32_t> indices, std::span<Vertex3D> vertices) {
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex3D);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
    GPUMeshBuffers createdSurface;
    createdSurface.vertexBuffer = create_allocated_buffer(vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    VkBufferDeviceAddressInfo vulkanBufferDeviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                            .buffer = createdSurface.vertexBuffer.vulkanBuffer };
    createdSurface.vertexBufferAdress = vkGetBufferDeviceAddress(_vulkanDevice, &vulkanBufferDeviceAdressInfo);
    createdSurface.indexBuffer = create_allocated_buffer(indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    AllocatedBuffer stagingBuffer = create_allocated_buffer(vertexBufferSize + indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY);
    void* data = stagingBuffer.vulkanMemoryAllocation->GetMappedData();
    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);
    immediate_command_submit([&](VkCommandBuffer vulkanCommandBuffer) {
        VkBufferCopy vertexCopy{ 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;
        vkCmdCopyBuffer(vulkanCommandBuffer, stagingBuffer.vulkanBuffer, createdSurface.vertexBuffer.vulkanBuffer, 1, &vertexCopy);
        VkBufferCopy indexCopy{ 0 };
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;
        vkCmdCopyBuffer(vulkanCommandBuffer, stagingBuffer.vulkanBuffer, createdSurface.indexBuffer.vulkanBuffer, 1, &indexCopy);
        });
    destroy_allocated_buffer(stagingBuffer);
    _mainDeletionQueue.push_back_deleting_function([=] {
        destroy_allocated_buffer(createdSurface.vertexBuffer);
        destroy_allocated_buffer(createdSurface.indexBuffer);
        });
    return createdSurface;
    // using gradual gpu-cpu-gpu logic instead of more efficient background threading for simplicity
}

void GLTFMetalRoughness::build_pipelines(VulkanEngine* vulkanEngine) {
    VkShaderModule meshFragmentShaderModule;
    if (!vkutil::load_shader_module("../../vulkan-base/shaders/mesh.frag.spv", vulkanEngine->_vulkanDevice, &meshFragmentShaderModule))
        fmt::print("Error during fragment shaders build \n");

    VkShaderModule meshVertexShaderModule;
    if (!vkutil::load_shader_module("../../vulkan-base/shaders/mesh.vert.spv", vulkanEngine->_vulkanDevice, &meshVertexShaderModule)) // only for windows + msvc folders
        fmt::print("Error during vertex shaders build \n");

    VkPushConstantRange matrixPushConstantRange{};
    matrixPushConstantRange.offset = 0;
    matrixPushConstantRange.size = sizeof(GPUDrawingPushConstants);
    matrixPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder descriptorLayoutBuilder;
    descriptorLayoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptorLayoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptorLayoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    materialDescriptorSetLayout = descriptorLayoutBuilder.build(vulkanEngine->_vulkanDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout descriptorSetLayouts[] = {vulkanEngine->_gpuSceneDataDescriptorLayout, materialDescriptorSetLayout};

    VkPipelineLayoutCreateInfo meshPipelineLayoutCreateInfo = vkinit::pipeline_layout_create_info();
    meshPipelineLayoutCreateInfo.setLayoutCount = 2;
    meshPipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts;
    meshPipelineLayoutCreateInfo.pPushConstantRanges = &matrixPushConstantRange;
    meshPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    
    VkPipelineLayout newPipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(vulkanEngine->_vulkanDevice, &meshPipelineLayoutCreateInfo, nullptr, &newPipelineLayout));

    opaqueObjectsPipeline.pipelineLayout = newPipelineLayout;
    transparentObjectsPipeline.pipelineLayout = newPipelineLayout;

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(meshVertexShaderModule, meshFragmentShaderModule);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    //pipelineBuilder.enable_blending_alphablend();
    //pipelineBuilder.enable_blending_additive();
    //pipelineBuilder.disable_depth_test();
    pipelineBuilder.enable_depth_test(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.set_color_attachment_format(vulkanEngine->_allocatedImage.vulkanImageFormat);
    pipelineBuilder.set_depth_format(vulkanEngine->_depthImage.vulkanImageFormat);

    pipelineBuilder._vulkanPipelineLayout = newPipelineLayout;

    opaqueObjectsPipeline.pipeline = pipelineBuilder.build_pipeline(vulkanEngine->_vulkanDevice);

    pipelineBuilder.enable_blending_additive();

    pipelineBuilder.enable_depth_test(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparentObjectsPipeline.pipeline = pipelineBuilder.build_pipeline(vulkanEngine->_vulkanDevice);

    vkDestroyShaderModule(vulkanEngine->_vulkanDevice, meshFragmentShaderModule, nullptr);
    vkDestroyShaderModule(vulkanEngine->_vulkanDevice, meshVertexShaderModule, nullptr);

    vulkanEngine->_mainDeletionQueue.push_back_deleting_function([&]() {
        vkDestroyPipelineLayout(vulkanEngine->_vulkanDevice, vulkanEngine->_vulkanMeshPipelineLayout, nullptr);
        vkDestroyPipeline(vulkanEngine->_vulkanDevice, vulkanEngine->_vulkanMeshPipeline, nullptr);
        });
}

RenderableMaterial GLTFMetalRoughness::write_material(VkDevice vulkanDevice, MaterialType type, 
                                                      const MaterialResources& resources, ScalableDescriptorAllocator& descriptorAllocator) {
    RenderableMaterial materialInfo;
    materialInfo.materialType = type;
    if (type == MaterialType::Transparent) 
        materialInfo.pipeline = &transparentObjectsPipeline;
    else 
        materialInfo.pipeline = &opaqueObjectsPipeline;

    materialInfo.materialSet = descriptorAllocator.allocate(vulkanDevice, materialDescriptorSetLayout);
    descriptorWriter.clear();
    descriptorWriter.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptorWriter.write_image(1, resources.colorImage.vulkanImageView, resources.colorSampler, 
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptorWriter.write_image(2, resources.metalRoughnessImage.vulkanImageView, resources.metalRoughnessSampler, 
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptorWriter.update_set(vulkanDevice, materialInfo.materialSet);

    return materialInfo;
}

void MeshNode::Draw(const glm::mat4& topMatrix, DrawContext& drawContext) {
    glm::mat4 nodeTransform = topMatrix * worldTransform;
    for (auto& surface : meshAsset->surfaces) {
        RenderableObject face;
        face.indexCount = surface.count;
        face.firstIndex = surface.startIndex;
        face.indexBuffer = meshAsset->meshBuffers.indexBuffer.vulkanBuffer;
        face.renderableMaterial = &surface.gltfMaterial->materialData;
        face.objectTransform = nodeTransform;
        face.vertexBufferAddress = meshAsset->meshBuffers.vertexBufferAdress;

        drawContext.OpaqueSurfaces.push_back(face);
    }
    HierarchyNode::Draw(topMatrix, drawContext);
}
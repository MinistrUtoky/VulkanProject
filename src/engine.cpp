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
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

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

    _mainDeletionQueue.push_back_deleting_function([=]() {
        vkDestroyImageView(_vulkanDevice, _allocatedImage.vulkanImageView, nullptr);
        vmaDestroyImage(_vulkanMemoryAllocator, _allocatedImage.vulkanImage, _allocatedImage.vulkanMemoryAllocation); });
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

void VulkanEngine::commands_init() {
    // creating pool and buffer
    VkCommandPoolCreateInfo vulkanCommandPoolInfo = {};
    vulkanCommandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    vulkanCommandPoolInfo.pNext = nullptr;
    vulkanCommandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vulkanCommandPoolInfo.queueFamilyIndex = _vulkanGraphicsQueueFamily;
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateCommandPool(_vulkanDevice, &vulkanCommandPoolInfo, nullptr, &_frames[i]._vulkanCommandPool));
        /*VkCommandBufferAllocateInfo commandAllocationInfo = {};
        commandAllocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandAllocationInfo.pNext = nullptr;
        commandAllocationInfo.commandPool = _frames[i]._vulkanCommandPool;
        commandAllocationInfo.commandBufferCount = 1;
        commandAllocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; */
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
    std::vector<DescriptorAllocator::PoolSizeRatio> poolSizeRatios = { {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1} };
    globalDescriptorAllocator.initialize_pool(_vulkanDevice, 10, poolSizeRatios);
    { 
        DescriptorLayoutBuilder descriptorLayoutBuilder; 
        descriptorLayoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); 
        _vulkanImageDescriptorSetLayout = descriptorLayoutBuilder.build(_vulkanDevice, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    _vulkanImageDescriptorSet = globalDescriptorAllocator.allocate(_vulkanDevice, _vulkanImageDescriptorSetLayout);
    VkDescriptorImageInfo vulkanDescriptorImageInfo{};
    vulkanDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    vulkanDescriptorImageInfo.imageView = _allocatedImage.vulkanImageView;
    VkWriteDescriptorSet vulkanImageWriteDescriptorSet = {};
    vulkanImageWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    vulkanImageWriteDescriptorSet.pNext = nullptr;
    vulkanImageWriteDescriptorSet.dstBinding = 0;
    vulkanImageWriteDescriptorSet.dstSet = _vulkanImageDescriptorSet;
    vulkanImageWriteDescriptorSet.descriptorCount = 1;
    vulkanImageWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    vulkanImageWriteDescriptorSet.pImageInfo = &vulkanDescriptorImageInfo;
    vkUpdateDescriptorSets(_vulkanDevice, 1, &vulkanImageWriteDescriptorSet, 0, nullptr);
}

void VulkanEngine::pipelines_init() {
    background_pipelines_init();
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
    
    VkShaderModule vulkanComputreShaderModule;
    if (!vkutil::load_shader_module("../../vulkan-base/shaders/gradient_color.comp.spv", _vulkanDevice, &vulkanComputreShaderModule)) // only for windows + msvc folders
        fmt::print("Error during compute shaders build \n");
    VkPipelineShaderStageCreateInfo vulkanPipelineShaderStageCreateInfo{};
    vulkanPipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vulkanPipelineShaderStageCreateInfo.pNext = nullptr;
    vulkanPipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    vulkanPipelineShaderStageCreateInfo.module = vulkanComputreShaderModule;
    vulkanPipelineShaderStageCreateInfo.pName = "main";

    VkComputePipelineCreateInfo vulkanComputePipelineCreateInfo{};
    vulkanComputePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    vulkanComputePipelineCreateInfo.pNext = nullptr;
    vulkanComputePipelineCreateInfo.layout = _gradientPipelineLayout;
    vulkanComputePipelineCreateInfo.stage = vulkanPipelineShaderStageCreateInfo;
    VK_CHECK(vkCreateComputePipelines(_vulkanDevice, VK_NULL_HANDLE,1,&vulkanComputePipelineCreateInfo,nullptr,&_gradientPipeline));

    vkDestroyShaderModule(_vulkanDevice, vulkanComputreShaderModule, nullptr);
    _mainDeletionQueue.push_back_deleting_function([&]() {
        vkDestroyPipelineLayout(_vulkanDevice, _gradientPipelineLayout, nullptr);
        vkDestroyPipeline(_vulkanDevice, _gradientPipeline, nullptr);
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
#pragma endregion 

void VulkanEngine::cleanup()
{
    if (_isInitialized) {
        vkDeviceWaitIdle(_vulkanDevice);

        _mainDeletionQueue.flushAll();

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

#pragma region Runtime
void VulkanEngine::draw()
{
    // we wait for previous frame
    VK_CHECK(vkWaitForFences(_vulkanDevice, 1, &get_current_frame()._vulkanRenderingFence, true, 1e+9));
    //deleting previous frame data
    get_current_frame()._deletionQueue.flushAll();

    VK_CHECK(vkResetFences(_vulkanDevice, 1, &get_current_frame()._vulkanRenderingFence));
    // we get image index
    uint32_t vulkanSwapchainImgIndex;
    VK_CHECK(vkAcquireNextImageKHR(_vulkanDevice, _vulkanSwapchain, 1e+9, get_current_frame()._vulkanSwapchainSemaphore,
                                    nullptr, &vulkanSwapchainImgIndex));
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
    VK_CHECK(vkQueuePresentKHR(_vulkanGraphicsQueue, &vulkanPresentInfo));

    _frameNumber++;
}

void VulkanEngine::draw_background(VkCommandBuffer vulkanCommandBuffer) {
    /*VkClearColorValue clearValue;
    float flash = abs(sin(_frameNumber / 60.f));
    clearValue = { { 0.f, 0.f, flash, 1.f} };
    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(vulkanCommandBuffer, _allocatedImage.vulkanImage, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);*/
    vkCmdBindPipeline(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipeline); // binding pipeline 
    vkCmdBindDescriptorSets(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_vulkanImageDescriptorSet, 0, nullptr); // binding descriptor
    vkCmdDispatch(vulkanCommandBuffer, std::ceil(_vulkanImageExtent2D.width / 16.0), std::ceil(_vulkanImageExtent2D.height / 16.0), 1); //exeuting pipeline dispatch
}

void VulkanEngine::draw_imgui(VkCommandBuffer vulkanCommandBuffer, VkImageView targetVulkanImageView) {
    VkRenderingAttachmentInfo vulkanColorRenderingAttachmentInfo = vkinit::attachment_info(targetVulkanImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingInfo vulkanRenderingInfo = vkinit::rendering_info(_vulkanSwapchainExtent, &vulkanColorRenderingAttachmentInfo, nullptr);
    vkCmdBeginRendering(vulkanCommandBuffer, &vulkanRenderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vulkanCommandBuffer);
    vkCmdEndRendering(vulkanCommandBuffer);
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
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(_window);
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        ImGui::Render();
        draw();
    }
}
#pragma endregion

void VulkanEngine::immediate_command_submit(std::function<void(VkCommandBuffer vulkanCommandBuffer)>&& function) {
    VK_CHECK(vkResetFences(_vulkanDevice,1,&_immediateVulkanFence));
    VK_CHECK(vkResetCommandBuffer(_immediateVulkanCommandBuffer,0));
    VkCommandBuffer vulkanCommandBuffer = _immediateVulkanCommandBuffer;
    VkCommandBufferBeginInfo vulkanCommandBufferBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(vulkanCommandBuffer, &vulkanCommandBufferBeginInfo));
    function(vulkanCommandBuffer);
    VK_CHECK(vkEndCommandBuffer(vulkanCommandBuffer));
    VkCommandBufferSubmitInfo vulkanCommandBufferSubmitInfo = vkinit::command_buffer_submit_info(vulkanCommandBuffer);
    VkSubmitInfo2 vulkanSubmitInfo = vkinit::submit_info(&vulkanCommandBufferSubmitInfo, nullptr, nullptr);
    VK_CHECK(vkQueueSubmit2(_vulkanGraphicsQueue, 1, &vulkanSubmitInfo, _immediateVulkanFence));
    VK_CHECK(vkWaitForFences(_vulkanDevice,1,&_immediateVulkanFence,true,1e+9-1));
}
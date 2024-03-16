﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>
#include <fmt/core.h>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>


#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)

struct AllocatedImage {
    VkImage vulkanImage;
    VkImageView vulkanImageView;
    VmaAllocation vulkanMemoryAllocation;
    VkExtent3D vulkanImageExtent3D;
    VkFormat vulkanImageFormat;
};
struct AllocatedBuffer {
    VkBuffer vulkanBuffer;
    VmaAllocation vulkanMemoryAllocation;
    VmaAllocationInfo vulkanMemoryAllocationInfo;
};
struct Vertex3D {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};
// uv_x and uv_y are interleaved because of gpu alignment limitations
struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAdress;
};
struct GPUDrawingPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};
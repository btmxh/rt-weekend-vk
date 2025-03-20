#pragma once

#define VMA_IMPLEMENTATION
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

namespace vma {

struct Image {
  VmaAllocator allocator;
  VkImage image;
  VmaAllocation allocation;
  VmaAllocationInfo alloc_info;

  Image() = default;
  ~Image() {
    if (allocator)
      vmaDestroyImage(allocator, image, allocation);
  }

  Image(const Image &) = delete;
  Image(Image &&other) {
    allocator = std::exchange(other.allocator, VK_NULL_HANDLE);
    image = std::exchange(other.image, VK_NULL_HANDLE);
    allocation = other.allocation;
    alloc_info = other.alloc_info;
  }

  Image &operator=(const Image &) = delete;
  Image &operator=(Image &&rhs) {
    using std::swap;
    swap(allocator, rhs.allocator);
    swap(image, rhs.image);
    swap(allocation, rhs.allocation);
    swap(alloc_info, rhs.alloc_info);
    return *this;
  }

  operator VkImage() const { return image; }
};

struct Buffer {
  VmaAllocator allocator;
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo alloc_info;

  Buffer() = default;
  ~Buffer() {
    if (allocator)
      vmaDestroyBuffer(allocator, buffer, allocation);
  }

  Buffer(const Buffer &) = delete;
  Buffer(Buffer &&other) {
    allocator = std::exchange(other.allocator, VK_NULL_HANDLE);
    buffer = std::exchange(other.buffer, VK_NULL_HANDLE);
    allocation = other.allocation;
    alloc_info = other.alloc_info;
  }

  Buffer &operator=(const Buffer &) = delete;
  Buffer &operator=(Buffer &&rhs) {
    using std::swap;
    swap(allocator, rhs.allocator);
    swap(buffer, rhs.buffer);
    swap(allocation, rhs.allocation);
    swap(alloc_info, rhs.alloc_info);
    return *this;
  }

  void map(auto &&callback) {
    vmaMapMemory(allocator, allocation, &alloc_info.pMappedData);
    callback(alloc_info.pMappedData);
    vmaUnmapMemory(allocator, allocation);
  }

  operator VkBuffer() const { return buffer; }
};

inline void check_vk_result(VkResult err, const char *msg) {
  if (err == VK_SUCCESS) {
    return;
  }

  std::stringstream ss;
  ss << msg << ": " << string_VkResult(err);
  throw std::runtime_error{ss.str()};
}

struct Allocator {
  VmaAllocator allocator;

  Allocator(VkInstance instance, VkPhysicalDevice phys_device,
            VkDevice device) {
    VmaAllocatorCreateInfo info{};
    info.instance = instance;
    info.physicalDevice = phys_device;
    info.device = device;
    check_vk_result(vmaCreateAllocator(&info, &allocator),
                    "Unable to create VMA allocator");
  }

  ~Allocator() { vmaDestroyAllocator(allocator); }

  Image create_image(const VkImageCreateInfo &image_info,
                     const VmaAllocationCreateInfo alloc_info) {
    Image img;
    img.allocator = allocator;
    check_vk_result(vmaCreateImage(allocator, &image_info, &alloc_info,
                                   &img.image, &img.allocation,
                                   &img.alloc_info),
                    "Unable to create image");
    return std::move(img);
  }

  Buffer create_buffer(const VkBufferCreateInfo &buffer_info,
                       const VmaAllocationCreateInfo alloc_info) {
    Buffer buf;
    buf.allocator = allocator;
    check_vk_result(vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
                                    &buf.buffer, &buf.allocation,
                                    &buf.alloc_info),
                    "Unable to create staging buffer");
    return std::move(buf);
  }

  Buffer create_staging_buffer_src(std::size_t size, const void *data) {
    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VmaAllocationCreateInfo alloc_info = {
        .flags = data != nullptr ? VMA_ALLOCATION_CREATE_MAPPED_BIT
                                 : static_cast<VmaAllocationCreateFlags>(0),
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    };

    auto buffer = create_buffer(buf_info, alloc_info);
    if (data) {
      std::memcpy(buffer.alloc_info.pMappedData, data, size);
    }

    return std::move(buffer);
  }

  Buffer create_staging_buffer_dst(std::size_t size) {
    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VmaAllocationCreateInfo alloc_info = {
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    };

    return create_buffer(buf_info, alloc_info);
  }

  Image create_image_rgba32f_2d(std::uint32_t width, std::uint32_t height) {
    VkImageCreateInfo img_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = {width, height, 1U},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    return create_image(img_info, alloc_info);
  }
};
} // namespace vma

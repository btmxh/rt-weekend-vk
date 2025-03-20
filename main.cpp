#include <VkBootstrap.h>
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include "vma.hpp"

auto create_command_pool_and_buffer(vk::raii::Device &device,
                                    std::uint32_t queue_family_index,
                                    vk::raii::CommandPool &pool,
                                    vk::raii::CommandBuffer &buffer)
    -> decltype(auto) {
  vk::CommandPoolCreateInfo create_info{};
  create_info.queueFamilyIndex = queue_family_index;
  pool = vk::raii::CommandPool{device, create_info};

  vk::CommandBufferAllocateInfo alloc_info;
  alloc_info.commandPool = *pool;
  alloc_info.commandBufferCount = 1;
  vk::raii::CommandBuffers buffers{device, alloc_info};
  buffer = std::move(buffers[0]);
}

auto create_image_staging_buffer(vma::Allocator &alloc, vma::Image &img,
                                 std::uint32_t width, std::uint32_t height)
    -> decltype(auto) {
  std::vector<float> data;
  data.resize(width * height * 4);

  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      data[(y * width + x) * 4 + 0] = static_cast<float>(x) / width;
      data[(y * width + x) * 4 + 1] = static_cast<float>(y) / height;
      data[(y * width + x) * 4 + 2] = 0.0f;
      data[(y * width + x) * 4 + 3] = 1.0f;
    }
  }

  return alloc.create_staging_buffer_src(data.size() * sizeof(data[0]),
                                         data.data());
}

auto load_shader_module(const char *path, vk::raii::Device &device)
    -> decltype(auto) {
  std::ifstream fs{path, std::ios::binary};
  std::vector<std::uint8_t> data;
  std::copy(std::istreambuf_iterator<char>{fs},
            std::istreambuf_iterator<char>{}, std::back_inserter(data));

  vk::ShaderModuleCreateInfo info;
  info.pCode = reinterpret_cast<const std::uint32_t *>(data.data());
  info.codeSize = data.size();

  return vk::raii::ShaderModule{device, info};
}

int main() {
  vkb::InstanceBuilder b_inst;
  auto r_inst = b_inst.set_app_name("rt-weekend-vk")
                    .request_validation_layers()
                    .use_default_debug_messenger()
                    .set_headless()
                    .build();
  if (!r_inst) {
    std::cerr << "Unable to create instance: " << r_inst.error().message()
              << std::endl;
    return 1;
  }

  auto vkb_inst = r_inst.value();
  vk::raii::Context ctx;
  vk::raii::Instance inst{ctx, vkb_inst.instance};
  vk::raii::DebugUtilsMessengerEXT debug_msg{inst, vkb_inst.debug_messenger};

  vkb::PhysicalDeviceSelector pd_selector{vkb_inst};
  auto r_pd = pd_selector.set_minimum_version(1, 3).select();
  if (!r_pd) {
    std::cerr << "Unable to pick physical device: " << r_pd.error().message()
              << std::endl;
  }

  vkb::DeviceBuilder d_builder{r_pd.value()};
  auto r_device = d_builder.build();
  if (!r_device) {
    std::cerr << "Unable to create device: " << r_device.error().message()
              << std::endl;
  }

  vk::raii::PhysicalDevice phys_device{inst, r_pd.value().physical_device};
  vk::raii::Device device{phys_device, r_device.value()};

  auto r_compute_queue_idx = r_device->get_queue_index(vkb::QueueType::compute);
  if (!r_compute_queue_idx) {
    std::cerr << "Unable to get compute queue: "
              << r_compute_queue_idx.error().message() << std::endl;
  }

  auto compute_queue = device.getQueue(r_compute_queue_idx.value(), 0);

  vk::raii::CommandPool cmd_pool = nullptr;
  vk::raii::CommandBuffer cmd_buffer = nullptr;
  create_command_pool_and_buffer(device, r_compute_queue_idx.value(), cmd_pool,
                                 cmd_buffer);

  vma::Allocator allocator{*inst, *phys_device, *device};

  std::uint32_t width = 640, height = 360;
  auto image = allocator.create_image_rgba32f_2d(width, height);

  vk::ImageViewCreateInfo image_view_info;
  image_view_info.image = image.image;
  image_view_info.viewType = vk::ImageViewType::e2D;
  image_view_info.format = vk::Format::eR32G32B32A32Sfloat;
  image_view_info.subresourceRange =
      vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
  vk::raii::ImageView image_view{device, image_view_info};

  auto image_staging_buffer_src =
      create_image_staging_buffer(allocator, image, width, height);
  auto image_staging_buffer_dst =
      allocator.create_staging_buffer_dst(width * height * 4 * sizeof(float));
  auto compute_shader_module = load_shader_module("compute.spv", device);

  vk::DescriptorSetLayoutBinding desc_set_layout_binding;
  desc_set_layout_binding.binding = 0;
  desc_set_layout_binding.descriptorType = vk::DescriptorType::eStorageImage;
  desc_set_layout_binding.descriptorCount = 1;
  desc_set_layout_binding.stageFlags = vk::ShaderStageFlagBits::eCompute;

  vk::DescriptorSetLayoutCreateInfo desc_set_layout_info;
  desc_set_layout_info.bindingCount = 1;
  desc_set_layout_info.pBindings = &desc_set_layout_binding;
  vk::raii::DescriptorSetLayout desc_set_layout{device, desc_set_layout_info};

  vk::PipelineLayoutCreateInfo create_info;
  create_info.setLayoutCount = 1;
  create_info.pSetLayouts = &*desc_set_layout;

  vk::raii::PipelineLayout compute_pipeline_layout{device, create_info};

  vk::ComputePipelineCreateInfo compute_pipeline_info;
  compute_pipeline_info.stage.stage = vk::ShaderStageFlagBits::eCompute;
  compute_pipeline_info.stage.module = *compute_shader_module;
  compute_pipeline_info.stage.pName = "main";
  compute_pipeline_info.layout = compute_pipeline_layout;

  vk::raii::Pipeline compute_pipeline{device, nullptr, compute_pipeline_info};

  vk::DescriptorPoolCreateInfo desc_pool_info;
  vk::DescriptorPoolSize storage_image_pool_size{
      vk::DescriptorType::eStorageImage, 1};
  desc_pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
  desc_pool_info.maxSets = 1;
  desc_pool_info.poolSizeCount = 1;
  desc_pool_info.pPoolSizes = &storage_image_pool_size;
  vk::raii::DescriptorPool desc_pool{device, desc_pool_info};

  vk::DescriptorSetAllocateInfo desc_set_alloc_info;
  desc_set_alloc_info.descriptorPool = *desc_pool;
  desc_set_alloc_info.descriptorSetCount = 1;
  desc_set_alloc_info.pSetLayouts = &*desc_set_layout;
  vk::raii::DescriptorSets descriptor_sets{device, desc_set_alloc_info};
  vk::raii::DescriptorSet descriptor_set{std::move(descriptor_sets[0])};

  vk::DescriptorImageInfo image_write_desc_set;
  image_write_desc_set.imageView = *image_view;
  image_write_desc_set.imageLayout = vk::ImageLayout::eGeneral;

  vk::WriteDescriptorSet write_desc_set;
  write_desc_set.dstSet = *descriptor_set;
  write_desc_set.dstBinding = 0;
  write_desc_set.dstArrayElement = 0;
  write_desc_set.descriptorType = vk::DescriptorType::eStorageImage;
  write_desc_set.descriptorCount = 1;
  write_desc_set.pImageInfo = &image_write_desc_set;
  device.updateDescriptorSets(
      std::array<vk::WriteDescriptorSet, 1>{write_desc_set}, {});

  vk::CommandBufferBeginInfo begin_info;
  begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
  cmd_buffer.begin(begin_info);

  vk::BufferImageCopy copy_region;
  copy_region.setImageExtent({width, height, 1});
  copy_region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageSubresource.mipLevel = 0;

  {
    vk::ImageMemoryBarrier undef_to_transfer_src_image_barrier;
    undef_to_transfer_src_image_barrier.oldLayout = vk::ImageLayout::eUndefined;
    undef_to_transfer_src_image_barrier.newLayout =
        vk::ImageLayout::eTransferDstOptimal;
    undef_to_transfer_src_image_barrier.srcAccessMask =
        vk::AccessFlagBits::eNone;
    undef_to_transfer_src_image_barrier.dstAccessMask =
        vk::AccessFlagBits::eTransferWrite;
    undef_to_transfer_src_image_barrier.srcQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;
    undef_to_transfer_src_image_barrier.dstQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;
    undef_to_transfer_src_image_barrier.image = image.image;
    undef_to_transfer_src_image_barrier.subresourceRange =
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                               vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
                               std::array<vk::ImageMemoryBarrier, 1>{
                                   undef_to_transfer_src_image_barrier});
  }
  cmd_buffer.copyBufferToImage(image_staging_buffer_src.buffer, image.image,
                               vk::ImageLayout::eTransferDstOptimal,
                               std::array<vk::BufferImageCopy, 1>{copy_region});
  {
    vk::ImageMemoryBarrier transfer_src_to_shader_image_barrier;
    transfer_src_to_shader_image_barrier.oldLayout =
        vk::ImageLayout::eTransferDstOptimal;
    transfer_src_to_shader_image_barrier.newLayout = vk::ImageLayout::eGeneral;
    transfer_src_to_shader_image_barrier.srcAccessMask =
        vk::AccessFlagBits::eTransferWrite;
    transfer_src_to_shader_image_barrier.dstAccessMask =
        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
    transfer_src_to_shader_image_barrier.srcQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;
    transfer_src_to_shader_image_barrier.dstQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;
    transfer_src_to_shader_image_barrier.image = image.image;
    transfer_src_to_shader_image_barrier.subresourceRange =
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                               vk::PipelineStageFlagBits::eComputeShader, {},
                               {}, {},
                               std::array<vk::ImageMemoryBarrier, 1>{
                                   transfer_src_to_shader_image_barrier});
  }
  cmd_buffer.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, *compute_pipeline_layout, 0,
      std::array<vk::DescriptorSet, 1>{descriptor_set}, {});
  cmd_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, compute_pipeline);
  cmd_buffer.dispatch(width / 8, height / 8, 1);
  {
    vk::ImageMemoryBarrier shader_to_transfer_dst_image_barrier;
    shader_to_transfer_dst_image_barrier.oldLayout = vk::ImageLayout::eGeneral;
    shader_to_transfer_dst_image_barrier.newLayout =
        vk::ImageLayout::eTransferSrcOptimal;
    shader_to_transfer_dst_image_barrier.srcAccessMask =
        vk::AccessFlagBits::eShaderWrite;
    shader_to_transfer_dst_image_barrier.dstAccessMask =
        vk::AccessFlagBits::eTransferRead;
    shader_to_transfer_dst_image_barrier.srcQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;
    shader_to_transfer_dst_image_barrier.dstQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;
    shader_to_transfer_dst_image_barrier.image = image.image;
    shader_to_transfer_dst_image_barrier.subresourceRange =
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                               vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
                               std::array<vk::ImageMemoryBarrier, 1>{
                                   shader_to_transfer_dst_image_barrier});
  }
  cmd_buffer.copyImageToBuffer(image.image,
                               vk::ImageLayout::eTransferSrcOptimal,
                               image_staging_buffer_dst.buffer,
                               std::array<vk::BufferImageCopy, 1>{copy_region});

  cmd_buffer.end();

  vk::raii::Fence fence{device, vk::FenceCreateInfo{}};

  vk::SubmitInfo submit_info;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &*cmd_buffer;
  compute_queue.submit(submit_info, *fence);
  auto wait_result =
      device.waitForFences(std::array<vk::Fence, 1>{*fence}, true, UINT64_MAX);
  vma::check_vk_result(static_cast<VkResult>(wait_result),
                       "Unable to wait for fence");

  std::vector<std::uint8_t> img_data(width * height * 4);

  image_staging_buffer_dst.map([&](void *data) {
    stbi_write_hdr("output.hdr", static_cast<int>(width),
                   static_cast<int>(height), 4, static_cast<float *>(data));
  });

  return 0;
}

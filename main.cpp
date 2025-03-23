#include <VkBootstrap.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <vulkan/vulkan_structs.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include "vma.hpp"

using vec3 = std::array<float, 3>;
using vec4 = std::array<float, 4>;

constexpr std::size_t NUM_SPHERES = 128;
constexpr std::size_t NUM_MATERIALS = 128;

enum class MaterialKind : std::int32_t {
  Lambert = 0,
  Dielectric = 1,
  Metal = 2,
};

struct Material {
  vec3 color;
  MaterialKind kind;
};

struct GPURenderData {
  Material materials[NUM_MATERIALS];
  vec4 spheres[NUM_SPHERES];
  std::int32_t sphere_mat[NUM_SPHERES];
  std::int32_t num_spheres, num_materials;
};

constexpr bool USE_UNIFORM_BUFFER = sizeof(GPURenderData) < (16 << 10);

struct RenderData {
  std::uint32_t width, height;
  GPURenderData gpu;
};

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

  vma::Buffer ubo_staging_buffer_src;
  std::uint32_t width = 640, height = 360;
  {
    auto render_data = std::make_unique<RenderData>();
    try {
      std::ifstream data_file{"render.dat"};
      data_file.exceptions(std::ios::badbit | std::ifstream::failbit);
      data_file.read(reinterpret_cast<char *>(render_data.get()),
                     sizeof(*render_data));
      std::size_t num_spheres = render_data->gpu.num_spheres;
      std::cout << "Loaded " << num_spheres << " spheres." << std::endl;
    } catch (std::exception &ex) {
      std::cerr << "Unable to open render.dat, using sample data instead."
                << "(Exception: " << ex.what() << ")" << std::endl;
      render_data->width = 640;
      render_data->height = 360;
      render_data->gpu.spheres[0] = {0.0f, 0.0f, -1.0f, 0.5f};
      render_data->gpu.spheres[1] = {0.0f, -100.5f, -1.0f, 100.0f};
      render_data->gpu.sphere_mat[0] = 1;
      render_data->gpu.sphere_mat[1] = 0;
      render_data->gpu.materials[0] = {vec3{0.5, 0.5, 0.5},
                                       MaterialKind::Lambert};
      render_data->gpu.materials[1] = {vec3{0.5, 0.5, 0.5},
                                       MaterialKind::Metal};
      render_data->gpu.num_spheres = 2;
      render_data->gpu.num_materials = 2;
    }

    ubo_staging_buffer_src = allocator.create_staging_buffer_src(
        sizeof(render_data->gpu), &render_data->gpu);
    width = render_data->width;
    height = render_data->height;
  }

  auto image = allocator.create_image_rgba32f_2d(width, height);
  vma::Buffer gpu_buffer;
  if constexpr (USE_UNIFORM_BUFFER) {
    gpu_buffer = allocator.create_uniform_buffer(sizeof(GPURenderData));
  } else {
    gpu_buffer = allocator.create_storage_buffer(sizeof(GPURenderData));
  }

  vk::ImageViewCreateInfo image_view_info;
  image_view_info.image = image.image;
  image_view_info.viewType = vk::ImageViewType::e2D;
  image_view_info.format = vk::Format::eR32G32B32A32Sfloat;
  image_view_info.subresourceRange =
      vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
  vk::raii::ImageView image_view{device, image_view_info};

  auto image_staging_buffer_dst =
      allocator.create_staging_buffer_dst(width * height * 4 * sizeof(float));
  auto compute_shader_module = load_shader_module("shaders/main.spv", device);

  vk::DescriptorSetLayoutBinding bindings[2];
  bindings[0].binding = 0;
  bindings[0].descriptorType = vk::DescriptorType::eStorageImage;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = vk::ShaderStageFlagBits::eCompute;

  bindings[1].binding = 1;
  bindings[1].descriptorType = USE_UNIFORM_BUFFER
                                   ? vk::DescriptorType::eUniformBuffer
                                   : vk::DescriptorType::eStorageBuffer;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = vk::ShaderStageFlagBits::eCompute;

  vk::DescriptorSetLayoutCreateInfo desc_set_layout_info;
  desc_set_layout_info.bindingCount = 2;
  desc_set_layout_info.pBindings = bindings;

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
  std::array<vk::DescriptorPoolSize, 2> storage_image_pool_size = {
      vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 1},
      vk::DescriptorPoolSize{USE_UNIFORM_BUFFER
                                 ? vk::DescriptorType::eUniformBuffer
                                 : vk::DescriptorType::eStorageBuffer,
                             1},
  };
  desc_pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
  desc_pool_info.maxSets = 1;
  desc_pool_info.poolSizeCount = storage_image_pool_size.size();
  desc_pool_info.pPoolSizes = storage_image_pool_size.data();
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

  vk::DescriptorBufferInfo buffer_write_desc_set;
  buffer_write_desc_set.buffer = gpu_buffer.buffer;
  buffer_write_desc_set.offset = 0;
  buffer_write_desc_set.range = sizeof(GPURenderData);

  vk::WriteDescriptorSet write_desc_set[2];
  write_desc_set[0].dstSet = *descriptor_set;
  write_desc_set[0].dstBinding = 0;
  write_desc_set[0].dstArrayElement = 0;
  write_desc_set[0].descriptorType = vk::DescriptorType::eStorageImage;
  write_desc_set[0].descriptorCount = 1;
  write_desc_set[0].pImageInfo = &image_write_desc_set;
  write_desc_set[1].dstSet = *descriptor_set;
  write_desc_set[1].dstBinding = 1;
  write_desc_set[1].dstArrayElement = 0;
  write_desc_set[1].descriptorType = USE_UNIFORM_BUFFER
                                         ? vk::DescriptorType::eUniformBuffer
                                         : vk::DescriptorType::eStorageBuffer;
  write_desc_set[1].descriptorCount = 1;
  write_desc_set[1].pBufferInfo = &buffer_write_desc_set;
  device.updateDescriptorSets(write_desc_set, {});

  vk::CommandBufferBeginInfo begin_info;
  begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
  cmd_buffer.begin(begin_info);

  vk::BufferImageCopy copy_region;
  copy_region.setImageExtent({width, height, 1});
  copy_region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageSubresource.mipLevel = 0;

  vk::BufferCopy buf_copy_region;
  buf_copy_region.size = sizeof(GPURenderData);

  cmd_buffer.copyBuffer(ubo_staging_buffer_src.buffer, gpu_buffer.buffer,
                        std::array<vk::BufferCopy, 1>{buf_copy_region});
  {
    vk::ImageMemoryBarrier undef_to_shader_image_barrier;
    undef_to_shader_image_barrier.oldLayout = vk::ImageLayout::eUndefined;
    undef_to_shader_image_barrier.newLayout = vk::ImageLayout::eGeneral;
    undef_to_shader_image_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    undef_to_shader_image_barrier.dstAccessMask =
        vk::AccessFlagBits::eShaderWrite;
    undef_to_shader_image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    undef_to_shader_image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    undef_to_shader_image_barrier.image = image.image;
    undef_to_shader_image_barrier.subresourceRange =
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    vk::BufferMemoryBarrier ubo_barrier;
    ubo_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    ubo_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    ubo_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ubo_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ubo_barrier.buffer = gpu_buffer.buffer;
    ubo_barrier.offset = 0;
    ubo_barrier.size = sizeof(GPURenderData);

    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                               vk::PipelineStageFlagBits::eComputeShader, {},
                               {}, ubo_barrier, undef_to_shader_image_barrier);
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

  auto start = std::chrono::high_resolution_clock::now();
  vk::SubmitInfo submit_info;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &*cmd_buffer;
  compute_queue.submit(submit_info, *fence);
  auto wait_result =
      device.waitForFences(std::array<vk::Fence, 1>{*fence}, true, UINT64_MAX);
  vma::check_vk_result(static_cast<VkResult>(wait_result),
                       "Unable to wait for fence");
  auto end = std::chrono::high_resolution_clock::now();

  auto elapsed = std::chrono::duration<double>(end - start);
  std::cout << "Done in " << elapsed.count() << "s" << std::endl;
  std::vector<std::uint8_t> img_data(width * height * 4);

  image_staging_buffer_dst.map([&](void *data) {
    std::span<float> flt_pixels(static_cast<float *>(data), width * height * 4);
    for (int x = 0; x < width; ++x) {
      for (int y = 0; y < height; ++y) {
        for (int c = 0; c < 4; ++c) {
          float value = flt_pixels[c + (y + x * height) * 4];
          if (value < 0.0 || value > 1.0 || std::isnan(value)) {
            std::cerr << "Invalid pixel value: " << value << " (x=" << x
                      << ", y=" << y << ", c=" << c << ")" << std::endl;
          }
        }
      }
    }
#ifdef OUTPUT_PNG
    std::vector<unsigned char> pixels(width * height * 4);
    std::transform(flt_pixels.begin(), flt_pixels.end(), pixels.begin(),
                   [](float val) {
                     val = std::clamp(val, 0.0f, 1.0f);
                     return static_cast<unsigned char>(val * 255.0f);
                   });
    stbi_write_png("output.png", width, height, 4, pixels.data(), 0);
#else

    stbi_write_hdr("output.hdr", static_cast<int>(width),
                   static_cast<int>(height), 4, static_cast<float *>(data));
#endif
  });

  return 0;
}

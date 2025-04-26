cmake_minimum_required(VERSION 3.31 FATAL_ERROR)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")
set(CMAKE_CXX_MODULE_STD 1)

option(OUTPUT_PNG "Output a PNG image instead of a HDR file" OFF)

project(rt-weekend-vk LANGUAGES CXX)

find_package(Vulkan REQUIRED)
if( ${Vulkan_VERSION} VERSION_LESS "1.3.256" )
  message( FATAL_ERROR "Minimum required Vulkan version for C++ modules is 1.3.256. "
           "Found ${Vulkan_VERSION}."
  )
endif()

add_library(VulkanHpp)
target_sources(VulkanHpp PUBLIC
  FILE_SET CXX_MODULES
  BASE_DIRS ${Vulkan_INCLUDE_DIR}
  FILES ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm
)
target_compile_features(VulkanHpp PUBLIC cxx_std_23)
target_link_libraries(VulkanHpp PUBLIC Vulkan::Vulkan)

find_package(vk-bootstrap REQUIRED)

add_library(vma_wrapper)
target_sources(vma_wrapper
  PUBLIC FILE_SET CXX_MODULES
  FILES ${CMAKE_CURRENT_LIST_DIR}/vma.cppm
)
target_compile_features(vma_wrapper PUBLIC cxx_std_23)
target_link_libraries(vma_wrapper PUBLIC VulkanHpp)

add_library(vkb_wrapper)
target_sources(vkb_wrapper
  PUBLIC FILE_SET CXX_MODULES
  BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}
  FILES ${CMAKE_CURRENT_LIST_DIR}/vkb.cppm
)
target_compile_features(vkb_wrapper PUBLIC cxx_std_23)
target_link_libraries(vkb_wrapper PUBLIC VulkanHpp vk-bootstrap::vk-bootstrap)

add_library(stbi_wrapper)
target_sources(stbi_wrapper
  PUBLIC FILE_SET CXX_MODULES
  BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}
  FILES ${CMAKE_CURRENT_LIST_DIR}/stbi.cppm
)

target_compile_features(stbi_wrapper PUBLIC cxx_std_23)

add_executable(rt_weekend_vk main.cpp)
target_compile_features(rt_weekend_vk PRIVATE cxx_std_23)
target_link_libraries(rt_weekend_vk PRIVATE VulkanHpp vma_wrapper vkb_wrapper stbi_wrapper)

if(OUTPUT_PNG)
  target_compile_definitions(rt_weekend_vk PRIVATE OUTPUT_PNG)
endif()

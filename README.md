# rt-weekend-vk

Implementation of [Ray tracing in one weekend](https://raytracing.github.io/)
using Vulkan compute shaders.

Dependencies: [vulkan.hpp](https://github.com/KhronosGroup/Vulkan-Hpp),
[vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap),
[stb_image_write.h](https://github.com/nothings/stb).

Build & run scripts in `scripts/` is only included for convenience, it requires
[g++](https://gcc.gnu.org/) (C++ compiler),
[glslc](https://github.com/google/shaderc) (GLSL to SPIR-V compiler) and
[viu](https://github.com/atanunq/viu) (command-line image viewer), but other
tools work fine. Support for other tools are NOT provided, user with different
setups must tweak the scripts or run them manually.

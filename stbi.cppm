module;

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

export module stbi;

export namespace stbi {
int write_png(const char *filename, int width, int height, int comp,
              const unsigned char *pixels, int stride) {
  return stbi_write_png(filename, width, height, comp, pixels, stride);
}

int write_hdr(const char *filename, int width, int height, int comp,
              float *pixels) {
  return stbi_write_hdr(filename, width, height, comp, pixels);
}
} // namespace stbi

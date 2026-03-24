#include "../macros.h"
#include "GL/gl.h"
#include "module.h"
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct png_module_t {
  FILE *f;
  unsigned char *fbuf;
  size_t fsize;
  GLuint textureID;
  int width, height;
  png_bytep *row_pointers;

  float widthDiff;
  float heightDiff;
} pngm;

static void InitTexture(GLsizei width, GLsizei height, uint8_t *data) {}

static void UpdateTexture(GLsizei width, GLsizei height, uint8_t *data) {
  GL_COMMAND(glBindTexture(GL_TEXTURE_2D, pngm.textureID));
  GL_COMMAND(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGB,
                          GL_UNSIGNED_BYTE, data));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
}

static void module_setup(char *filename) {
  FILE *fp = fopen(filename, "rb");
  png_byte bit_depth;
  png_byte color_type;
  unsigned int y;

  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png)
    abort();
  png_infop info = png_create_info_struct(png);
  if (!info)
    abort();
  if (setjmp(png_jmpbuf(png)))
    abort();
  png_init_io(png, fp);
  png_read_info(png, info);
  pngm.width = png_get_image_width(png, info);
  pngm.height = png_get_image_height(png, info);
  color_type = png_get_color_type(png, info);
  bit_depth = png_get_bit_depth(png, info);
  /* Read any color_type into 8bit depth, RGBA format. */
  /* See http://www.libpng.org/pub/png/libpng-manual.txt */
  if (bit_depth == 16)
    png_set_strip_16(png);
  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);
  /* PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth. */
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png);
  if (png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);
  /* These color_type don't have an alpha channel then fill it with 0xff. */
  if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);
  png_read_update_info(png, info);
  pngm.row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * pngm.height);
  for (y = 0; y < pngm.height; y++) {
    pngm.row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png, info));
  }
  png_read_image(png, pngm.row_pointers);
  fclose(fp);

  printf("%d %d\n", pngm.width, pngm.height);

  pngm.fbuf = malloc(pngm.width * pngm.height * 4);

  for (unsigned int y = 0; y < pngm.height; y++) {
    png_bytep row = pngm.row_pointers[y];
    for (unsigned int x = 0; x < pngm.width; x++) {
      pngm.fbuf[(y * pngm.width) + x] = 0;
      pngm.fbuf[((y * pngm.width) + x) + 1] = 255;
      pngm.fbuf[((y * pngm.width) + x) + 2] = 0;
      pngm.fbuf[((y * pngm.width) + x) + 3] = 255;
      printf("h\n");
      // png_bytep px = &(row[x * 4]);
      // printf("%4d, %4d = RGBA(%3d, %3d, %3d, %3d)\n", x, y, px[0], px[1],
      // px[2],
      //        px[3]);
      // png_byte old[4 * sizeof(png_byte)];
      // memcpy(old, px, sizeof(old));
      // px[0] = 255 - old[0];
      // px[1] = 255 - old[1];
      // px[2] = 255 - old[2];
    }
  }

  GL_COMMAND(glGenTextures(1, &pngm.textureID));
  GL_COMMAND(glBindTexture(GL_TEXTURE_2D, pngm.textureID));
  GL_COMMAND(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pngm.width, pngm.height, 0,
                          GL_RGB, GL_UNSIGNED_BYTE, pngm.row_pointers));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

  GL_COMMAND(glEnable(GL_TEXTURE_2D));

  pngm.widthDiff = 1.0;
  pngm.heightDiff = 1.0;
}

static void module_step() {
  GL_COMMAND(glClearColor(1.0f, 0.0f, 1.0f, 1.0f));
  GL_COMMAND(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
  GL_COMMAND(glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL));
  GL_COMMAND(glTranslatef(0, 0, 0));

  GL_COMMAND(glBindTexture(GL_TEXTURE_2D, pngm.textureID));

  glBegin(GL_QUADS);

  glTexCoord2f(0.0, pngm.heightDiff);
  glVertex2f(-1.0, -1.0);
  glTexCoord2f(pngm.widthDiff, pngm.heightDiff);
  glVertex2f(1.0, -1.0);
  glTexCoord2f(pngm.widthDiff, 0.0);
  glVertex2f(1.0, 1.0);
  glTexCoord2f(0.0, 0.0);
  glVertex2f(-1.0, 1.0);
  glEnd();
}

pp_module pp_png_module(char *buf) {
  return (pp_module){
      .setup = module_setup,
      .step = module_step,
  };
}

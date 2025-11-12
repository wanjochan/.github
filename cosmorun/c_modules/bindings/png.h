/* libpng FFI Bindings for CosmoRun
 * Provides PNG image encoding and decoding functionality
 *
 * Usage: __import("bindings/png")
 */

#ifndef COSMORUN_BINDINGS_PNG_H
#define COSMORUN_BINDINGS_PNG_H

/* TCC compatibility: include types before standard headers */
#ifdef __TINYC__
#include "tcc_stdint.h"
#endif

#include <stddef.h>
#ifndef __TINYC__
#include <stdint.h>
#endif
#include <stdio.h>
#include <setjmp.h>

/* PNG version */
#define PNG_LIBPNG_VER_STRING "1.6.37"

/* PNG color types */
#define PNG_COLOR_TYPE_GRAY         0
#define PNG_COLOR_TYPE_PALETTE      3
#define PNG_COLOR_TYPE_RGB          2
#define PNG_COLOR_TYPE_RGB_ALPHA    6
#define PNG_COLOR_TYPE_GRAY_ALPHA   4

/* PNG interlace types */
#define PNG_INTERLACE_NONE  0
#define PNG_INTERLACE_ADAM7 1

/* PNG compression types */
#define PNG_COMPRESSION_TYPE_DEFAULT 0

/* PNG filter types */
#define PNG_FILTER_TYPE_DEFAULT 0

/* PNG transforms */
#define PNG_TRANSFORM_IDENTITY      0x0000
#define PNG_TRANSFORM_STRIP_16      0x0001
#define PNG_TRANSFORM_STRIP_ALPHA   0x0002
#define PNG_TRANSFORM_PACKING       0x0004
#define PNG_TRANSFORM_PACKSWAP      0x0008
#define PNG_TRANSFORM_EXPAND        0x0010
#define PNG_TRANSFORM_INVERT_MONO   0x0020
#define PNG_TRANSFORM_SHIFT         0x0040
#define PNG_TRANSFORM_BGR           0x0080
#define PNG_TRANSFORM_SWAP_ALPHA    0x0100
#define PNG_TRANSFORM_SWAP_ENDIAN   0x0200
#define PNG_TRANSFORM_INVERT_ALPHA  0x0400
#define PNG_TRANSFORM_STRIP_FILLER  0x0800
#define PNG_TRANSFORM_GRAY_TO_RGB   0x2000
#define PNG_TRANSFORM_EXPAND_16     0x4000

/* PNG types */
typedef struct png_struct_def png_struct;
typedef png_struct* png_structp;
typedef struct png_info_def png_info;
typedef png_info* png_infop;
typedef unsigned char* png_bytep;
typedef unsigned char** png_bytepp;
typedef uint32_t png_uint_32;
typedef int32_t png_int_32;

/* PNG callback types */
typedef void (*png_error_ptr)(png_structp, const char*);
typedef void (*png_rw_ptr)(png_structp, png_bytep, size_t);
typedef void (*png_flush_ptr)(png_structp);

/* Core PNG functions */

/* Create PNG read/write structs */
png_structp png_create_read_struct(const char* user_png_ver, void* error_ptr, png_error_ptr error_fn, png_error_ptr warn_fn);
png_structp png_create_write_struct(const char* user_png_ver, void* error_ptr, png_error_ptr error_fn, png_error_ptr warn_fn);

png_infop png_create_info_struct(png_structp png_ptr);

void png_destroy_read_struct(png_structp* png_ptr_ptr, png_infop* info_ptr_ptr, png_infop* end_info_ptr_ptr);
void png_destroy_write_struct(png_structp* png_ptr_ptr, png_infop* info_ptr_ptr);

/* I/O functions */
void png_init_io(png_structp png_ptr, FILE* fp);
void png_set_read_fn(png_structp png_ptr, void* io_ptr, png_rw_ptr read_data_fn);
void png_set_write_fn(png_structp png_ptr, void* io_ptr, png_rw_ptr write_data_fn, png_flush_ptr output_flush_fn);

/* Read functions */
void png_read_png(png_structp png_ptr, png_infop info_ptr, int transforms, void* params);
void png_read_info(png_structp png_ptr, png_infop info_ptr);
void png_read_image(png_structp png_ptr, png_bytepp image);
void png_read_end(png_structp png_ptr, png_infop info_ptr);

/* Write functions */
void png_write_png(png_structp png_ptr, png_infop info_ptr, int transforms, void* params);
void png_write_info(png_structp png_ptr, png_infop info_ptr);
void png_write_image(png_structp png_ptr, png_bytepp image);
void png_write_end(png_structp png_ptr, png_infop info_ptr);

/* Set PNG image info */
void png_set_IHDR(png_structp png_ptr, png_infop info_ptr, png_uint_32 width, png_uint_32 height,
                  int bit_depth, int color_type, int interlace_method, int compression_method, int filter_method);

/* Get PNG image info */
png_uint_32 png_get_IHDR(png_structp png_ptr, png_infop info_ptr, png_uint_32* width, png_uint_32* height,
                         int* bit_depth, int* color_type, int* interlace_method, int* compression_method, int* filter_method);

png_uint_32 png_get_image_width(png_structp png_ptr, png_infop info_ptr);
png_uint_32 png_get_image_height(png_structp png_ptr, png_infop info_ptr);
uint8_t png_get_bit_depth(png_structp png_ptr, png_infop info_ptr);
uint8_t png_get_color_type(png_structp png_ptr, png_infop info_ptr);
uint8_t png_get_channels(png_structp png_ptr, png_infop info_ptr);
png_uint_32 png_get_rowbytes(png_structp png_ptr, png_infop info_ptr);

/* Row pointers */
png_bytepp png_get_rows(png_structp png_ptr, png_infop info_ptr);
void png_set_rows(png_structp png_ptr, png_infop info_ptr, png_bytepp row_pointers);

/* Error handling */
jmp_buf* png_set_longjmp_fn(png_structp png_ptr, void* longjmp_fn, size_t jmp_buf_size);

/* Helper macro for jmpbuf access */
#define png_jmpbuf(png_ptr) (*png_set_longjmp_fn(png_ptr, longjmp, sizeof(jmp_buf)))

/* Transforms */
void png_set_expand(png_structp png_ptr);
void png_set_strip_16(png_structp png_ptr);
void png_set_strip_alpha(png_structp png_ptr);
void png_set_packing(png_structp png_ptr);
void png_set_bgr(png_structp png_ptr);
void png_set_gray_to_rgb(png_structp png_ptr);
void png_set_rgb_to_gray(png_structp png_ptr, int error_action, double red_weight, double green_weight);

/* Simplified high-level API */

/* Image structure for convenience */
typedef struct {
    png_uint_32 width;
    png_uint_32 height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t channels;
    png_bytepp row_pointers;
    unsigned char* data;  /* Contiguous pixel data */
} png_image_t;

/* Load PNG from file */
static inline png_image_t* png_load_file(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return NULL;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL;
    }

    png_init_io(png, fp);
    png_read_png(png, info, PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_STRIP_16, NULL);

    png_image_t* img = (png_image_t*)malloc(sizeof(png_image_t));
    if (!img) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL;
    }

    img->width = png_get_image_width(png, info);
    img->height = png_get_image_height(png, info);
    img->bit_depth = png_get_bit_depth(png, info);
    img->color_type = png_get_color_type(png, info);
    img->channels = png_get_channels(png, info);
    img->row_pointers = png_get_rows(png, info);
    img->data = NULL;

    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return img;
}

/* Save PNG to file */
static inline int png_save_file(const char* filename, png_image_t* img) {
    if (!img) return 0;

    FILE* fp = fopen(filename, "wb");
    if (!fp) return 0;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return 0;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        return 0;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return 0;
    }

    png_init_io(png, fp);

    png_set_IHDR(png, info, img->width, img->height, img->bit_depth, img->color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_set_rows(png, info, img->row_pointers);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);

    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return 1;
}

/* Create new image */
static inline png_image_t* png_image_new(png_uint_32 width, png_uint_32 height, uint8_t color_type) {
    png_image_t* img = (png_image_t*)malloc(sizeof(png_image_t));
    if (!img) return NULL;

    img->width = width;
    img->height = height;
    img->bit_depth = 8;
    img->color_type = color_type;

    switch (color_type) {
        case PNG_COLOR_TYPE_GRAY: img->channels = 1; break;
        case PNG_COLOR_TYPE_GRAY_ALPHA: img->channels = 2; break;
        case PNG_COLOR_TYPE_RGB: img->channels = 3; break;
        case PNG_COLOR_TYPE_RGB_ALPHA: img->channels = 4; break;
        default: img->channels = 3; break;
    }

    size_t rowbytes = width * img->channels;
    img->row_pointers = (png_bytepp)malloc(sizeof(png_bytep) * height);
    if (!img->row_pointers) {
        free(img);
        return NULL;
    }

    img->data = (unsigned char*)malloc(rowbytes * height);
    if (!img->data) {
        free(img->row_pointers);
        free(img);
        return NULL;
    }

    for (png_uint_32 y = 0; y < height; y++) {
        img->row_pointers[y] = img->data + y * rowbytes;
    }

    return img;
}

/* Free image */
static inline void png_image_free(png_image_t* img) {
    if (!img) return;
    if (img->data) free(img->data);
    if (img->row_pointers) free(img->row_pointers);
    free(img);
}

/* Get pixel (RGB) */
static inline void png_get_pixel_rgb(png_image_t* img, png_uint_32 x, png_uint_32 y, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!img || x >= img->width || y >= img->height) return;
    unsigned char* pixel = img->row_pointers[y] + x * img->channels;
    *r = pixel[0];
    *g = (img->channels > 1) ? pixel[1] : pixel[0];
    *b = (img->channels > 2) ? pixel[2] : pixel[0];
}

/* Set pixel (RGB) */
static inline void png_set_pixel_rgb(png_image_t* img, png_uint_32 x, png_uint_32 y, uint8_t r, uint8_t g, uint8_t b) {
    if (!img || x >= img->width || y >= img->height) return;
    unsigned char* pixel = img->row_pointers[y] + x * img->channels;
    pixel[0] = r;
    if (img->channels > 1) pixel[1] = g;
    if (img->channels > 2) pixel[2] = b;
    if (img->channels > 3) pixel[3] = 255; /* Full alpha */
}

#endif /* COSMORUN_BINDINGS_PNG_H */

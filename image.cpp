// PNG image loader
#pragma once

#include <vector>
#include <iostream>
#include <cerrno>

#include "png.h"


uint8_t* read_png_file(const char* file_name, int* width, int* height, int* channels, int out_channels) {
    if (out_channels != 4) {
        std::cout << "Output channels must be 4 for RGBA format.\n";
    }

    FILE* fp;
    errno_t err;
    err = fopen_s(&fp, file_name, "rb");
    if (err != 0) {
        std::cout << "File could not be opened for reading.\n";
        std::cout << "File could not be opened for reading.\n";
        return nullptr;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        std::cout << "png_create_read_struct failed.\n";
        fclose(fp);
        return nullptr;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        std::cout << "png_create_info_struct failed.\n";
        fclose(fp);
        png_destroy_read_struct(&png, NULL, NULL);
        return nullptr;
    }

    if (setjmp(png_jmpbuf(png))) {
        std::cout << "Error during init_io.\n";
        fclose(fp);
        png_destroy_read_struct(&png, &info, NULL);
        return nullptr;
    }

    png_init_io(png, fp);

    png_read_info(png, info);

    *width = png_get_image_width(png, info);
    *height = png_get_image_height(png, info);
    *channels = png_get_channels(png, info);

    png_set_expand(png);
    if (*channels < out_channels) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }

    png_read_update_info(png, info);

    size_t rowbytes = png_get_rowbytes(png, info);
    uint8_t* image_data = new uint8_t[rowbytes * *height];

    std::vector<png_bytep> row_pointers(*height);
    for (int y = 0; y < *height; y++) {
        row_pointers[y] = &image_data[y * rowbytes];
    }

    png_read_image(png, row_pointers.data());

    fclose(fp);
    png_destroy_read_struct(&png, &info, NULL);

    return image_data;
}


/*std::vector<uint8_t> read_png_file(const char* file_name, int* width, int* height, int* channels, int out_channels) {
    if (out_channels != 4) {
        throw std::invalid_argument("Output channels must be 4 for RGBA format.");
    }

    FILE* fp = fopen(file_name, "rb");
    if (!fp) {
        throw std::runtime_error("File could not be opened for reading.");
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        throw std::runtime_error("png_create_read_struct failed.");
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        fclose(fp);
        png_destroy_read_struct(&png, NULL, NULL);
        throw std::runtime_error("png_create_info_struct failed.");
    }

    if (setjmp(png_jmpbuf(png))) {
        fclose(fp);
        png_destroy_read_struct(&png, &info, NULL);
        throw std::runtime_error("Error during init_io.");
    }

    png_init_io(png, fp);

    png_read_info(png, info);

    *width = png_get_image_width(png, info);
    *height = png_get_image_height(png, info);
    *channels = png_get_channels(png, info);

    png_set_expand(png);
    if (*channels < out_channels) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }

    png_read_update_info(png, info);

    size_t rowbytes = png_get_rowbytes(png, info);
    std::vector<uint8_t> image_data(rowbytes * *height);

    std::vector<png_bytep> row_pointers(*height);
    for (int y = 0; y < *height; y++) {
        row_pointers[y] = &image_data[y * rowbytes];
    }

    png_read_image(png, row_pointers.data());

    fclose(fp);
    png_destroy_read_struct(&png, &info, NULL);

    return image_data;
}*/


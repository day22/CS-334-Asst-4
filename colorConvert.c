/* color conversion of images from color to grayscale
* Author: Martin Cenek
* University of Portland
* Date: 3/2/2022
* dependencies: libpng16.a libz.a
* target architecture: Debian linux kernel
* To compile: gcc -o colorConvert colorConvert.c libpng16.a libz.a -lm 
* To execute: ./colorCovert <input file> <output file>
* Pre-condition: 8 bit png input file
* Post-condition: 8 bit png output file in grayscale
*/

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include "png.h"
 
void abort_(const char * s, ...){
        va_list args;
        va_start(args, s);
        vfprintf(stderr, s, args);
        fprintf(stderr, "\n");
        va_end(args);
        abort();
}

int colorConvert(int argv, char* argc[]){
    if (argv != 3){
        abort_("usage: <executable> <input file> <output file>");
        exit(EXIT_FAILURE);
    }
    char* fn_in = argc[1];
    char* fn_out= argc[2];
    unsigned char* img;
    int avg;                  //agerage rgb pixel value

    png_structp png_ptr_rd;   // pointer to png read struct
    png_infop info_ptr_rd;    // poiner to png read header struct
    png_structp png_ptr_wr;   // pointer to png write struct
    png_infop info_ptr_wr;    // poiner to png write header struct
    png_uint_32 width, height, bit_depth, color_type, interlace_type, number_of_passes;
    png_bytep * row_pointers; // pointer to image payload
    char header[8];           // to read magic number of 8 bytes

    // open file and test for it being a png
    FILE *fp = fopen(fn_in, "rb");
    if (!fp)
        abort_("[fopen] fopen");
    fread(header, 1, 8, fp); // read magic number
    if (png_sig_cmp(header, 0, 8))
        abort_("[png_sig_comp] not a PNG file");

    // initialize png read structs
    if((png_ptr_rd = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))==0)
        abort_("[png_create_read_struct] failed");

    if((info_ptr_rd = png_create_info_struct(png_ptr_rd))==0)
        abort_("[png_create_info_struct] failed");

    if (setjmp(png_jmpbuf(png_ptr_rd)))
        abort_("[init_io] failed");

    png_init_io(png_ptr_rd, fp);
    png_set_sig_bytes(png_ptr_rd, 8);
    //load structs
    png_read_info(png_ptr_rd, info_ptr_rd);
    //get local variable copies from structs
    width = png_get_image_width(png_ptr_rd, info_ptr_rd);
    height = png_get_image_height(png_ptr_rd, info_ptr_rd);
    color_type = png_get_color_type(png_ptr_rd, info_ptr_rd);
    bit_depth = png_get_bit_depth(png_ptr_rd, info_ptr_rd);

    //for inflating
    number_of_passes = png_set_interlace_handling(png_ptr_rd);
    png_read_update_info(png_ptr_rd, info_ptr_rd);

    // read file
    if (setjmp(png_jmpbuf(png_ptr_rd)))
        abort_("png_jmpbuf: error read_image");
    // allocated array of row pointers
    row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
    // allocated each row to read data into
    for (int y=0; y<height; y++)
            row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr_rd,info_ptr_rd));
    // read image into the 2D array
    png_read_image(png_ptr_rd, row_pointers);
    // done reading so close file
    fclose(fp);
    //check the file format is RBG to access it as [][][] with 0-255 values
    if (png_get_color_type(png_ptr_rd, info_ptr_rd) != PNG_COLOR_TYPE_RGB)
        abort_("must be a RGB file");
    //finally convert the image's bits to grayscale
    for (int y=0; y<height; y++) {
        png_byte* row = row_pointers[y];
        for (int x=0; x<width; x++) {
            png_byte* ptr = &(row[x*3]);
            avg = ((int) ptr[0] + (int) ptr[1] + (int) ptr[2])/3;
            ptr[0] = avg;
            ptr[1] = avg;
            ptr[2] = avg;
        }
    }
    /////////////////////////////////////////////////////
    // start of convert and write image file out section
    // convert the image to grayscale and write it out as a new file.
    /////////////////////////////////////////////////////

    FILE *fp_out = fopen(fn_out, "wb");
    if (!fp_out)
            abort_("[write_png_file] File %s could not be opened for writing", fn_out);


    // initialize and check write structs
    png_ptr_wr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info_ptr_wr = png_create_info_struct(png_ptr_wr);

    if (!png_ptr_wr)
            abort_("[write_png_file] png_create_write_struct failed");

    if (!info_ptr_wr)
            abort_("[write_png_file] png_create_info_struct failed");

    if (setjmp(png_jmpbuf(png_ptr_wr)))
            abort_("[write_png_file] Error during init_io");

    png_init_io(png_ptr_wr, fp_out);


    // write header
    if (setjmp(png_jmpbuf(png_ptr_wr)))
            abort_("[write_png_file] Error during writing header");

    png_set_IHDR(png_ptr_wr, info_ptr_wr, width, height,
                 bit_depth, color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr_wr, info_ptr_wr);


    // write bytes
    if (setjmp(png_jmpbuf(png_ptr_wr)))
            abort_("[write_png_file] Error during writing bytes");

     png_write_image(png_ptr_wr, row_pointers);


    // end write
    if (setjmp(png_jmpbuf(png_ptr_wr)))
            abort_("[write_png_file] Error during end of write");

    png_write_end(png_ptr_wr, NULL);
    //write memory clean up
    png_destroy_write_struct(&png_ptr_wr, &info_ptr_wr);
    fclose(fp_out);
    /////////////////////////////////////////////////////
    // end of convert and write image file out section
    ////////////////////////////////////////////////////

    //read memory clean up
    png_destroy_read_struct(&png_ptr_rd, &info_ptr_rd, NULL);

    //memory cleanup
    for (int y=0; y<height; y++)
        free(row_pointers[y]);
    free(row_pointers);

    return 0;
}
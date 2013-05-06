#ifndef _H_ANDROID_JPEG_ENCODER
#define _H_ANDROID_JPEG_ENCODER

#include "spice-common.h"
#include <jpeglib.h>
#include <spice/types.h>

typedef struct JpegEncoder {
    int (*more_space)(int size, uint8_t **io_ptr);
    void (*convert_line_to_RGB24) (uint8_t *line, int width, uint8_t **out_line);

    struct jpeg_destination_mgr dest_mgr;
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    struct {
	int width;
	int height;
	int stride;
	unsigned int out_size;
    } cur_image;
} JpegEncoder;

JpegEncoder* jpeg_encoder_create();
void jpeg_encoder_destroy(JpegEncoder *encoder);

/* returns the total size of the encoded data. Images must be supplied from the the 
   top line to the bottom */
int jpeg_encode(JpegEncoder *jpeg, int quality, int width, int height, uint8_t *lines, int stride, uint8_t** io_ptr);
void convert_BGRX32_to_RGB24(uint8_t *line, int width, uint8_t **out_line);

#endif

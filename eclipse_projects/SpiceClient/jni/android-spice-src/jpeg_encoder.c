#include "jpeg_encoder.h"

static int jpeg_usr_more_space(int size,uint8_t **io_ptr)
{
    /* create our in-memory output buffer to hold the jpeg */
    *io_ptr = (uint8_t*)spice_malloc(size);
    return size;
}

/* jpeg destination manager callbacks */

static void dest_mgr_init_destination(j_compress_ptr cinfo) 
{
    JpegEncoder *enc = (JpegEncoder *)cinfo->client_data;
    if (enc->dest_mgr.free_in_buffer == 0) {
	enc->dest_mgr.free_in_buffer = enc->more_space(3*enc->cur_image.width*enc->cur_image.height,
		&enc->dest_mgr.next_output_byte);

	if (enc->dest_mgr.free_in_buffer == 0) {
	    SPICE_DEBUG("not enough space");
	}
    }

    enc->cur_image.out_size = enc->dest_mgr.free_in_buffer;
}

static boolean dest_mgr_empty_output_buffer(j_compress_ptr cinfo)
{
    //hope this never happen
    JpegEncoder *enc = (JpegEncoder *)cinfo->client_data;
    enc->dest_mgr.free_in_buffer = enc->more_space(3*enc->cur_image.width*enc->cur_image.height,
	    &enc->dest_mgr.next_output_byte);

    if (enc->dest_mgr.free_in_buffer == 0) {
	SPICE_DEBUG("not enough space");
    }    
    enc->cur_image.out_size += enc->dest_mgr.free_in_buffer;
    return TRUE;
}

static void dest_mgr_term_destination(j_compress_ptr cinfo)
{
    JpegEncoder *enc = (JpegEncoder *)cinfo->client_data;
    enc->cur_image.out_size -= enc->dest_mgr.free_in_buffer;
}

JpegEncoder* jpeg_encoder_create()
{
    JpegEncoder *enc;

    enc = spice_new0(JpegEncoder, 1);

    enc->dest_mgr.init_destination = dest_mgr_init_destination;
    enc->dest_mgr.empty_output_buffer = dest_mgr_empty_output_buffer;
    enc->dest_mgr.term_destination = dest_mgr_term_destination;

    enc->convert_line_to_RGB24 = convert_BGRX32_to_RGB24;
    enc->more_space = jpeg_usr_more_space;

    enc->cinfo.err = jpeg_std_error(&enc->jerr);

    jpeg_create_compress(&enc->cinfo);
    enc->cinfo.client_data = enc;
    enc->cinfo.dest = &enc->dest_mgr;
    return enc;
}

void jpeg_encoder_destroy(JpegEncoder* encoder)
{    
    jpeg_destroy_compress(&(encoder->cinfo));
    free(encoder);
}

void convert_BGRX32_to_RGB24(uint8_t *line, int width, uint8_t **out_line)
{
    uint32_t *src_line = (uint32_t *)line;
    uint8_t *out_pix;
    int x;

    if(!(out_line && *out_line))
    {
	SPICE_DEBUG("cannot convert_BGRX32_to_RGB24!");
	return;
    }

    out_pix = *out_line;

    for (x = 0; x < width; x++) {
	uint32_t pixel = *src_line++;
	*out_pix++ = (pixel >> 16) & 0xff;
	*out_pix++ = (pixel >> 8) & 0xff;
	*out_pix++ = pixel & 0xff;
    }
}


static void do_jpeg_encode(JpegEncoder *jpeg, uint8_t *lines)
{    
    uint8_t *RGB24_line;
    int stride, width, height;
    JSAMPROW row_pointer[1];
    width = jpeg->cur_image.width;
    height = jpeg->cur_image.height;
    stride = jpeg->cur_image.stride;

    RGB24_line = (uint8_t *)spice_malloc(width*3);


    for (;jpeg->cinfo.next_scanline < jpeg->cinfo.image_height; lines += stride) {
	//lines+=stride to move to next line??
	jpeg->convert_line_to_RGB24(lines, width, &RGB24_line);
	row_pointer[0] = RGB24_line;
	jpeg_write_scanlines(&jpeg->cinfo, row_pointer, 1);
    }
    //can I free you?
    free(RGB24_line);
}

int jpeg_encode(JpegEncoder *jpeg, int quality, int width, int height,
	uint8_t *lines, int stride, uint8_t** io_ptr)
{
    int bufsize = 3*width*height;
    *io_ptr = (uint8_t*)spice_malloc(bufsize);
    JpegEncoder *enc = (JpegEncoder *)jpeg; 

    enc->cur_image.width = width;
    enc->cur_image.height = height;
    enc->cur_image.stride = stride;
    enc->cur_image.out_size = 0;


    enc->cinfo.image_width = width;
    enc->cinfo.image_height = height;
    enc->cinfo.input_components = 3;
    enc->cinfo.in_color_space = JCS_RGB;
    enc->cinfo.dct_method = JDCT_IFAST;
    jpeg_set_defaults(&enc->cinfo);
    jpeg_set_quality(&enc->cinfo, quality, TRUE);

    enc->dest_mgr.next_output_byte = *io_ptr;
    enc->dest_mgr.free_in_buffer = bufsize;

    jpeg_start_compress(&enc->cinfo, TRUE);

    do_jpeg_encode(enc, lines);

    jpeg_finish_compress(&enc->cinfo);

    //#define JPEG_DUMP
#ifdef JPEG_DUMP
    FILE* outfile = fopen("/data/local/tmp/ahoo.jpeg", "wb");
    fwrite(*io_ptr, enc->cur_image.out_size,1 ,outfile);
    SPICE_DEBUG("--------ahoooooooooo:jpeg with size :%d produced!",enc->cur_image.out_size);
    fclose(outfile);
#endif

    return enc->cur_image.out_size;
}


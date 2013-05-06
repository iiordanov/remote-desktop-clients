/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <glib/gi18n.h>
//fuck
#include <jni.h>

#include "spice-client.h"
#include "spice-common.h"
#include "spice-cmdline.h"

#include <jpeglib.h>
/* config */
static char *outf      = "snappy.ppm";

/* state */
static SpiceSession  *session;
static GMainLoop     *mainloop;

enum SpiceSurfaceFmt d_format;
gint                 d_width, d_height, d_stride;
gpointer             d_data;
/**
 * raw2jpg Writes the raw image data stored in the raw_image buffer
 * to a jpeg image with default compression and smoothing options in the file
 * specified by *filename.
 *
 * \returns positive integer if successful, -1 otherwise
 * \param *filename char string specifying the file name to save to
 *
 */
int raw2jpg(uint8_t* raw_image, int width,int height )
{
    char* filename = "ahoo.jpg";
    int bytes_per_pixel = 3;
    int color_space = JCS_RGB; /* or JCS_GRAYSCALE for grayscale images */
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    /* this is a pointer to one row of image data */
    JSAMPROW row_pointer[1];
    FILE *outfile = fopen( filename, "wb" );

    if ( !outfile )
    {
	printf("Error opening output jpeg file %s\n!", filename );
	return -1;
    }
    cinfo.err = jpeg_std_error( &jerr );
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);
    /*
    unsigned long outlen;
    unsigned char *outbuffer;
    jpeg_mem_dest (&cinfo,&outbuffer,&outlen );
    fprintf(stderr,"outlen is %lu\n",(long unsigned int)outlen);
    */

    /* Setting the parameters of the output file here */
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = bytes_per_pixel;
    cinfo.in_color_space = color_space;
    /* default compression parameters, we shouldn't be worried about these */

    jpeg_set_defaults( &cinfo );
    cinfo.num_components = 3;
    //cinfo.data_precision = 4;
    cinfo.dct_method = JDCT_FLOAT;
    jpeg_set_quality(&cinfo, 15, TRUE);
    /* Now do the compression .. */
    jpeg_start_compress( &cinfo, TRUE );
    /* like reading a file, this time write one row at a time */
    while( cinfo.next_scanline < cinfo.image_height )
    {
	row_pointer[0] = &raw_image[ cinfo.next_scanline * cinfo.image_width * cinfo.input_components];
	jpeg_write_scanlines( &cinfo, row_pointer, 1 );
    }
    /* similar to read file, clean up after we're done compressing */
    jpeg_finish_compress( &cinfo );
    jpeg_destroy_compress( &cinfo );
    fclose( outfile );
    /* success code is 1! */
    return 1;
}



/* ------------------------------------------------------------------ */
static int write_ppm_32(void)
{
    FILE *fp;
    uint8_t *p;
    int n;

    fp = fopen(outf,"w");
    if (NULL == fp) {
	fprintf(stderr, _("snappy: can't open %s: %s\n"), outf, strerror(errno));
	return -1;
    }
    fprintf(fp, "P6\n%d %d\n255\n",
	    d_width, d_height);
    n = d_width * d_height;
    p = d_data;
    uint8_t* bmp =(uint8_t*)malloc(3*d_width*d_height);
    uint8_t* loc = bmp;
    while (n > 0) {
	fputc(p[2], fp);
	loc[0]=p[2];
	fputc(p[1], fp);
	loc[1]=p[1];
	fputc(p[0], fp);
	loc[2]=p[0];
	p += 4;
	loc+=3;
	n--;
    }
    fclose(fp);
    raw2jpg(bmp,d_width,d_height);
    free(bmp);
    return 0;
}

static void primary_create(SpiceChannel *channel, gint format,
	gint width, gint height, gint stride,
	gint shmid, gpointer imgdata, gpointer data)
{
    SPICE_DEBUG("%s: %dx%d, format %d", __FUNCTION__, width, height, format);
    d_format = format;
    d_width  = width;
    d_height = height;
    d_stride = stride;
    d_data   = imgdata;
}


static void invalidate(SpiceChannel *channel,
	gint x, gint y, gint w, gint h, gpointer *data)
{
    int rc;

    switch (d_format) {
	case SPICE_SURFACE_FMT_32_xRGB:
	    rc = write_ppm_32();
	    break;
	default:
	    fprintf(stderr, _("unsupported spice surface format %d\n"), d_format);
	    rc = -1;
	    break;
    }
    if (rc == 0)
	fprintf(stderr, _("wrote screen shot to %s\n"), outf);
    g_main_loop_quit(mainloop);
}

static void channel_new(SpiceSession *s, SpiceChannel *channel, gpointer *data)
{
    int id;

    if (!SPICE_IS_DISPLAY_CHANNEL(channel))
	return;

    g_object_get(channel, "channel-id", &id, NULL);
    if (id != 0)
	return;

    g_signal_connect(channel, "display-primary-create",
	    G_CALLBACK(primary_create), NULL);
    g_signal_connect(channel, "display-invalidate",
	    G_CALLBACK(invalidate), NULL);
    spice_channel_connect(channel);
}

/* ------------------------------------------------------------------ */

static GOptionEntry app_entries[] = {
    {
	.long_name        = "out-file",
	.short_name       = 'o',
	.arg              = G_OPTION_ARG_FILENAME,
	.arg_data         = &outf,
	.description      = N_("output file name (*.ppm)"),
	.arg_description  = N_("<filename>"),
    },{
	/* end of list */
    }
};

int cmd_parse(char* cmd,char** argv,int* argc)
{
    char* ch;
    char* loc;
    int i = 0;
    int len = 0;
    loc = ch = cmd;
    bool need_parse = false;

    while(*ch!='\0') {
	if(*ch!=' '&&*ch!='\t') {
	    if(!need_parse) {
		need_parse = true;
		loc = ch;
	    }
	    ch++;
	} else {
	    if(need_parse) {
		need_parse = false;
		len = ch-loc;
		argv[i] = (char*)malloc(len+1);
		memcpy(argv[i],loc,len);
		argv[i][len] = '\0';
		i++;
	    }
	    ch++;
	}
    }
    if(need_parse) {
	need_parse = false;
	len = ch-loc;
	argv[i] = (char*)malloc(len+1);
	memcpy(argv[i],loc,len);
	argv[i][len] = '\0';
	i++;
    }
    *argc = i;
}

int main()
{
    return snappy_main("./snappy -h 192.168.1.16 -p 5900 -w gnoll -o ahoo.ppm");
}
int snappy_main(char* cmd)
{
    char** argv = (char**)malloc(12*sizeof(char*));
    int argc;
    cmd_parse(cmd,argv,&argc);
    int i;
    for(i=0;i<argc;i++)
    {
	fprintf(stderr,"argv:%s i:%d\n",argv[i],i);
    }

    GError *error = NULL;
    GOptionContext *context;

    bindtextdomain(GETTEXT_PACKAGE, SPICE_GTK_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    /* parse opts */
    context = g_option_context_new(_(" - write screen shots in ppm format"));
    g_option_context_add_main_entries(context, app_entries, NULL);
    g_option_context_add_group(context, spice_cmdline_get_option_group());
    if (!g_option_context_parse (context, &argc, &argv, &error)) {
	g_print (_("option parsing failed: %s\n"), error->message);
	exit (1);
    }

    g_type_init();
    mainloop = g_main_loop_new(NULL, false);

    session = spice_session_new();
    g_signal_connect(session, "channel-new",
	    G_CALLBACK(channel_new), NULL);
    spice_cmdline_session_setup(session);

    if (!spice_session_connect(session)) {
	fprintf(stderr, _("spice_session_connect failed\n"));
	exit(1);
    }

    g_main_loop_run(mainloop);

    for(i=0;i<argc;i++)
    {
	free(argv[i]);
    }
    fprintf(stderr,"argc:%d\n",argc);
    free(argv);

    return 0;
}

/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2013 Jeremy White

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

/* snd_codec.c
     General purpose sound codec routines for use by Spice.
   These routines abstract the work of picking a codec and
   encoding and decoding the buffers.
     Note:  these routines have some peculiarities that come from
   wanting to provide full backwards compatibility with the original
   Spice celt 0.51 implementation.  It has some hard requirements
   (fixed sample size, fixed compressed buffer size).

   See below for documentation of the public routines.
*/

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <spice/macros.h>
#include <spice/enums.h>


#include "snd_codec.h"
#include "mem.h"
#include "log.h"

typedef struct
{
    int mode;
    int frequency;
#if HAVE_CELT051
    CELTMode *celt_mode;
    CELTEncoder *celt_encoder;
    CELTDecoder *celt_decoder;
#endif

#if HAVE_OPUS
    OpusEncoder *opus_encoder;
    OpusDecoder *opus_decoder;
#endif
} SndCodecInternal;



/* celt 0.51 specific support routines */
#if HAVE_CELT051
static void snd_codec_destroy_celt051(SndCodecInternal *codec)
{
    if (codec->celt_decoder) {
        celt051_decoder_destroy(codec->celt_decoder);
        codec->celt_decoder = NULL;
    }

    if (codec->celt_encoder) {
        celt051_encoder_destroy(codec->celt_encoder);
        codec->celt_encoder = NULL;
    }

    if (codec->celt_mode) {
        celt051_mode_destroy(codec->celt_mode);
        codec->celt_mode = NULL;
    }
}

static int snd_codec_create_celt051(SndCodecInternal *codec, int purpose)
{
    int celt_error;

    codec->celt_mode = celt051_mode_create(codec->frequency,
                                           SND_CODEC_PLAYBACK_CHAN,
                                           SND_CODEC_CELT_FRAME_SIZE, &celt_error);
    if (! codec->celt_mode) {
        spice_printerr("create celt mode failed %d", celt_error);
        return SND_CODEC_UNAVAILABLE;
    }

    if (purpose & SND_CODEC_ENCODE) {
        codec->celt_encoder = celt051_encoder_create(codec->celt_mode);
        if (! codec->celt_encoder) {
            spice_printerr("create celt encoder failed");
            goto error;
        }
    }

    if (purpose & SND_CODEC_DECODE) {
        codec->celt_decoder = celt051_decoder_create(codec->celt_mode);
        if (! codec->celt_decoder) {
            spice_printerr("create celt decoder failed");
            goto error;
        }
    }

    codec->mode = SPICE_AUDIO_DATA_MODE_CELT_0_5_1;
    return SND_CODEC_OK;

error:
    snd_codec_destroy_celt051(codec);
    return SND_CODEC_UNAVAILABLE;
}

static int snd_codec_encode_celt051(SndCodecInternal *codec, uint8_t *in_ptr, int in_size, uint8_t *out_ptr, int *out_size)
{
    int n;
    if (in_size != SND_CODEC_CELT_FRAME_SIZE * SND_CODEC_PLAYBACK_CHAN * 2)
        return SND_CODEC_INVALID_ENCODE_SIZE;
    n = celt051_encode(codec->celt_encoder, (celt_int16_t *) in_ptr, NULL, out_ptr, *out_size);
    if (n < 0) {
        spice_printerr("celt051_encode failed %d\n", n);
        return SND_CODEC_ENCODE_FAILED;
    }
    *out_size = n;
    return SND_CODEC_OK;
}

static int snd_codec_decode_celt051(SndCodecInternal *codec, uint8_t *in_ptr, int in_size, uint8_t *out_ptr, int *out_size)
{
    int n;
    n = celt051_decode(codec->celt_decoder, in_ptr, in_size, (celt_int16_t *) out_ptr);
    if (n < 0) {
        spice_printerr("celt051_decode failed %d\n", n);
        return SND_CODEC_DECODE_FAILED;
    }
    *out_size = SND_CODEC_CELT_FRAME_SIZE * SND_CODEC_PLAYBACK_CHAN * 2 /* 16 fmt */;
    return SND_CODEC_OK;
}
#endif


/* Opus support routines */
#if HAVE_OPUS
static void snd_codec_destroy_opus(SndCodecInternal *codec)
{
    if (codec->opus_decoder) {
        opus_decoder_destroy(codec->opus_decoder);
        codec->opus_decoder = NULL;
    }

    if (codec->opus_encoder) {
        opus_encoder_destroy(codec->opus_encoder);
        codec->opus_encoder = NULL;
    }

}

static int snd_codec_create_opus(SndCodecInternal *codec, int purpose)
{
    int opus_error;

    if (purpose & SND_CODEC_ENCODE) {
        codec->opus_encoder = opus_encoder_create(codec->frequency,
                                SND_CODEC_PLAYBACK_CHAN,
                                OPUS_APPLICATION_AUDIO, &opus_error);
        if (! codec->opus_encoder) {
            spice_printerr("create opus encoder failed; error %d", opus_error);
            goto error;
        }
    }

    if (purpose & SND_CODEC_DECODE) {
        codec->opus_decoder = opus_decoder_create(codec->frequency,
                                SND_CODEC_PLAYBACK_CHAN, &opus_error);
        if (! codec->opus_decoder) {
            spice_printerr("create opus decoder failed; error %d", opus_error);
            goto error;
        }
    }

    codec->mode = SPICE_AUDIO_DATA_MODE_OPUS;
    return SND_CODEC_OK;

error:
    snd_codec_destroy_opus(codec);
    return SND_CODEC_UNAVAILABLE;
}

static int snd_codec_encode_opus(SndCodecInternal *codec, uint8_t *in_ptr, int in_size, uint8_t *out_ptr, int *out_size)
{
    int n;
    if (in_size != SND_CODEC_OPUS_FRAME_SIZE * SND_CODEC_PLAYBACK_CHAN * 2)
        return SND_CODEC_INVALID_ENCODE_SIZE;
    n = opus_encode(codec->opus_encoder, (opus_int16 *) in_ptr, SND_CODEC_OPUS_FRAME_SIZE, out_ptr, *out_size);
    if (n < 0) {
        spice_printerr("opus_encode failed %d\n", n);
        return SND_CODEC_ENCODE_FAILED;
    }
    *out_size = n;
    return SND_CODEC_OK;
}

static int snd_codec_decode_opus(SndCodecInternal *codec, uint8_t *in_ptr, int in_size, uint8_t *out_ptr, int *out_size)
{
    int n;
    n = opus_decode(codec->opus_decoder, in_ptr, in_size, (opus_int16 *) out_ptr,
                *out_size / SND_CODEC_PLAYBACK_CHAN / 2, 0);
    if (n < 0) {
        spice_printerr("opus_decode failed %d\n", n);
        return SND_CODEC_DECODE_FAILED;
    }
    *out_size = n * SND_CODEC_PLAYBACK_CHAN * 2 /* 16 fmt */;
    return SND_CODEC_OK;
}
#endif


/*----------------------------------------------------------------------------
**          PUBLIC INTERFACE
**--------------------------------------------------------------------------*/

/*
  snd_codec_is_capable
    Returns TRUE if the current spice implementation can
      use the given codec, FALSE otherwise.
   mode must be a SPICE_AUDIO_DATA_MODE_XXX enum from spice/enum.h
 */
int snd_codec_is_capable(int mode, int frequency)
{
#if HAVE_CELT051
    if (mode == SPICE_AUDIO_DATA_MODE_CELT_0_5_1)
        return TRUE;
#endif

#if HAVE_OPUS
    if (mode == SPICE_AUDIO_DATA_MODE_OPUS &&
         (frequency == SND_CODEC_ANY_FREQUENCY ||
          frequency == 48000 || frequency == 24000 ||
          frequency == 16000 || frequency == 12000 ||
          frequency == 8000) )
        return TRUE;
#endif

    return FALSE;
}

/*
  snd_codec_create
    Create a codec control.  Required for most functions in this library.
    Parameters:
      1.  codec     Pointer to preallocated codec control
      2.  mode      SPICE_AUDIO_DATA_MODE_XXX enum from spice/enum.h
      3.  encode    TRUE if encoding is desired
      4.  decode    TRUE if decoding is desired
     Returns:
       SND_CODEC_OK  if all went well; a different code if not.

  snd_codec_destroy is the obvious partner of snd_codec_create.
 */
int snd_codec_create(SndCodec *codec, int mode, int frequency, int purpose)
{
    int rc = SND_CODEC_UNAVAILABLE;
    SndCodecInternal **c = (SndCodecInternal **) codec;

    *c = spice_new0(SndCodecInternal, 1);
    (*c)->frequency = frequency;

#if HAVE_CELT051
    if (mode == SPICE_AUDIO_DATA_MODE_CELT_0_5_1)
        rc = snd_codec_create_celt051(*c, purpose);
#endif

#if HAVE_OPUS
    if (mode == SPICE_AUDIO_DATA_MODE_OPUS)
        rc = snd_codec_create_opus(*c, purpose);
#endif

    return rc;
}

/*
  snd_codec_destroy
    The obvious companion to snd_codec_create
*/
void snd_codec_destroy(SndCodec *codec)
{
    SndCodecInternal **c = (SndCodecInternal **) codec;
    if (! c || ! *c)
        return;

#if HAVE_CELT051
    snd_codec_destroy_celt051(*c);
#endif

#if HAVE_OPUS
    snd_codec_destroy_opus(*c);
#endif

    free(*c);
    *c = NULL;
}

/*
  snd_codec_frame_size
    Returns the size, in frames, of the raw PCM frame buffer
      required by this codec.  To get bytes, you'll need
      to multiply by channels and sample width.
 */
int snd_codec_frame_size(SndCodec codec)
{
#if defined(HAVE_CELT051) || defined(HAVE_OPUS)
    SndCodecInternal *c = (SndCodecInternal *) codec;
#endif
#if HAVE_CELT051
    if (c && c->mode == SPICE_AUDIO_DATA_MODE_CELT_0_5_1)
        return SND_CODEC_CELT_FRAME_SIZE;
#endif
#if HAVE_OPUS
    if (c && c->mode == SPICE_AUDIO_DATA_MODE_OPUS)
        return SND_CODEC_OPUS_FRAME_SIZE;
#endif
    return SND_CODEC_MAX_FRAME_SIZE;
}

/*
  snd_codec_encode
     Encode a block of data to a compressed buffer.

  Parameters:
    1.  codec       Pointer to codec control previously allocated + created
    2.  in_data     Pointer to uncompressed PCM data
    3.  in_size     Input size  (for celt, this must be a
                    particular size, governed by the frame size)
    4.  out_ptr     Pointer to area to write encoded data
    5.  out_size    On input, the maximum size of the output buffer; on
                    successful return, it will hold the number of bytes
                    returned.  For celt, this must be set to a particular
                    size to ensure compatibility.

     Returns:
       SND_CODEC_OK  if all went well
*/
int snd_codec_encode(SndCodec codec, uint8_t *in_ptr, int in_size, uint8_t *out_ptr, int *out_size)
{
#if defined(HAVE_CELT051) || defined(HAVE_OPUS)
    SndCodecInternal *c = (SndCodecInternal *) codec;
#endif
#if HAVE_CELT051
    if (c && c->mode == SPICE_AUDIO_DATA_MODE_CELT_0_5_1) {
        /* The output buffer size in celt determines the compression,
            and so is essentially mandatory to use a certain value (47) */
        if (*out_size > SND_CODEC_CELT_COMPRESSED_FRAME_BYTES)
            *out_size = SND_CODEC_CELT_COMPRESSED_FRAME_BYTES;
        return snd_codec_encode_celt051(c, in_ptr, in_size, out_ptr, out_size);
    }
#endif

#if HAVE_OPUS
    if (c && c->mode == SPICE_AUDIO_DATA_MODE_OPUS)
        return snd_codec_encode_opus(c, in_ptr, in_size, out_ptr, out_size);
#endif

    return SND_CODEC_ENCODER_UNAVAILABLE;
}

/*
  snd_codec_decode
     Decode a block of data from a compressed buffer.

  Parameters:
    1.  codec       Pointer to codec control previously allocated + created
    2.  in_data     Pointer to compressed data
    3.  in_size     Input size
    4.  out_ptr     Pointer to area to write decoded data
    5.  out_size    On input, the maximum size of the output buffer; on
                    successful return, it will hold the number of bytes
                    returned.

     Returns:
       SND_CODEC_OK  if all went well
*/
int snd_codec_decode(SndCodec codec, uint8_t *in_ptr, int in_size, uint8_t *out_ptr, int *out_size)
{
#if defined(HAVE_CELT051) || defined(HAVE_OPUS)
    SndCodecInternal *c = (SndCodecInternal *) codec;
#endif
#if HAVE_CELT051
    if (c && c->mode == SPICE_AUDIO_DATA_MODE_CELT_0_5_1)
        return snd_codec_decode_celt051(c, in_ptr, in_size, out_ptr, out_size);
#endif

#if HAVE_OPUS
    if (c && c->mode == SPICE_AUDIO_DATA_MODE_OPUS)
        return snd_codec_decode_opus(c, in_ptr, in_size, out_ptr, out_size);
#endif

    return SND_CODEC_DECODER_UNAVAILABLE;
}

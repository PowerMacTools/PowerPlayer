#include "ff.h"
#include "macros.h"

#include <stdbool.h>

void init_pq() {
  pq.head = NULL;
  pq.last = NULL;
  pq.size = 0;
  pq.codecCtx = NULL;
}

int pq_empty() { return pq.size == 0; }

void pq_put(AVPacket packet) {
  AVList *node = (AVList *)malloc(sizeof(AVList));
  node->self = packet;
  node->next = NULL;
  if (pq_empty()) {
    pq.head = node;
    pq.last = node;
  } else {
    pq.last->next = node;
    pq.last = node;
  }
  pq.size++;
}

AVPacket pq_get() {
  while (pq_empty()) {
  }
  AVList *node = pq.head;
  pq.head = pq.head->next;
  AVPacket p = node->self;
  free(node);
  if (pq.head == NULL) {
    pq.last = NULL;
  }
  pq.size--;
  return p;
}

void pq_free() {
  // Free all nodes
  printf("total %d node found, destroying them...\n", pq.size);
  AVList *node = pq.head;
  while (node != NULL) {
    AVList *next = node->next;
    av_packet_unref(&node->self);
    free(node);
    node = next;
  }
}

int audio_decode_frame(uint8_t *buf) {
  AVPacket packet = pq_get();
  int ret = avcodec_send_packet(pq.codecCtx, &packet);
  if (ret < 0) {
    printf("Error sending a packet for decoding\n");
    av_packet_unref(&packet);
    return -1;
  }
  AVFrame *frame = av_frame_alloc();

  ret = avcodec_receive_frame(pq.codecCtx, frame);
  if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    av_frame_free(&frame);
    av_packet_unref(&packet);
    return -1;
  } else if (ret < 0) {
    printf("Error during decoding\n");
    av_frame_free(&frame);
    av_packet_unref(&packet);
    return -1;
  }
  // Got frame
  // uint f_size = frame->linesize[0];
  // uint b_ch = f_size / pq.codecCtx->ch_layout.nb_channels;
  int res =
      audio_resampling(pq.codecCtx, frame, AV_SAMPLE_FMT_FLT, 2, 44100, buf);

  av_frame_free(&frame);
  av_packet_unref(&packet);
  return res;
}

void audio_callback(void *buffer, unsigned int frames) {
  // uint8_t *origin = (uint8_t *)buffer;
  uint8_t *dbuf = (uint8_t *)buffer;
  static uint8_t audio_buf[19200];
  static unsigned int audio_buf_size = 0;
  static unsigned int audio_buf_index = 0;
  int len1 = -1;
  int audio_size = -1;
  int len = frames * sizeof(float) * 2; // Stereo
  static int jj = 0;
  ++jj;
  printf("AFrame: %d, %d\n", jj, pq.size);
  while (len > 0) {
    if (audio_buf_index >= audio_buf_size) {
      audio_size = audio_decode_frame(audio_buf);
      if (audio_size < 0) {
        // output silence
        printf("Skipped one frame.\n");
        continue;
      } else {
        audio_buf_size = audio_size;
      }
      audio_buf_index = 0;
    }
    len1 = audio_buf_size - audio_buf_index;

    if (len1 > len) {
      len1 = len;
    }

    memcpy(dbuf, audio_buf + audio_buf_index, len1);

    len -= len1;
    dbuf += len1;
    audio_buf_index += len1;
  }
}

int audio_resampling(AVCodecContext *audio_decode_ctx,
                     AVFrame *decoded_audio_frame,
                     enum AVSampleFormat out_sample_fmt, int out_channels,
                     int out_sample_rate, uint8_t *out_buf) {
  SwrContext *swr_ctx = NULL;
  int ret = 0;
  AVChannelLayout in_channel_layout = audio_decode_ctx->ch_layout;
  AVChannelLayout out_channel_layout = AV_CHANNEL_LAYOUT_STEREO;
  int out_nb_channels = 0;
  int out_linesize = 0;
  int in_nb_samples = 0;
  int out_nb_samples = 0;
  int max_out_nb_samples = 0;
  uint8_t **resampled_data = NULL;
  int resampled_data_size = 0;

  swr_ctx = swr_alloc();

  if (!swr_ctx) {
    printf("swr_alloc error.\n");
    return -1;
  }

  // set output audio channels based on the input audio channels
  if (out_channels == 1) {
    AVChannelLayout tmp = AV_CHANNEL_LAYOUT_MONO;
    out_channel_layout = tmp;
  } else if (out_channels == 2) {
    AVChannelLayout tmp = AV_CHANNEL_LAYOUT_STEREO;
    out_channel_layout = tmp;
  } else {
    AVChannelLayout tmp = AV_CHANNEL_LAYOUT_SURROUND;
    out_channel_layout = tmp;
  }

  // retrieve number of audio samples (per channel)
  in_nb_samples = decoded_audio_frame->nb_samples;
  if (in_nb_samples <= 0) {
    printf("in_nb_samples error.\n");
    return -1;
  }

  swr_alloc_set_opts2(&swr_ctx, &out_channel_layout, out_sample_fmt,
                      out_sample_rate, &in_channel_layout,
                      audio_decode_ctx->sample_fmt,
                      audio_decode_ctx->sample_rate, 0, NULL);

  // Once all values have been set for the SwrContext, it must be initialized
  // with swr_init().
  ret = swr_init(swr_ctx);
  ;
  if (ret < 0) {
    printf("Failed to initialize the resampling context.\n");
    return -1;
  }

  max_out_nb_samples = out_nb_samples =
      av_rescale_rnd(in_nb_samples, out_sample_rate,
                     audio_decode_ctx->sample_rate, AV_ROUND_UP);

  // check rescaling was successful
  if (max_out_nb_samples <= 0) {
    printf("av_rescale_rnd error.\n");
    return -1;
  }

  // get number of output audio channels
  out_nb_channels = out_channel_layout.nb_channels;

  ret = av_samples_alloc_array_and_samples(&resampled_data, &out_linesize,
                                           out_nb_channels, out_nb_samples,
                                           out_sample_fmt, 0);

  if (ret < 0) {
    printf("av_samples_alloc_array_and_samples() error: Could not allocate "
           "destination "
           "samples.\n");
    return -1;
  }

  // retrieve output samples number taking into account the progressive delay
  out_nb_samples = av_rescale_rnd(
      swr_get_delay(swr_ctx, audio_decode_ctx->sample_rate) + in_nb_samples,
      out_sample_rate, audio_decode_ctx->sample_rate, AV_ROUND_UP);

  // check output samples number was correctly retrieved
  if (out_nb_samples <= 0) {
    printf("av_rescale_rnd error\n");
    return -1;
  }

  if (out_nb_samples > max_out_nb_samples) {
    // free memory block and set pointer to NULL
    av_free(resampled_data[0]);

    // Allocate a samples buffer for out_nb_samples samples
    ret = av_samples_alloc(resampled_data, &out_linesize, out_nb_channels,
                           out_nb_samples, out_sample_fmt, 1);

    // check samples buffer correctly allocated
    if (ret < 0) {
      printf("av_samples_alloc failed.\n");
      return -1;
    }

    max_out_nb_samples = out_nb_samples;
  }

  if (swr_ctx) {
    // do the actual audio data resampling
    ret = swr_convert(swr_ctx, resampled_data, out_nb_samples,
                      (const uint8_t **)decoded_audio_frame->extended_data,
                      decoded_audio_frame->nb_samples);

    // check audio conversion was successful
    if (ret < 0) {
      printf("swr_convert_error.\n");
      return -1;
    }

    // Get the required buffer size for the given audio parameters
    resampled_data_size = av_samples_get_buffer_size(
        &out_linesize, out_nb_channels, ret, out_sample_fmt, 1);

    // check audio buffer size
    if (resampled_data_size < 0) {
      printf("av_samples_get_buffer_size error.\n");
      return -1;
    }
  } else {
    printf("swr_ctx null error.\n");
    return -1;
  }

  // copy the resampled data to the output buffer
  memcpy(out_buf, resampled_data[0], resampled_data_size);

  /*
   * Memory Cleanup.
   */
  if (resampled_data) {
    // free memory block and set pointer to NULL
    av_freep(&resampled_data[0]);
  }

  av_freep(&resampled_data);
  resampled_data = NULL;

  if (swr_ctx) {
    // Free the given SwrContext and set the pointer to NULL
    swr_free(&swr_ctx);
  }

  return resampled_data_size;
}

#define __UNSUPPORTED_ENUM(enum)                                               \
  case enum:                                                                   \
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported enum " #enum "\n");          \
    return 0;

#define RETURN_AND_PRINT(enum)                                                 \
  printf("Selected GL Enum " #enum "\n");                                      \
  return enum;

GLint ffmpeg_pix_format_to_gl(int pix_format) {
  switch (pix_format) {
  case AV_PIX_FMT_0RGB:
    RETURN_AND_PRINT(GL_RGB);

  case AV_PIX_FMT_RGB0:
  case AV_PIX_FMT_RGB24:
    RETURN_AND_PRINT(GL_RGB);
  case AV_PIX_FMT_RGB4:
  case AV_PIX_FMT_RGB444:

  case AV_PIX_FMT_RGB32_1:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_RGB32_1\n");
    return 0;
  case AV_PIX_FMT_RGB565:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_RGB565\n");
    return 0;

  case AV_PIX_FMT_BGR0:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_BGR0\n");
    return 0;
  case AV_PIX_FMT_BGR24:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_BGR24\n");
    return 0;
  case AV_PIX_FMT_BGR4:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_BGR4\n");
    return 0;
  case AV_PIX_FMT_BGR444:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_BGR444\n");
    return 0;

  case AV_PIX_FMT_BGR48:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_BGR48\n");
    return 0;

  case AV_PIX_FMT_BGR4_BYTE:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_BGR4_BYTE\n");
    return 0;
  case AV_PIX_FMT_BGR555:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_BGR555\n");
    return 0;

  case AV_PIX_FMT_BGR565:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_BGR565\n");
    return 0;

  case AV_PIX_FMT_BGR8:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_BGR8\n");
    return 0;
  case AV_PIX_FMT_BGRA64:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_BGRA64\n");
    return 0;

  case AV_PIX_FMT_RGBAF32:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_RGBAF32\n");
    return 0;

  case AV_PIX_FMT_RGBF32:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_RGBF32\n");
    return 0;

  case AV_PIX_FMT_ARGB:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_ARGB\n");
    return 0;
  case AV_PIX_FMT_AYUV64:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_AYUV64\n");
    return 0;

  case AV_PIX_FMT_BAYER_BGGR16:
    __THROW_FATAL_ERROR(
        1, "GL Error: Unsupported format AV_PIX_FMT_BAYER_BGGR16\n");
    return 0;

  case AV_PIX_FMT_BAYER_BGGR8:
    __THROW_FATAL_ERROR(
        1, "GL Error: Unsupported format AV_PIX_FMT_BAYER_BGGR8\n");
    return 0;
  case AV_PIX_FMT_BAYER_GBRG16:
    __THROW_FATAL_ERROR(
        1, "GL Error: Unsupported format AV_PIX_FMT_BAYER_GBRG16\n");
    return 0;

  case AV_PIX_FMT_BAYER_GBRG8:
    __THROW_FATAL_ERROR(
        1, "GL Error: Unsupported format AV_PIX_FMT_BAYER_GBRG8\n");
    return 0;
  case AV_PIX_FMT_BAYER_GRBG16:
    __THROW_FATAL_ERROR(
        1, "GL Error: Unsupported format AV_PIX_FMT_BAYER_GRBG16\n");
    return 0;

  case AV_PIX_FMT_BAYER_GRBG8:
    __THROW_FATAL_ERROR(
        1, "GL Error: Unsupported format AV_PIX_FMT_BAYER_GRBG8\n");
    return 0;
  case AV_PIX_FMT_BAYER_RGGB16:
    __THROW_FATAL_ERROR(
        1, "GL Error: Unsupported format AV_PIX_FMT_BAYER_RGGB16\n");
    return 0;

  case AV_PIX_FMT_BAYER_RGGB8:
    __THROW_FATAL_ERROR(
        1, "GL Error: Unsupported format AV_PIX_FMT_BAYER_RGGB8\n");
    return 0;
  case AV_PIX_FMT_CUDA:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_CUDA\n");
    return 0;
  case AV_PIX_FMT_D3D11:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_D3D11\n");
    return 0;
  case AV_PIX_FMT_D3D11VA_VLD:
    __THROW_FATAL_ERROR(
        1, "GL Error: Unsupported format AV_PIX_FMT_D3D11VA_VLD\n");
    return 0;
  case AV_PIX_FMT_DRM_PRIME:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_DRM_PRIME\n");
    return 0;
  case AV_PIX_FMT_DXVA2_VLD:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_DXVA2_VLD\n");
    return 0;
  case AV_PIX_FMT_GBR24P:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GBR24P\n");
    return 0;
  case AV_PIX_FMT_GBRAP:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GBRAP\n");
    return 0;
  case AV_PIX_FMT_GBRAP10:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GBRAP10\n");
    return 0;

  case AV_PIX_FMT_GBRAP12:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GBRAP12\n");
    return 0;

  case AV_PIX_FMT_GBRAP14:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GBRAP14\n");
    return 0;

  case AV_PIX_FMT_GBRAP16:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GBRAP16\n");
    return 0;

  case AV_PIX_FMT_GBRAPF32:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_GBRAPF32\n");
    return 0;

  case AV_PIX_FMT_GBRP10:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GBRP10\n");
    return 0;

  case AV_PIX_FMT_GBRP12:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GBRP12\n");
    return 0;

  case AV_PIX_FMT_GBRP14:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GBRP14\n");
    return 0;

  case AV_PIX_FMT_GBRP16:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GBRP16\n");
    return 0;

  case AV_PIX_FMT_GBRP9:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GBRP9\n");
    return 0;

  case AV_PIX_FMT_GBRPF32:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GBRPF32\n");
    return 0;

  case AV_PIX_FMT_GRAY10:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GRAY10\n");
    return 0;

  case AV_PIX_FMT_GRAY12:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GRAY12\n");
    return 0;

  case AV_PIX_FMT_GRAY14:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GRAY14\n");
    return 0;

  case AV_PIX_FMT_GRAY16:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GRAY16\n");
    return 0;

  case AV_PIX_FMT_GRAY8:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GRAY8\n");
    return 0;
  case AV_PIX_FMT_GRAY8A:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GRAY8A\n");
    return 0;
  case AV_PIX_FMT_GRAY9:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GRAY9\n");
    return 0;

  case AV_PIX_FMT_GRAYF32:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_GRAYF32\n");
    return 0;

  case AV_PIX_FMT_MEDIACODEC:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_MEDIACODEC\n");
    return 0;
  case AV_PIX_FMT_MMAL:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_MMAL\n");
    return 0;
  case AV_PIX_FMT_MONOBLACK:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_MONOBLACK\n");
    return 0;
  case AV_PIX_FMT_MONOWHITE:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_MONOWHITE\n");
    return 0;
  case AV_PIX_FMT_NB:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_NB\n");
    return 0;
  case AV_PIX_FMT_NONE:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_NONE\n");
    return 0;
  case AV_PIX_FMT_NV12:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_NV12\n");
    return 0;
  case AV_PIX_FMT_NV16:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_NV16\n");
    return 0;
  case AV_PIX_FMT_NV20:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_NV20\n");
    return 0;

  case AV_PIX_FMT_NV21:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_NV21\n");
    return 0;
  case AV_PIX_FMT_NV24:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_NV24\n");
    return 0;
  case AV_PIX_FMT_NV42:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_NV42\n");
    return 0;
  case AV_PIX_FMT_OPENCL:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_OPENCL\n");
    return 0;
  case AV_PIX_FMT_P010:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_P010\n");
    return 0;

  case AV_PIX_FMT_P012:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_P012\n");
    return 0;

  case AV_PIX_FMT_P016:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_P016\n");
    return 0;

  case AV_PIX_FMT_P210:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_P210\n");
    return 0;

  case AV_PIX_FMT_P212:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_P212\n");
    return 0;

  case AV_PIX_FMT_P216:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_P216\n");
    return 0;

  case AV_PIX_FMT_P410:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_P410\n");
    return 0;

  case AV_PIX_FMT_P412:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_P412\n");
    return 0;

  case AV_PIX_FMT_P416:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_P416\n");
    return 0;

  case AV_PIX_FMT_PAL8:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_PAL8\n");
    return 0;
  case AV_PIX_FMT_QSV:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_QSV\n");
    return 0;

  case AV_PIX_FMT_RGB48:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_RGB48\n");
    return 0;

  case AV_PIX_FMT_RGBA64:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_RGBA64\n");
    return 0;

  case AV_PIX_FMT_UYVY422:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_UYVY422\n");
    return 0;
  case AV_PIX_FMT_UYYVYY411:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_UYYVYY411\n");
    return 0;
  case AV_PIX_FMT_VAAPI:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_VAAPI\n");
    return 0;
  case AV_PIX_FMT_VDPAU:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_VDPAU\n");
    return 0;
  case AV_PIX_FMT_VIDEOTOOLBOX:
    __THROW_FATAL_ERROR(
        1, "GL Error: Unsupported format AV_PIX_FMT_VIDEOTOOLBOX\n");
    return 0;
  case AV_PIX_FMT_VULKAN:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_VULKAN\n");
    return 0;
  case AV_PIX_FMT_VUYA:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_VUYA\n");
    return 0;
  case AV_PIX_FMT_VUYX:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_VUYX\n");
    return 0;
  case AV_PIX_FMT_X2BGR10:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_X2BGR10\n");
    return 0;

  case AV_PIX_FMT_XV30:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_XV30\n");
    return 0;

  case AV_PIX_FMT_XV36:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_XV36\n");
    return 0;

  case AV_PIX_FMT_XYZ12:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_XYZ12\n");
    return 0;

  case AV_PIX_FMT_Y210:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_Y210\n");
    return 0;

  case AV_PIX_FMT_Y212:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_Y212\n");
    return 0;

  case AV_PIX_FMT_YA16:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_YA16\n");
    return 0;

  case AV_PIX_FMT_YUV410P:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_YUV410P\n");
    return 0;
  case AV_PIX_FMT_YUV411P:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_YUV411P\n");
    return 0;
  case AV_PIX_FMT_YUV420P:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_YUV420P\n");
    return 0;
  case AV_PIX_FMT_YUV420P10:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV420P10\n");
    return 0;

  case AV_PIX_FMT_YUV420P12:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV420P12\n");
    return 0;

  case AV_PIX_FMT_YUV420P14:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV420P14\n");
    return 0;

  case AV_PIX_FMT_YUV420P16:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV420P16\n");
    return 0;

  case AV_PIX_FMT_YUV420P9:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV420P9\n");
    return 0;

  case AV_PIX_FMT_YUV422P:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_YUV422P\n");
    return 0;
  case AV_PIX_FMT_YUV422P10:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV422P10\n");
    return 0;

  case AV_PIX_FMT_YUV422P12:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV422P12\n");
    return 0;

  case AV_PIX_FMT_YUV422P14:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV422P14\n");
    return 0;

  case AV_PIX_FMT_YUV422P16:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV422P16\n");
    return 0;

  case AV_PIX_FMT_YUV422P9:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV422P9\n");
    return 0;

  case AV_PIX_FMT_YUV440P:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_YUV440P\n");
    return 0;
  case AV_PIX_FMT_YUV440P10:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV440P10\n");
    return 0;

  case AV_PIX_FMT_YUV440P12:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV440P12\n");
    return 0;

  case AV_PIX_FMT_YUV444P:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_YUV444P\n");
    return 0;
  case AV_PIX_FMT_YUV444P10:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV444P10\n");
    return 0;

  case AV_PIX_FMT_YUV444P12:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV444P12\n");
    return 0;

  case AV_PIX_FMT_YUV444P14:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV444P14\n");
    return 0;

  case AV_PIX_FMT_YUV444P16:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV444P16\n");
    return 0;

  case AV_PIX_FMT_YUV444P9:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUV444P9\n");
    return 0;

  case AV_PIX_FMT_YUVA420P:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA420P\n");
    return 0;
  case AV_PIX_FMT_YUVA420P10:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA420P10\n");
    return 0;

  case AV_PIX_FMT_YUVA420P16:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA420P16\n");
    return 0;

  case AV_PIX_FMT_YUVA420P9:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA420P9\n");
    return 0;

  case AV_PIX_FMT_YUVA422P:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA422P\n");
    return 0;
  case AV_PIX_FMT_YUVA422P10:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA422P10\n");
    return 0;

  case AV_PIX_FMT_YUVA422P12:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA422P12\n");
    return 0;

  case AV_PIX_FMT_YUVA422P16:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA422P16\n");
    return 0;

  case AV_PIX_FMT_YUVA422P9:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA422P9\n");
    return 0;

  case AV_PIX_FMT_YUVA444P:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA444P\n");
    return 0;
  case AV_PIX_FMT_YUVA444P10:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA444P10\n");
    return 0;

  case AV_PIX_FMT_YUVA444P12:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA444P12\n");
    return 0;

  case AV_PIX_FMT_YUVA444P16:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA444P16\n");
    return 0;
  case AV_PIX_FMT_YUVA444P9:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVA444P9\n");
    return 0;
  case AV_PIX_FMT_YUVJ411P:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVJ411P\n");
    return 0;
  case AV_PIX_FMT_YUVJ420P:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVJ420P\n");
    return 0;
  case AV_PIX_FMT_YUVJ422P:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVJ422P\n");
    return 0;
  case AV_PIX_FMT_YUVJ440P:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVJ440P\n");
    return 0;
  case AV_PIX_FMT_YUVJ444P:
    __THROW_FATAL_ERROR(1,
                        "GL Error: Unsupported format AV_PIX_FMT_YUVJ444P\n");
    return 0;
  case AV_PIX_FMT_YUYV422:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_YUYV422\n");
    return 0;
  case AV_PIX_FMT_YVYU422:
    __THROW_FATAL_ERROR(1, "GL Error: Unsupported format AV_PIX_FMT_YVYU422\n");
    return 0;
  }
}

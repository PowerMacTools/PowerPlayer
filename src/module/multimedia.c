#include "../ff/ff.h"
#include "../macros.h"
#include "GL/gl.h"
#include "module.h"
#include <Mw/BaseTypes.h>
#include <Mw/TypeDefs.h>

typedef struct pp_player_t {
  AVFormatContext *pFormatCtx;
  struct SwsContext *img_convert_ctx;
  struct SwsContext *sws_ctx;
  AVStream *videoStream;
  AVStream *audioStream;
  AVCodecParameters *videoPar;
  AVCodecParameters *audioPar;

  const AVCodec *videoCodec;
  const AVCodec *audioCodec;
  AVCodecContext *audioCodecCtx;
  AVCodecContext *videoCodecCtx;
  AVFrame *frame;
  AVPacket *packet;

  int vframe;
  GLint format;

  MwBool hasVideo;
  MwBool hasAudio;

  int realWidth;
  int realHeight;

  float widthDiff;
  float heightDiff;

  AVFrame *pRGBFrame;
  GLuint textureID;
} pp_player;

static pp_player *pl = NULL;

int pp_player_framerate(pp_player *self) {
  return self->videoCodecCtx->framerate.num;
}

pp_player *pp_player_create(char *buf) {
  pp_player *player = malloc(sizeof(pp_player));
  memset(player, 0, sizeof(pp_player));

  init_pq();

  // Texture texture = {0};
  player->pFormatCtx = avformat_alloc_context();
  player->img_convert_ctx = sws_alloc_context();
  avformat_open_input(&player->pFormatCtx, (const char *)buf, NULL, NULL);
  printf("CODEC: Format %s\n", player->pFormatCtx->iformat->long_name);
  avformat_find_stream_info(player->pFormatCtx, NULL);

  for (unsigned int i = 0; i < player->pFormatCtx->nb_streams; i++) {
    AVStream *tmpStream = player->pFormatCtx->streams[i];
    AVCodecParameters *tmpPar = tmpStream->codecpar;
    if (tmpPar->codec_type == AVMEDIA_TYPE_AUDIO) {
      player->audioStream = tmpStream;
      player->audioPar = tmpPar;
      printf("CODEC: Audio sample rate %d, channels: %d\n",
             player->audioPar->sample_rate,
             player->audioPar->ch_layout.nb_channels);
      player->hasAudio = MwTRUE;
      continue;
    }
    if (tmpPar->codec_type == AVMEDIA_TYPE_VIDEO) {
      player->videoStream = tmpStream;
      player->videoPar = tmpPar;
      printf("CODEC: Resolution %d x %d, type: %d\n", player->videoPar->width,
             player->videoPar->height, player->videoPar->codec_id);
      player->hasVideo = MwTRUE;
      continue;
    }
  }

  if (player->hasVideo) {
    player->videoCodec = avcodec_find_decoder(player->videoPar->codec_id);
    printf("CODEC: %s ID %d, Bit rate %lld\n", player->videoCodec->name,
           player->videoCodec->id, player->videoPar->bit_rate);
    printf("FPS: %d/%d, TBR: %d/%d, TimeBase: %d/%d\n",
           player->videoStream->avg_frame_rate.num,
           player->videoStream->avg_frame_rate.den,
           player->videoStream->r_frame_rate.num,
           player->videoStream->r_frame_rate.den,
           player->videoStream->time_base.num,
           player->videoStream->time_base.den);
    player->videoCodecCtx = avcodec_alloc_context3(player->videoCodec);
    avcodec_parameters_to_context(player->videoCodecCtx, player->videoPar);
    avcodec_open2(player->videoCodecCtx, player->videoCodec, NULL);
  }

  player->audioCodec = avcodec_find_decoder(player->audioPar->codec_id);
  player->audioCodecCtx = avcodec_alloc_context3(player->audioCodec);
  pq.codecCtx = player->audioCodecCtx;
  avcodec_parameters_to_context(player->audioCodecCtx, player->audioPar);
  avcodec_open2(player->audioCodecCtx, player->audioCodec, NULL);

  player->frame = av_frame_alloc();
  player->packet = av_packet_alloc();

  if (player->hasVideo) {
    player->sws_ctx = sws_getContext(
        player->videoCodecCtx->width, player->videoCodecCtx->height,
        player->videoCodecCtx->pix_fmt, player->videoCodecCtx->width,
        player->videoCodecCtx->height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, 0,
        0, 0);
    player->pRGBFrame = av_frame_alloc();
    player->pRGBFrame->format = AV_PIX_FMT_RGB24;
    player->pRGBFrame->width =
        pow(2, ceil(log(player->videoCodecCtx->width) / log(2)));
    player->pRGBFrame->height =
        pow(2, ceil(log(player->videoCodecCtx->height) / log(2)));
    player->realWidth = player->videoCodecCtx->width;
    player->realHeight = player->videoCodecCtx->height;

    player->widthDiff =
        (float)player->realWidth / (float)player->pRGBFrame->width;
    player->heightDiff =
        (float)player->realHeight / (float)player->pRGBFrame->height;
    av_frame_get_buffer(player->pRGBFrame, 0);
  }
  return player;
}
void pp_player_step(pp_player *p) {
  p->vframe++;
  while (av_read_frame(p->pFormatCtx, p->packet) >= 0) {
    if (p->packet->stream_index == p->audioStream->index) {
      // Getting audio data from audio
      AVPacket *cloned = av_packet_clone(p->packet);
      pq_put(*cloned);
    } else if (p->hasVideo) {
      if (p->packet->stream_index == p->videoStream->index) {
        // Getting frame from video
        // printf("avcodec_send_packet\n");
        int ret = avcodec_send_packet(p->videoCodecCtx, p->packet);
        if (ret < 0) {
          // Error
          printf("Error sending packet\n");
          continue;
        }
        while (ret >= 0) {
          // printf("avcodec_receive_frame\n");
          ret = avcodec_receive_frame(p->videoCodecCtx, p->frame);
          if (ret == AVERROR(EAGAIN)) {
            break;
          }
          if (ret == AVERROR_EOF) {
            printf("end\n");
            exit(0);
          }
          // printf("sws_scale\n");
          sws_scale(p->sws_ctx, (uint8_t const *const *)p->frame->data,
                    p->frame->linesize, 0, p->frame->height, p->pRGBFrame->data,
                    p->pRGBFrame->linesize);
        }
        av_packet_unref(p->packet);
        break;
      }
    }
    av_packet_unref(p->packet);
  }
  if (p->hasVideo) {
    if (p->vframe == p->videoStream->nb_frames) {
      exit(1);
    }
  }
}

void pp_player_drop(pp_player *p) {
  av_frame_free(&p->frame);
  av_frame_free(&p->pRGBFrame);
  av_packet_unref(p->packet);
  av_packet_free(&p->packet);
  avcodec_free_context(&p->videoCodecCtx);
  sws_freeContext(p->sws_ctx);

  avformat_close_input(&p->pFormatCtx);
  pq_free();
}

static void InitTexture(GLsizei width, GLsizei height, uint8_t *data) {
  GL_COMMAND(glGenTextures(1, &pl->textureID));
  GL_COMMAND(glBindTexture(GL_TEXTURE_2D, pl->textureID));
  GL_COMMAND(glTexImage2D(GL_TEXTURE_2D, 0, pl->format, width, height, 0,
                          GL_RGB, GL_UNSIGNED_BYTE, data));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
}

static void UpdateTexture(GLsizei width, GLsizei height, uint8_t *data) {
  GL_COMMAND(glBindTexture(GL_TEXTURE_2D, pl->textureID));
  GL_COMMAND(glTexImage2D(GL_TEXTURE_2D, 0, pl->format, width, height, 0,
                          GL_RGB, GL_UNSIGNED_BYTE, data));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
}

static void display_hasVideo(void) {
  GL_COMMAND(glClearColor(1.0f, 0.0f, 1.0f, 1.0f));
  GL_COMMAND(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
  GL_COMMAND(glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL));
  GL_COMMAND(glTranslatef(0, 0, 0));

  AVFrame *frame = pl->pRGBFrame;
  UpdateTexture(frame->width, frame->height, frame->data[0]);

  GL_COMMAND(glBindTexture(GL_TEXTURE_2D, pl->textureID));

  GL_COMMAND(glEnable(GL_TEXTURE_2D));

  glBegin(GL_QUADS);

  printf("%0.2f\n", pl->widthDiff);
  glTexCoord2f(0.0, pl->heightDiff);
  glVertex2f(-1.0, -1.0);
  glTexCoord2f(pl->widthDiff, pl->heightDiff);
  glVertex2f(1.0, -1.0);
  glTexCoord2f(pl->widthDiff, 0.0);
  glVertex2f(1.0, 1.0);
  glTexCoord2f(0.0, 0.0);
  glVertex2f(-1.0, 1.0);
  glEnd();
}

static void display_onlyAudio(void) {
  GL_COMMAND(glClearColor(1.0f, 1.0f, 1.0f, 1.0f));
}

static void module_setup(char *buf) {
  pl = pp_player_create(buf);

  AVFrame *frame = pl->pRGBFrame;
  InitTexture(frame->width, frame->height, frame->data[0]);

  pl->format = ffmpeg_pix_format_to_gl(pl->pRGBFrame->format);
}

static void module_step() {
  int framerate = pp_player_framerate(pl);
  pp_player_step(pl);

  if (pl->hasVideo) {
    display_hasVideo();
  } else {
    display_onlyAudio();
  }

#ifdef __RETRO68__
  PrimeTime(&what, framerate);
#endif
}

pp_module pp_multimedia_module(char *buf) {
  return (pp_module){
      .setup = module_setup,
      .step = module_step,
  };
};

void PauseIfGLError(const char *file, int line_num, const char *code) {
  GLenum err = glGetError();
  switch (err) {
  case GL_NO_ERROR:
    return;
  case GL_INVALID_ENUM:
    __THROW_ERROR(err, "OpenGL Error at %s:%d \"%s\": Invalid Enum\n", file,
                  line_num, code);
    break;
  case GL_INVALID_VALUE:
    __THROW_ERROR(err, "OpenGL Error at %s:%d \"%s\": Invalid Value\n", file,
                  line_num, code);
    break;
  case GL_INVALID_OPERATION:
    __THROW_ERROR(err, "OpenGL Error at %s:%d \"%s\": Invalid Operation\n",
                  file, line_num, code);
    break;
  case GL_OUT_OF_MEMORY:
    __THROW_ERROR(err, "OpenGL Error at %s:%d \"%s\": Out of Memory\n", file,
                  line_num, code);
    break;
  case GL_STACK_UNDERFLOW:
    __THROW_ERROR(err, "OpenGL Error at %s:%d \"%s\": Stack Underflow\n", file,
                  line_num, code);
    break;
  case GL_STACK_OVERFLOW:
    __THROW_ERROR(err, "OpenGL Error at %s:%d \"%s\": Stack Overflow\n", file,
                  line_num, code);
    break;
  }
}

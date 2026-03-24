#pragma once

#include "ff.h"
#include <stdbool.h>

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

  bool hasVideo;
  bool hasAudio;

  int realWidth;
  int realHeight;

  float widthDiff;
  float heightDiff;

  AVFrame *pRGBFrame;
} pp_player;

int pp_player_framerate(pp_player *self);
pp_player *pp_player_create(char *buf);
void pp_player_step(pp_player *);
void pp_player_drop(pp_player *);

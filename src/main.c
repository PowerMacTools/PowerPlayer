#include "GL/gl.h"
#include "macros.h"
#include "player.h"
#include <Mw/Constants.h>
#include <Mw/Core.h>
#include <Mw/Milsko.h>
#include <Mw/StringDefs.h>
#include <Mw/TypeDefs.h>
#include <Mw/Widget/Window.h>
#include <libavutil/frame.h>

#include <Mw/Widget/OpenGL.h>

#ifdef __RETRO68__
#include <Timer.h>
#endif

static pp_player *pl = NULL;

static MwWidget *window = NULL;
static MwWidget *gl = NULL;

void idle(MwWidget handle, void *user_data, void *call_data);
GLuint textureID;

GLint format = GL_RGB;
void InitTexture(GLsizei width, GLsizei height, uint8_t *data);

#ifdef __RETRO68__
static QElem what;
#endif

int main(int argc, char **argv) {
  MwLibraryInit();

#ifdef __RETRO68__
  InsTime(&what);
#endif
  char buf[255];
  memset(buf, 0, 255);
  if (argc < 2) {
    printf("Type a file to open: \n");
    scanf("%s", &buf);
  } else {
    strncpy(buf, argv[1], 255);
  }
  pl = pp_player_create(buf);

  if (pl->hasVideo) {
    window = MwCreateWidget(MwWindowClass, buf, NULL, MwDEFAULT, MwDEFAULT,
                            pl->realWidth, pl->realHeight);
    gl = MwCreateWidget(MwOpenGLClass, "gl", window, MwDEFAULT, MwDEFAULT,
                        pl->realWidth, pl->realHeight);

    AVFrame *frame = pl->pRGBFrame;
    InitTexture(frame->width, frame->height, frame->data[0]);

    format = ffmpeg_pix_format_to_gl(pl->pRGBFrame->format);
  } else {
    window = MwCreateWidget(MwWindowClass, buf, NULL, MwDEFAULT, MwDEFAULT, 512,
                            384);
    gl = MwCreateWidget(MwOpenGLClass, "gl", window, MwDEFAULT, MwDEFAULT, 512,
                        384);
  }

  MwAddUserHandler(window, MwNtickHandler, idle, NULL);

  MwLoop(window);

  printf("Program exit.\n");
  getchar();
  return 0;
}

void InitTexture(GLsizei width, GLsizei height, uint8_t *data) {

  // glActiveTexture(GL_TEXTURE);
  GL_COMMAND(glGenTextures(1, &textureID));
  GL_COMMAND(glBindTexture(GL_TEXTURE_2D, textureID));
  GL_COMMAND(glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGB,
                          GL_UNSIGNED_BYTE, data));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
}

void UpdateTexture(GLsizei width, GLsizei height, uint8_t *data) {
  GL_COMMAND(glBindTexture(GL_TEXTURE_2D, textureID));
  GL_COMMAND(glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGB,
                          GL_UNSIGNED_BYTE, data));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
}

void display_hasVideo(void) {

  GL_COMMAND(glClearColor(1.0f, 0.0f, 1.0f, 1.0f));
  GL_COMMAND(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
  GL_COMMAND(glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL));
  GL_COMMAND(glPushMatrix());
  GL_COMMAND(glTranslatef(0, 0, 0));

  AVFrame *frame = pl->pRGBFrame;
  UpdateTexture(frame->width, frame->height, frame->data[0]);

  GL_COMMAND(glBindTexture(GL_TEXTURE_2D, textureID));

  GL_COMMAND(glEnable(GL_TEXTURE_2D));

  glBegin(GL_QUADS);

  glTexCoord2f(0.0, pl->heightDiff);
  glVertex2f(-1.0, -1.0);
  glTexCoord2f(pl->widthDiff, pl->heightDiff);
  glVertex2f(1.0, -1.0);
  glTexCoord2f(pl->widthDiff, 0.0);
  glVertex2f(1.0, 1.0);
  glTexCoord2f(0.0, 0.0);
  glVertex2f(-1.0, 1.0);
  glEnd();

  GL_COMMAND(glPopMatrix());
}

void display_onlyAudio(void) {
  GL_COMMAND(glClearColor(1.0f, 1.0f, 1.0f, 1.0f));
}

void idle(MwWidget handle, void *user_data, void *call_data) {
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

  MwOpenGLSwapBuffer(gl);
}

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

// #include "SDL.h"
#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glut.h"
#include "player.hpp"
#include <chrono>
#include <cstring>
// #include "console/Console.hpp"
#include "macros.h"

#ifdef __RETRO68__
#include <Timer.h>
#endif

static player::Player *pl = NULL;

void display(void);
void idle(void);
GLuint textureID;
int window;

GLint format = GL_RGB;
void InitTexture(GLsizei width, GLsizei height, uint8_t *data);

auto getTime() { return std::chrono::system_clock::now(); }

#ifdef __RETRO68__
static QElem what;
#endif
int main(int argc, char **argv) {
#ifdef __RETRO68__
  InsTime(&what);
#endif
  char buf[255];
  printf("%d\n", argc);
  if (argc < 2) {
    printf("Type a file to open: \n");
    scanf("%s", &buf);
  } else {
    strncpy(buf, argv[1], 255);
  }
  pl = new player::Player(buf);

  /* start of glut windowing and control functions */
  GL_COMMAND(glutInit(&argc, argv));
  GL_COMMAND(glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH));

  if (pl->hasVideo) {
    GL_COMMAND(glutInitWindowSize(pl->realWidth, pl->realHeight));

    auto frame = pl->pRGBFrame;
    InitTexture(frame->width, frame->height, frame->data[0]);

    format = ffmpeg_pix_format_to_gl(pl->pRGBFrame->format);
  } else {
    GL_COMMAND(glutInitWindowSize(512, 384));
  }

  window = GL_COMMAND(glutCreateWindow(buf));

  GL_COMMAND(glutSetWindowTitle(buf));

  GL_COMMAND(glutIdleFunc(idle));
  GL_COMMAND(glutDisplayFunc(display));

  GL_COMMAND(glutMainLoop());

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
  // GL_COMMAND(glGenTextures(1, &textureID));
  GL_COMMAND(glBindTexture(GL_TEXTURE_2D, textureID));
  GL_COMMAND(glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGB,
                          GL_UNSIGNED_BYTE, data));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  GL_COMMAND(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
}

void idle(void) {
  auto framerate = pl->framerate();
  pl->step();
  GL_COMMAND(glutPostRedisplay());

#ifdef __RETRO68__
  PrimeTime(&what, framerate);
#endif
}
void display_hasVideo(void) {

  GL_COMMAND(glClearColor(1.0f, 0.0f, 1.0f, 1.0f));
  GL_COMMAND(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
  GL_COMMAND(glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL));
  GL_COMMAND(glPushMatrix());
  GL_COMMAND(glTranslatef(0, 0, 0));

  auto frame = pl->pRGBFrame;
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

  GL_COMMAND(glutSwapBuffers());
  GL_COMMAND(glPopMatrix());
}

void display_onlyAudio(void) {
  GL_COMMAND(glClearColor(1.0f, 1.0f, 1.0f, 1.0f));

  GL_COMMAND(glutSwapBuffers());
}

void display(void) {
  if (pl->hasVideo) {
    display_hasVideo();
  } else {
    display_onlyAudio();
  }
}

void PauseIfGLError(const char *file, int line_num, const char *code) {
  auto err = glGetError();
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

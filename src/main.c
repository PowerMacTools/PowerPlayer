#include "module/module.h"
#include <Mw/Core.h>
#include <Mw/Milsko.h>
#include <Mw/StringDefs.h>
#include <libavutil/frame.h>

#include <Mw/Widget/OpenGL.h>

#ifdef __RETRO68__
#include <Timer.h>
#endif

static MwWidget *window = NULL;
static MwWidget *gl = NULL;
static pp_module m;

void idle(MwWidget handle, void *user_data, void *call_data) {
  m.step();

  MwOpenGLSwapBuffer(gl);
}

#ifdef __RETRO68__
static QElem what;
#endif

int main(int argc, char **argv) {
  char filename[255];

  MwLibraryInit();

#ifdef __RETRO68__
  InsTime(&what);
#endif
  memset(filename, 0, 255);
  if (argc < 2) {
    printf("Type a file to open: \n");
    scanf("%s", filename);
  } else {
    strncpy(filename, argv[1], 255);
  }

  m = pp_get_module(filename);

  m.setup(filename);

  window = MwVaCreateWidget(MwWindowClass, filename, NULL, MwDEFAULT, MwDEFAULT,
                            512, 384, MwNtitle, filename, NULL);
  gl = MwCreateWidget(MwOpenGLClass, "gl", window, MwDEFAULT, MwDEFAULT, 512,
                      384);

  MwAddUserHandler(window, MwNtickHandler, idle, NULL);

  MwLoop(window);

#ifdef __RETRO68__
  printf("Program exit.\n");
  getchar();
#endif

  MwDestroyWidget(gl);
  MwDestroyWidget(window);

  return 0;
}

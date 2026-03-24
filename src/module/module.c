#include "module.h"
#include <string.h>

pp_module pp_get_module(char *buf) {
  // TODO: actually check magics maybe
  if (strstr(buf, "png") == buf + strlen(buf) - 3) {
    return pp_png_module(buf);
  };
  return pp_multimedia_module(buf);
};

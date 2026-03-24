#pragma once

typedef struct pp_module_t {
  void (*setup)(char *buf);
  void (*step)();
} pp_module;

pp_module pp_get_module(char *buf);

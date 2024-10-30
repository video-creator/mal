#pragma once
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif

void* mdp_create_parser(char *path);
void *mdp_dump_box(void *parser);
#ifdef __cplusplus
}
#endif

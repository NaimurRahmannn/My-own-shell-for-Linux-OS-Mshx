#ifndef BACKEND_H
#define BACKEND_H

#include "node.h"

char *search_path(char *file);
int do_exec_cmd(int argc, char **argv);
int do_simple_command(struct node_s *node);
int do_pipeline(struct node_s **commands, int num_commands);

#endif
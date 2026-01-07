#ifndef BACKEND_H
#define BACKEND_H

#include "node.h"

/* Execution mode enum for real vs dry-run execution */
enum exec_mode { EXEC_REAL, EXEC_DRY };

/* Current execution mode (default is EXEC_REAL) */
extern enum exec_mode current_exec_mode;

char *search_path(char *file);
int do_exec_cmd(int argc, char **argv);
int do_simple_command(struct node_s *node);
int do_pipeline(struct node_s **commands, int num_commands);
int do_pipeline_background(struct node_s **commands, int num_commands);

/* Dry-run execution functions */
int dry_run_command(const char *cmd_line);
void dry_print_exec(int argc, char **argv);
void dry_print_glob(const char *pattern, char **matches, int count);
void dry_print_redirect(const char *direction, const char *filename);
void dry_print_pipe(const char *cmd1, const char *cmd2);
void dry_print_sequence(void);
void dry_print_background(void);

#endif
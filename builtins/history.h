#ifndef HISTORY_H
#define HISTORY_H

/* Maximum number of commands to store in history */
#define HISTORY_MAX_SIZE 1000

/* Maximum length of a single command */
#define HISTORY_MAX_CMD_LEN 4096

/* Initialize the history system */
void history_init(void);

/* Add a command to history */
void history_add(const char *cmd);

/* Get command at index (1-based, like bash) */
char *history_get(int index);

/* Get the last command */
char *history_get_last(void);

/* Get total number of commands in history */
int history_count(void);

/* Clear all history */
void history_clear(void);

/* Print all history (builtin command) */
int history_builtin(int argc, char **argv);

/* Expand history references like !! and !n in command string */
/* Returns newly allocated string or NULL if no expansion needed */
char *history_expand(const char *cmd);

#endif /* HISTORY_H */

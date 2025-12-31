#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "shell.h"
#include "source.h"
#include "parser.h"
#include "executor.h"
int main(int argc, char **argv)
{
    char *command = NULL;
    initsh();
    do // Read Eval Print Loop
    {
        print_prompt1(); // Print primary prompt

        command = read_cmd(); // Read command from stdin

        if (!command)
        {
            exit(EXIT_SUCCESS);
        }

        if (command[0] == '\0' || strcmp(command, "\n") == 0)
        {
            free(command);
            continue;
        }

        if (strcmp(command, "exit\n") == 0)
        {
            free(command);
            break;
        }

        struct source_s src;
        src.buffer = command;
        src.buffer_size = strlen(command);
        src.current_pos = INIT_SRC_POS;
        parse_and_execute(&src);

        free(command);

    } while (1);

    exit(EXIT_SUCCESS);
}
char *read_cmd(void)
{
    char line_buffer[1024];      // temporary holding area for input lines
    char *command_buffer = NULL; // dynamically allocated command buffer
    size_t command_length = 0;

    while (fgets(line_buffer, sizeof(line_buffer), stdin))
    {
        size_t line_length = strlen(line_buffer);

        if (!command_buffer)
        {
            command_buffer = malloc(line_length + 1);
        }
        else
        {
            char *resized_buffer = realloc(command_buffer, command_length + line_length + 1);

            if (resized_buffer)
            {
                command_buffer = resized_buffer;
            }
            else
            {
                free(command_buffer);
                command_buffer = NULL;
            }
        }

        if (!command_buffer)
        {
            fprintf(stderr, "error: failed to alloc buffer: %s\n", strerror(errno));
            return NULL;
        }

        strcpy(command_buffer + command_length, line_buffer);

        if (line_length > 0 && line_buffer[line_length - 1] == '\n')
        {
            if (line_length == 1 || line_buffer[line_length - 2] != '\\')
            {
                return command_buffer;
            }

            command_buffer[command_length + line_length - 2] = '\0';
            line_length -= 2;
            print_prompt2();
        }

        command_length += line_length;
    }

    return command_buffer;
}
int parse_and_execute(struct source_s *src)
{
    skip_white_spaces(src);

    struct token_s *tok = tokenize(src);

    if (tok == &eof_token)
    {
        return 0;
    }

    while (tok && tok != &eof_token)
    {
        struct node_s *cmd = parse_simple_command(tok);

        if (!cmd)
        {
            break;
        }

        do_simple_command(cmd);
        free_node_tree(cmd);
        tok = tokenize(src);
    }

    return 1;
}
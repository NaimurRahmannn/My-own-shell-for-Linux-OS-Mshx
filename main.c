#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "shell.h"

int main(int argc, char **argv)
{
    char *command = NULL;

    do    //Read Eval Print Loop
    {
        print_prompt1();    // Print primary prompt

        command = read_cmd();     // Read command from stdin

        if(!command)
        {
            exit(EXIT_SUCCESS);
        }

        if(command[0] == '\0' || strcmp(command, "\n") == 0)
        {
            free(command);
            continue;
        }

        if(strcmp(command, "exit\n") == 0)
        {
            free(command);
            break;
        }

        printf("%s\n", command);

        free(command);

    } while(1);

    exit(EXIT_SUCCESS);
}
char *read_cmd(void)
{
    char line_buffer[1024];  // temporary holding area for input lines
    char *command_buffer = NULL;   // dynamically allocated command buffer
    size_t command_length = 0;

    while(fgets(line_buffer, sizeof(line_buffer), stdin))
    {
        size_t line_length = strlen(line_buffer);

        if(!command_buffer)
        {
            command_buffer = malloc(line_length + 1);
        }
        else
        {
            char *resized_buffer = realloc(command_buffer, command_length + line_length + 1);

            if(resized_buffer)
            {
                command_buffer = resized_buffer;
            }
            else
            {
                free(command_buffer);
                command_buffer = NULL;
            }
        }

        if(!command_buffer)
        {
            fprintf(stderr, "error: failed to alloc buffer: %s\n", strerror(errno));
            return NULL;
        }

        strcpy(command_buffer + command_length, line_buffer);

        if(line_length > 0 && line_buffer[line_length - 1] == '\n')
        {
            if(line_length == 1 || line_buffer[line_length - 2] != '\\')
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
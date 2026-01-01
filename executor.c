#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "shell.h"
#include "node.h"
#include "executor.h"

char *search_path(char *file)
{
    char *PATH = getenv("PATH");
    char *p = PATH;
    char *p2;

    while (p && *p)
    {
        p2 = p;

        while (*p2 && *p2 != ':')
        {
            p2++;
        }

        int plen = p2 - p;
        if (!plen)
        {
            plen = 1;
        }

        int alen = strlen(file);
        char path[plen + 1 + alen + 1];

        strncpy(path, p, p2 - p);
        path[p2 - p] = '\0';

        if (p2[-1] != '/')
        {
            strcat(path, "/");
        }

        strcat(path, file);

        struct stat st;
        if (stat(path, &st) == 0)
        {
            if (!S_ISREG(st.st_mode))
            {
                errno = ENOENT;
                p = p2;
                if (*p2 == ':')
                {
                    p++;
                }
                continue;
            }

            p = malloc(strlen(path) + 1);
            if (!p)
            {
                return NULL;
            }

            strcpy(p, path);
            return p;
        }
        else /* file not found */
        {
            p = p2;
            if (*p2 == ':')
            {
                p++;
            }
        }
    }

    errno = ENOENT;
    return NULL;
}

int do_exec_cmd(int argc, char **argv)
{
    if (strchr(argv[0], '/'))
    {
        execv(argv[0], argv);
    }
    else
    {
        char *path = search_path(argv[0]);
        if (!path)
        {
            return 0;
        }
        execv(path, argv);
        free(path);
    }
    return 0;
}

static inline void free_argv(int argc, char **argv)
{
    if (!argc)
    {
        return;
    }

    while (argc--)
    {
        free(argv[argc]);
    }
}

int do_simple_command(struct node_s *node)
{
    if (!node)
    {
        return 0;
    }

    struct node_s *child = node->first_child;
    if (!child)
    {
        return 0;
    }

     int argc = 0;
    int targc = 0;
    char **argv = NULL;
    char *str;

    while(child)
    {
        str = child->val.str;
        struct word_s *w = word_expand(str);
        
        if(!w)
        {
            child = child->next_sibling;
            continue;
        }

        struct word_s *w2 = w;
        while(w2)
        {
            if(check_buffer_bounds(&argc, &targc, &argv))
            {
                str = malloc(strlen(w2->data)+1);
                if(str)
                {
                    strcpy(str, w2->data);
                    argv[argc++] = str;
                }
            }
            w2 = w2->next;
        }
        
        free_all_words(w);
        
        child = child->next_sibling;
    }

    if(check_buffer_bounds(&argc, &targc, &argv))
    {
        argv[argc] = NULL;
    }
    int i = 0;
    for (; i < builtins_count; i++)   //check if the typing command is in the builtin if yes then call the function
    {
        if (strcmp(argv[0], builtins[i].name) == 0)
        {
            builtins[i].func(argc, argv);
            free_argv(argc, argv);
            return 1;
        }
    }
    pid_t child_pid = 0;
    if ((child_pid = fork()) == 0)  
    {
        do_exec_cmd(argc, argv);
        fprintf(stderr, "error: failed to execute command: %s\n", strerror(errno));
        if (errno == ENOEXEC)
        {
            exit(126); // Command invoked cannot execute
        }
        else if (errno == ENOENT)
        {
            exit(127); // Command not found
        }
        else
        {
            exit(EXIT_FAILURE);
        }
    }
    else if (child_pid < 0)
    {
        fprintf(stderr, "error: failed to fork command: %s\n", strerror(errno));
        return 0;
    }

    int status = 0;
    waitpid(child_pid, &status, 0);
    free_argv(argc, argv);

    return 1;
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glob.h>
#include "mshX.h"
#include "node.h"
#include "executor.h"

/* extern declaration for exit status (defined in wordexp.c) */
extern int exit_status;

/* Current execution mode - defaults to real execution */
enum exec_mode current_exec_mode = EXEC_REAL;

/* Redirection types */
#define REDIRECT_INPUT    0   /* < */
#define REDIRECT_OUTPUT   1   /* > */
#define REDIRECT_APPEND   2   /* >> */

/* Redirection structure */
struct redirect_s
{
    int type;                   /* type of redirection */
    char *filename;             /* target filename */
    struct redirect_s *next;    /* next redirection in list */
};

// Forward declarations
struct word_s *word_expand(char *str);
void free_all_words(struct word_s *words);
int check_buffer_bounds(int *argc, int *targc, char ***argv);
int has_glob_chars(char *str, size_t len);
char **get_filename_matches(char *pattern, glob_t *matches);

/* Free redirection list */
static void free_redirects(struct redirect_s *redirects)
{
    while(redirects)
    {
        struct redirect_s *next = redirects->next;
        if(redirects->filename)
        {
            free(redirects->filename);
        }
        free(redirects);
        redirects = next;
    }
}

/*
 * Dry-run print functions
 */

/* Print EXEC line with full path */
void dry_print_exec(int argc, char **argv)
{
    if(argc <= 0 || !argv || !argv[0])
    {
        return;
    }
    
    char *path;
    if(strchr(argv[0], '/'))
    {
        path = argv[0];
        printf("EXEC: %s", path);
    }
    else
    {
        path = search_path(argv[0]);
        if(path)
        {
            printf("EXEC: %s", path);
            free(path);
        }
        else
        {
            printf("EXEC: %s", argv[0]);
        }
    }
    
    for(int i = 1; i < argc; i++)
    {
        printf(" %s", argv[i]);
    }
    printf("\n");
}

/* Print GLOB expansion */
void dry_print_glob(const char *pattern, char **matches, int count)
{
    printf("GLOB: %s ->", pattern);
    for(int i = 0; i < count; i++)
    {
        printf(" %s", matches[i]);
    }
    printf("\n");
}

/* Print REDIRECT */
void dry_print_redirect(const char *direction, const char *filename)
{
    printf("REDIRECT: %s -> %s\n", direction, filename);
}

/* Print PIPE */
void dry_print_pipe(const char *cmd1, const char *cmd2)
{
    printf("PIPE: %s -> %s\n", cmd1, cmd2);
}

/* Print SEQUENCE */
void dry_print_sequence(void)
{
    printf("SEQUENCE:\n");
}

/* Print BACKGROUND */
void dry_print_background(void)
{
    printf("BACKGROUND:\n");
}

/* Apply redirections - returns 0 on success, -1 on error */
static int apply_redirects(struct redirect_s *redirects)
{
    while(redirects)
    {
        int fd;
        switch(redirects->type)
        {
            case REDIRECT_INPUT:
                fd = open(redirects->filename, O_RDONLY);
                if(fd < 0)
                {
                    fprintf(stderr, "error: cannot open %s: %s\n", 
                            redirects->filename, strerror(errno));
                    return -1;
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
                break;
                
            case REDIRECT_OUTPUT:
                fd = open(redirects->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(fd < 0)
                {
                    fprintf(stderr, "error: cannot open %s: %s\n", 
                            redirects->filename, strerror(errno));
                    return -1;
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                break;
                
            case REDIRECT_APPEND:
                fd = open(redirects->filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if(fd < 0)
                {
                    fprintf(stderr, "error: cannot open %s: %s\n", 
                            redirects->filename, strerror(errno));
                    return -1;
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                break;
        }
        redirects = redirects->next;
    }
    return 0;
}

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

int do_exec_cmd(int argc __attribute__((unused)), char **argv)
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
    if (!argc || !argv)
    {
        return;
    }

    while (argc--)
    {
        free(argv[argc]);
    }
    
    free(argv);
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
    struct redirect_s *redirects = NULL;
    struct redirect_s *last_redirect = NULL;
    int expect_filename = 0;
    int redirect_type = 0;

    /* For dry-run mode: track glob patterns */
    char *glob_patterns[64];
    char *glob_expansions[64];
    int glob_count = 0;

    while(child)
    {
        str = child->val.str;
        
        /* Check for redirection operators */
        if(strcmp(str, ">") == 0)
        {
            expect_filename = 1;
            redirect_type = REDIRECT_OUTPUT;
            child = child->next_sibling;
            continue;
        }
        else if(strcmp(str, ">>") == 0)
        {
            expect_filename = 1;
            redirect_type = REDIRECT_APPEND;
            child = child->next_sibling;
            continue;
        }
        else if(strcmp(str, "<") == 0)
        {
            expect_filename = 1;
            redirect_type = REDIRECT_INPUT;
            child = child->next_sibling;
            continue;
        }
        
        /* If we're expecting a filename for redirection */
        if(expect_filename)
        {
            struct word_s *w = word_expand(str);
            if(w && w->data)
            {
                struct redirect_s *redir = malloc(sizeof(struct redirect_s));
                if(redir)
                {
                    redir->type = redirect_type;
                    redir->filename = malloc(strlen(w->data) + 1);
                    if(redir->filename)
                    {
                        strcpy(redir->filename, w->data);
                    }
                    redir->next = NULL;
                    
                    if(last_redirect)
                    {
                        last_redirect->next = redir;
                    }
                    else
                    {
                        redirects = redir;
                    }
                    last_redirect = redir;
                }
                free_all_words(w);
            }
            expect_filename = 0;
            child = child->next_sibling;
            continue;
        }
        
        /* For dry-run mode: check for glob patterns before expansion */
        int is_glob = 0;
        char *pattern_copy = NULL;
        if(current_exec_mode == EXEC_DRY && has_glob_chars(str, strlen(str)))
        {
            is_glob = 1;
            pattern_copy = strdup(str);
        }
        
        struct word_s *w = word_expand(str);
        
        if(!w)
        {
            if(pattern_copy) free(pattern_copy);
            child = child->next_sibling;
            continue;
        }

        /* For dry-run mode: track glob expansion */
        if(current_exec_mode == EXEC_DRY && is_glob && glob_count < 64)
        {
            /* Count expanded words */
            int word_count = 0;
            struct word_s *wc = w;
            size_t total_len = 0;
            while(wc)
            {
                word_count++;
                total_len += strlen(wc->data) + 1;
                wc = wc->next;
            }
            
            /* Build expansion string */
            char *expansion = malloc(total_len + 1);
            if(expansion)
            {
                expansion[0] = '\0';
                wc = w;
                while(wc)
                {
                    if(expansion[0]) strcat(expansion, " ");
                    strcat(expansion, wc->data);
                    wc = wc->next;
                }
                glob_patterns[glob_count] = pattern_copy;
                glob_expansions[glob_count] = expansion;
                glob_count++;
                pattern_copy = NULL; /* Don't free, it's stored */
            }
        }
        if(pattern_copy) free(pattern_copy);

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
    
    // Check if we have any arguments after expansion
    if(argc == 0 || !argv || !argv[0])
    {
        if(argv)
        {
            free_argv(argc, argv);
        }
        free_redirects(redirects);
        /* Free glob tracking data */
        for(int g = 0; g < glob_count; g++)
        {
            free(glob_patterns[g]);
            free(glob_expansions[g]);
        }
        return 0;
    }
    
    /* DRY-RUN MODE: Just print what would happen */
    if(current_exec_mode == EXEC_DRY)
    {
        /* Print glob expansions first */
        for(int g = 0; g < glob_count; g++)
        {
            printf("GLOB: %s -> %s\n", glob_patterns[g], glob_expansions[g]);
            free(glob_patterns[g]);
            free(glob_expansions[g]);
        }
        
        /* Print EXEC line */
        dry_print_exec(argc, argv);
        
        /* Print redirections */
        struct redirect_s *r = redirects;
        while(r)
        {
            const char *direction;
            switch(r->type)
            {
                case REDIRECT_INPUT:
                    direction = "stdin";
                    break;
                case REDIRECT_OUTPUT:
                case REDIRECT_APPEND:
                    direction = "stdout";
                    break;
                default:
                    direction = "unknown";
                    break;
            }
            dry_print_redirect(direction, r->filename);
            r = r->next;
        }
        
        free_argv(argc, argv);
        free_redirects(redirects);
        return 1;
    }
    
    /* REAL EXECUTION MODE */
    int i = 0;
    for (; i < builtins_count; i++)   //check if the typing command is in the builtin if yes then call the function
    {
        if (strcmp(argv[0], builtins[i].name) == 0)
        {
            /* For builtins with redirection, we need to handle it specially */
            if(redirects)
            {
                /* Save original file descriptors */
                int saved_stdin = dup(STDIN_FILENO);
                int saved_stdout = dup(STDOUT_FILENO);
                
                if(apply_redirects(redirects) == 0)
                {
                    exit_status = builtins[i].func(argc, argv);
                }
                else
                {
                    exit_status = 1;
                }
                
                /* Restore original file descriptors */
                dup2(saved_stdin, STDIN_FILENO);
                dup2(saved_stdout, STDOUT_FILENO);
                close(saved_stdin);
                close(saved_stdout);
            }
            else
            {
                exit_status = builtins[i].func(argc, argv);
            }
            free_argv(argc, argv);
            free_redirects(redirects);
            return 1;
        }
    }
    pid_t child_pid = 0;
    if ((child_pid = fork()) == 0)  
    {
        /* Apply redirections in child process */
        if(redirects && apply_redirects(redirects) < 0)
        {
            exit(EXIT_FAILURE);
        }
        
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
        free_argv(argc, argv);
        free_redirects(redirects);
        return 0;
    }

    int status = 0;
    waitpid(child_pid, &status, 0);
    
    /* Set the exit status for $? */
    if(WIFEXITED(status))
    {
        exit_status = WEXITSTATUS(status);
    }
    else if(WIFSIGNALED(status))
    {
        exit_status = 128 + WTERMSIG(status);
    }
    
    free_argv(argc, argv);
    free_redirects(redirects);

    return 1;
}

/*
 * Helper function to get the first command word from a node (for dry-run PIPE output)
 */
static char *get_first_command_word(struct node_s *node)
{
    if(!node || !node->first_child)
    {
        return NULL;
    }
    
    struct node_s *child = node->first_child;
    while(child)
    {
        char *str = child->val.str;
        /* Skip redirection operators and their targets */
        if(strcmp(str, ">") == 0 || strcmp(str, ">>") == 0 || strcmp(str, "<") == 0)
        {
            child = child->next_sibling;
            if(child) child = child->next_sibling;
            continue;
        }
        return str;
    }
    return NULL;
}

/*
 * Execute a pipeline of commands.
 * commands: array of node_s pointers (each is a simple command)
 * num_commands: number of commands in the pipeline
 */
int do_pipeline(struct node_s **commands, int num_commands)
{
    if(num_commands == 0 || !commands)
    {
        return 0;
    }
    
    /* Single command - no pipe needed */
    if(num_commands == 1)
    {
        return do_simple_command(commands[0]);
    }
    
    /* DRY-RUN MODE: Print pipe information without actually piping */
    if(current_exec_mode == EXEC_DRY)
    {
        /* Print PIPE information for each pair */
        for(int i = 0; i < num_commands - 1; i++)
        {
            char *cmd1 = get_first_command_word(commands[i]);
            char *cmd2 = get_first_command_word(commands[i + 1]);
            if(cmd1 && cmd2)
            {
                dry_print_pipe(cmd1, cmd2);
            }
        }
        
        /* Execute each command in dry-run mode (which just prints EXEC) */
        for(int i = 0; i < num_commands; i++)
        {
            do_simple_command(commands[i]);
        }
        
        return 1;
    }
    
    /* REAL EXECUTION MODE */
    int pipefds[2 * (num_commands - 1)];
    pid_t pids[num_commands];
    
    /* Create all pipes */
    for(int i = 0; i < num_commands - 1; i++)
    {
        if(pipe(pipefds + i * 2) < 0)
        {
            fprintf(stderr, "error: failed to create pipe: %s\n", strerror(errno));
            return 0;
        }
    }
    
    /* Fork and execute each command */
    for(int i = 0; i < num_commands; i++)
    {
        pids[i] = fork();
        
        if(pids[i] == 0)
        {
            /* Child process */
            
            /* Set up input from previous pipe (if not first command) */
            if(i > 0)
            {
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }
            
            /* Set up output to next pipe (if not last command) */
            if(i < num_commands - 1)
            {
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }
            
            /* Close all pipe fds in child */
            for(int j = 0; j < 2 * (num_commands - 1); j++)
            {
                close(pipefds[j]);
            }
            
            /* Execute the command - need to build argv from node */
            struct node_s *node = commands[i];
            struct node_s *child = node->first_child;
            
            int argc = 0;
            int targc = 0;
            char **argv = NULL;
            char *str;
            
            while(child)
            {
                str = child->val.str;
                
                /* Skip redirection operators and their targets for pipe commands */
                if(strcmp(str, ">") == 0 || strcmp(str, ">>") == 0 || strcmp(str, "<") == 0)
                {
                    child = child->next_sibling;
                    if(child) child = child->next_sibling; /* skip filename too */
                    continue;
                }
                
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
            
            if(argc > 0 && argv)
            {
                if(check_buffer_bounds(&argc, &targc, &argv))
                {
                    argv[argc] = NULL;
                }
                do_exec_cmd(argc, argv);
                fprintf(stderr, "error: failed to execute command: %s\n", strerror(errno));
            }
            exit(EXIT_FAILURE);
        }
        else if(pids[i] < 0)
        {
            fprintf(stderr, "error: failed to fork: %s\n", strerror(errno));
            /* Close pipes and return */
            for(int j = 0; j < 2 * (num_commands - 1); j++)
            {
                close(pipefds[j]);
            }
            return 0;
        }
    }
    
    /* Parent: close all pipe fds */
    for(int j = 0; j < 2 * (num_commands - 1); j++)
    {
        close(pipefds[j]);
    }
    
    /* Wait for all children */
    int status;
    for(int i = 0; i < num_commands; i++)
    {
        waitpid(pids[i], &status, 0);
    }
    
    /* Set exit status from last command in pipeline */
    if(WIFEXITED(status))
    {
        exit_status = WEXITSTATUS(status);
    }
    else if(WIFSIGNALED(status))
    {
        exit_status = 128 + WTERMSIG(status);
    }
    
    return 1;
}

/*
 * Execute a pipeline in the background.
 * Similar to do_pipeline but doesn't wait for children.
 */
int do_pipeline_background(struct node_s **commands, int num_commands)
{
    if(num_commands == 0 || !commands)
    {
        return 0;
    }
    
    /* DRY-RUN MODE: Print background information without actually forking */
    if(current_exec_mode == EXEC_DRY)
    {
        dry_print_background();
        
        /* Print pipe information if multiple commands */
        if(num_commands > 1)
        {
            for(int i = 0; i < num_commands - 1; i++)
            {
                char *cmd1 = get_first_command_word(commands[i]);
                char *cmd2 = get_first_command_word(commands[i + 1]);
                if(cmd1 && cmd2)
                {
                    dry_print_pipe(cmd1, cmd2);
                }
            }
        }
        
        /* Execute each command in dry-run mode (which just prints EXEC) */
        for(int i = 0; i < num_commands; i++)
        {
            do_simple_command(commands[i]);
        }
        
        return 1;
    }
    
    /* REAL EXECUTION MODE */
    /* Fork a subshell to handle the entire pipeline in background */
    pid_t bg_pid = fork();
    
    if(bg_pid == 0)
    {
        /* Child: this becomes the background job leader */
        /* Detach from terminal's process group */
        setpgid(0, 0);
        
        if(num_commands == 1)
        {
            /* Single command - execute directly */
            struct node_s *node = commands[0];
            struct node_s *child = node->first_child;
            
            int argc = 0;
            int targc = 0;
            char **argv = NULL;
            char *str;
            
            while(child)
            {
                str = child->val.str;
                
                if(strcmp(str, ">") == 0 || strcmp(str, ">>") == 0 || strcmp(str, "<") == 0)
                {
                    child = child->next_sibling;
                    if(child) child = child->next_sibling;
                    continue;
                }
                
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
            
            if(argc > 0 && argv)
            {
                if(check_buffer_bounds(&argc, &targc, &argv))
                {
                    argv[argc] = NULL;
                }
                do_exec_cmd(argc, argv);
            }
            exit(EXIT_FAILURE);
        }
        
        /* Multiple commands - set up pipeline */
        int pipefds[2 * (num_commands - 1)];
        pid_t pids[num_commands];
        
        for(int i = 0; i < num_commands - 1; i++)
        {
            if(pipe(pipefds + i * 2) < 0)
            {
                exit(EXIT_FAILURE);
            }
        }
        
        for(int i = 0; i < num_commands; i++)
        {
            pids[i] = fork();
            
            if(pids[i] == 0)
            {
                if(i > 0)
                {
                    dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
                }
                
                if(i < num_commands - 1)
                {
                    dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
                }
                
                for(int j = 0; j < 2 * (num_commands - 1); j++)
                {
                    close(pipefds[j]);
                }
                
                struct node_s *node = commands[i];
                struct node_s *child = node->first_child;
                
                int argc = 0;
                int targc = 0;
                char **argv = NULL;
                char *str;
                
                while(child)
                {
                    str = child->val.str;
                    
                    if(strcmp(str, ">") == 0 || strcmp(str, ">>") == 0 || strcmp(str, "<") == 0)
                    {
                        child = child->next_sibling;
                        if(child) child = child->next_sibling;
                        continue;
                    }
                    
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
                
                if(argc > 0 && argv)
                {
                    if(check_buffer_bounds(&argc, &targc, &argv))
                    {
                        argv[argc] = NULL;
                    }
                    do_exec_cmd(argc, argv);
                }
                exit(EXIT_FAILURE);
            }
            else if(pids[i] < 0)
            {
                exit(EXIT_FAILURE);
            }
        }
        
        for(int j = 0; j < 2 * (num_commands - 1); j++)
        {
            close(pipefds[j]);
        }
        
        int status;
        for(int i = 0; i < num_commands; i++)
        {
            waitpid(pids[i], &status, 0);
        }
        
        if(WIFEXITED(status))
        {
            exit(WEXITSTATUS(status));
        }
        exit(EXIT_FAILURE);
    }
    else if(bg_pid < 0)
    {
        fprintf(stderr, "error: failed to fork background job: %s\n", strerror(errno));
        return 0;
    }
    
    /* Parent: print background job info and continue */
    printf("[%d] %d\n", 1, bg_pid);
    exit_status = 0;
    
    return 1;
}
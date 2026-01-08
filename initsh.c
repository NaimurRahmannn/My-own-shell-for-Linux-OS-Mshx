#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include "mshX.h"
#include "symtab/symtab.h"
#include "builtins/history.h"

extern char **environ;

/* extern declaration for shell_pid (defined in wordexp.c) */
extern pid_t shell_pid;

/* Signal handler for SIGINT (Ctrl+C) - does nothing, just prevents shell termination */
static void sigint_handler(int sig)
{
    (void)sig;
    /* Write a newline so the prompt appears on a fresh line */
    write(STDERR_FILENO, "\n", 1);
}

/* Signal handler for SIGTSTP (Ctrl+Z) - does nothing, shell ignores it */
static void sigtstp_handler(int sig)
{
    (void)sig;
    /* Shell ignores SIGTSTP */
}

/* Setup signal handling for the shell */
void setup_signals(void)
{
    struct sigaction sa;
    
    /* Setup SIGINT handler */
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  /* Restart interrupted system calls */
    sigaction(SIGINT, &sa, NULL);
    
    /* Setup SIGTSTP handler */
    sa.sa_handler = sigtstp_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa, NULL);
    
    /* Ignore SIGTTOU (background write to terminal) */
    signal(SIGTTOU, SIG_IGN);
}

void initsh(void)
{
    init_symtab();
    
    /* Initialize history system */
    history_init();
    
    /* Setup signal handlers - shell ignores SIGINT and SIGTSTP */
    setup_signals();
    
    /* Initialize shell PID for $$ */
    shell_pid = getpid();

    struct symtab_entry_s *entry;
    char **p2 = environ;

    while(*p2)
    {
        char *eq = strchr(*p2, '=');
        if(eq)
        {
            int len = eq-(*p2);
            char name[len+1];

            strncpy(name, *p2, len);
            name[len] = '\0';
            entry = add_to_symtab(name);

            if(entry)
            {
                symtab_entry_setval(entry, eq+1);
                entry->flags |= FLAG_EXPORT;
            }
        }
        else
        {
            entry = add_to_symtab(*p2);
        }
        p2++;
    }

    entry = add_to_symtab("PS1");
    symtab_entry_setval(entry, "$ ");

    entry = add_to_symtab("PS2");
    symtab_entry_setval(entry, "> ");
}
#ifndef TIMELINE_H
#define TIMELINE_H

#include <sys/types.h>
#include <time.h>

/* Maximum number of timeline events we can track */
#define MAX_TIMELINE_EVENTS 256

/* Event types for command execution timeline */
typedef enum {
    EVENT_FORKED,
    EVENT_EXECVE,
    EVENT_EXITED,
    EVENT_SIGNALED,
    EVENT_STOPPED,
    EVENT_CONTINUED,
    EVENT_PIPED,
    EVENT_REDIRECTED
} timeline_event_type_t;

/* Structure for a single timeline event */
typedef struct timeline_event_s {
    pid_t pid;                      /* Process ID or job identifier */
    timeline_event_type_t type;     /* Event type */
    double time_ms;                 /* Time in milliseconds relative to command start */
    
    /* Additional data for specific event types */
    union {
        struct {
            pid_t from_pid;         /* For PIPED: source PID */
            pid_t to_pid;           /* For PIPED: destination PID */
        } pipe_info;
        struct {
            int exit_code;          /* For EXITED: exit status */
        } exit_info;
        struct {
            int signal;             /* For SIGNALED/STOPPED: signal number */
        } signal_info;
        struct {
            char *target;           /* For REDIRECTED: target file/fd */
            int direction;          /* 0=input, 1=output, 2=append */
        } redirect_info;
    } data;
} timeline_event_t;

/* Timeline context structure */
typedef struct timeline_s {
    struct timespec start_time;     /* Command start time */
    timeline_event_t events[MAX_TIMELINE_EVENTS];
    int event_count;
    int enabled;                    /* Whether timeline tracking is enabled */
} timeline_t;

/* Global timeline context */
extern timeline_t g_timeline;

/* Initialize timeline for a new command */
void timeline_init(void);

/* Reset/clear the timeline */
void timeline_reset(void);

/* Enable/disable timeline tracking */
void timeline_enable(int enable);

/* Check if timeline is enabled */
int timeline_is_enabled(void);

/* Record events */
void timeline_record_fork(pid_t pid);
void timeline_record_execve(pid_t pid);
void timeline_record_exit(pid_t pid, int exit_code);
void timeline_record_signaled(pid_t pid, int signal);
void timeline_record_stopped(pid_t pid, int signal);
void timeline_record_continued(pid_t pid);
void timeline_record_pipe(pid_t from_pid, pid_t to_pid);
void timeline_record_redirect(pid_t pid, const char *target, int direction);

/* Print the timeline in formatted output */
void timeline_print(void);

/* Get event type as string */
const char *timeline_event_type_str(timeline_event_type_t type);

#endif /* TIMELINE_H */

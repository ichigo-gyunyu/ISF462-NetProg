/**
 * Stores information and functions to control and update
 * processes and its' arguements.
 */
#ifndef PROCESS_H
#define PROCESS_H

#include "utils.h"

// process statuses
typedef enum PROC_STATUS {
    PS_RUNNING_FG,
    PS_RUNNING_BG,
    PS_STOPPED,
    PS_COMPLETED,

} PROC_STATUS;

/**
 * Data structure holding the necessary information
 * to execute a process.
 */
typedef struct process {
    pid_t       pid;
    pid_t       pgid;
    PROC_STATUS status;
    bool        is_bg;
    int         argc;
    int         stdin_fd;
    int         stdout_fd;
    char       *file;
    char      **argv;
} process;

#define PROCESS_NOT_FOUND 127

/**
 * Initialize the process data structure.
 */
process *proc_make(char *file);

/**
 * Add an arguement to a process.
 */
int proc_addarg(process *pr, char *arg);

/**
 * Print information about a process.
 */
void proc_info(const process *pr);

/**
 * Execute a process, remapping stdin and stdout where necessary.
 * If not a background process, give it control of the terminal.
 */
int proc_launch(process *pr);

/**
 * Wait for a foreground process to terminate
 */
PROC_STATUS proc_waitfor(process *pr);

/**
 * Synchronously check the status of a background process
 */
PROC_STATUS proc_update_status(process *pr);

/**
 * Launch a process as a daemon
 */
int proc_dmn(process *pr);

#endif

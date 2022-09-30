/**
 * A simple bash-like shell
 *
 * Able to launch any program with arguements from PATH,
 * can place it in the background if `&` is provided
 *
 * Has full job control support - can stop and start jobs,
 * place them in the background or foreground as required
 * using shell builtins like `jobs`, `fg` and `bg`
 *
 * Supports pipelining of any number of processes
 * proc1 | proc2 | proc3 ...
 *
 * Supports one to many process communication using message queues
 * proc1 ## proc2 , proc3 , proc4 ...
 * (proc1's output is sent as input to proc2, proc3 and proc4)
 *
 * Supports one to many process communication using shared memory
 * proc1 SS proc2 , proc3 , proc4 ...
 * (proc1's output is sent as input to proc2, proc3 and proc4)
 *
 * Supports daemonization of processes with the shell builtin command
 * `daemonize`
 *
 * Supports 3 redirection operators `>` `>>` and `<` with single process
 * jobs as well as pipelined jobs
 *
 * Other shell builtins - `clear` and `exit`
 */

#ifndef SHELL_H
#define SHELL_H

#include "job.h"
#include "parser.h"
#include "process.h"
#include "utils.h"

// for colourful output
#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define WHT "\x1B[37m"
#define RST "\x1B[0m"

// for formatting output
#define NORM "\033[0m"
#define BOLD "\033[1m"

#define PROMPT MAG BOLD "shell> " NORM RST

// shell builtin commands
#define CMD_EXIT   "exit"
#define CMD_CLEAR  "clear"
#define CMD_JOBS   "jobs"
#define CMD_FG     "fg"
#define CMD_BG     "bg"
#define CMD_DMN    "daemonize"
#define BACKGROUND "&"

// pipeline symbols
#define SYMBOL_PIPE "|"
#define SYMBOL_MSGQ "##"
#define SYMBOL_SHDM "SS"

// redirection symbols
#define SYMBOL_REDIR_IN  "<"
#define SYMBOL_REDIR_OUT ">"
#define SYMBOL_REDIR_APP ">>"

// state variables of shell
static const int      jobcontrol_signals[] = {SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU};
extern int            num_jobcontrol_signals;
extern int            cterminal_fd;
extern struct termios cterminal_attr;

/**
 * Add a job to the list of currently running/stopped
 * background jobs.
 */
void shell_add_job(job *j);

/**
 * Remove a job from the list of currently running/stopped
 * background jobs.
 */
void shell_remove_job(job *j);

/**
 * List the currently running/stopped
 * background jobs
 */
void shell_list_jobs();

/**
 * Find a background job from it's pgid
 */
job *shell_get_job(pid_t pgid);

/**
 * Perform cleanup operations and exit the program
 */
void shell_exit(bool newline);

#endif

/**
 * A job consists of many related process, all having the
 * same process group id.
 *
 * These processes communicate with each other using a pipe,
 * a message queue or shared memory.
 */

#ifndef JOB_H
#define JOB_H

#include "process.h"
#include "utils.h"

// pipeline mediums
typedef enum PIPELINE_MEDIUM {
    PM_NONE, // single process
    PM_PIPE, // proc1 | proc 2
    PM_MSGQ, // proc1 ## proc2 , proc3
    PM_SHDM, // proc1 SS proc2 , proc3

} PIPELINE_MEDIUM;

// job statuses
typedef enum JOB_STATUS {
    JS_RUNNING_FG,
    JS_RUNNING_BG,
    JS_STOPPED,
    JS_COMPLETED,

} JOB_STATUS;

typedef struct job {
    char           *cmd;             // the command entered by user
    char           *_cmd;            // same command, but destroyed by strtok_r
    pid_t           pgid;            // all processes have a single pgid
    bool            is_bg;           // background or foreground
    JOB_STATUS      status;          // running/stopped/completed
    PIPELINE_MEDIUM pipeline_medium; // if it is | or ## or SS
    int             num_processes;   // in the array of processes in the job
    process       **processes;       // array of all the processes in the job
    char           *redir_in;        // filename to remap stdin to (if any)
    char           *redir_out;       // filename to remap stdout to (if any)
    bool            redir_app;       // append or not to the remapped stdout file
} job;

extern const char *job_status_string[]; // for printing the JOB_STATUS

/**
 * Initialize a job
 */
job *job_make(char *cmd, char *_cmd);

/**
 * Add a process to the job
 */
int job_addprocess(job *j, process *p);

/**
 * Mark the job as a background job
 */
int job_makebg(job *j);

/**
 * For foreground jobs, the shell waits for all processes
 * in the job to terminate in order to regain terminal control
 */
JOB_STATUS job_waitfor(job *j);

/**
 * Perform necessary checks, remap stdin and stdout
 * if required and start the job
 */
JOB_STATUS job_start(job *j);

/**
 * Synchronous method of updating the status of background jobs
 */
JOB_STATUS job_update_status(job *j);

/**
 * Resume (into the foreground or background) a stopped job
 */
JOB_STATUS job_resume(job *j, bool bg);

/**
 * Free up any allocated memory
 */
void job_cleanup(job *j);

/**
 * Print details about a job
 */
void job_info(const job *j);
void job_info_lite(const job *j);

#endif

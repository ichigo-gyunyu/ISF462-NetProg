#include "job.h"
#include "shell.h"

#define PERM_BITS S_IRUSR | S_IWUSR

// for printing
const char *job_status_string[] = {
    [JS_RUNNING_FG] = "Running (fg)", // unused
    [JS_RUNNING_BG] = "Running",
    [JS_STOPPED]    = "Stopped",
    [JS_COMPLETED]  = "Completed",

};

// data structure for the message queue
struct msgbuf {
    long mtype;
    char mtext[BUFLEN];
};

static int shdm_num = 0; // for naming the shared memory object

int job_start_pipe(job *j);
int job_start_msgq(job *j);
int job_start_shdm(job *j);

job *job_make(char *cmd, char *_cmd) {
    job *j = malloc(sizeof *j);

    *j = (job){
        .pgid            = -1,
        .cmd             = strdup(cmd),
        ._cmd            = _cmd,
        .num_processes   = 0,
        .pipeline_medium = PM_NONE,
        .is_bg           = false,
        .processes       = malloc(sizeof(process *)),
        .redir_in        = NULL,
        .redir_out       = NULL,
        .redir_app       = false,
    };

    return j;
}

int job_addprocess(job *j, process *p) {
    j->num_processes++;
    // optimizable
    if ((j->processes = realloc(j->processes, j->num_processes * sizeof(process *))) == NULL)
        perror_and_exit("no memory");
    j->processes[j->num_processes - 1] = p;

    return 0;
}

int job_makebg(job *j) {
    j->is_bg = true;

    for (int i = 0; i < j->num_processes; i++) {
        j->processes[i]->is_bg = true;
    }

    return 0;
}

JOB_STATUS job_start(job *j) {
    switch (j->pipeline_medium) {
    case PM_NONE: {

        // remap fd if redirection operator
        if (j->redir_in != NULL) {
            int fd_in = open(j->redir_in, O_RDONLY);
            if (fd_in == -1) {
                perror("opening input file");
                job_cleanup(j);
                return -1;
            }
            j->processes[0]->stdin_fd = fd_in;
        }

        // remap fd if redirection operator
        if (j->redir_out != NULL) {
            int oflags = O_CREAT | O_WRONLY;
            if (j->redir_out)
                oflags |= O_APPEND;
            int fd_out = open(j->redir_out, oflags, PERM_BITS | S_IRGRP | S_IROTH);
            if (fd_out == -1) {
                perror("opening output file");
                job_cleanup(j);
                return -1;
            }
            j->processes[0]->stdout_fd = fd_out;
        }

        // launch process
        proc_launch(j->processes[0]);

        // update pgid
        j->pgid = j->processes[0]->pgid;

        // close fd, if not stdin
        if (j->processes[0]->stdin_fd != STDIN_FILENO)
            if (close(j->processes[0]->stdin_fd) == -1)
                perror("closing input file");

        // close fd, if not stdout
        if (j->processes[0]->stdout_fd != STDOUT_FILENO)
            if (close(j->processes[0]->stdout_fd) == -1)
                perror("closing output file");

        break;
    }

    case PM_PIPE:
        if (job_start_pipe(j) == -1)
            return -1;
        break;

    case PM_MSGQ:
        if (job_start_msgq(j) == -1)
            return -1;
        break;

    case PM_SHDM:
        if (job_start_shdm(j) == -1)
            return -1;
        break;
    }

    // update job and processes statuses
    JOB_STATUS  js;
    PROC_STATUS ps;
    if (j->is_bg)
        js = JS_RUNNING_BG, ps = PS_RUNNING_BG;
    else
        js = JS_RUNNING_FG, ps = PS_RUNNING_FG;

    j->status = js;
    for (int i = 0; i < j->num_processes; i++) {
        j->processes[i]->status = ps;
    }

    return j->status;
}
JOB_STATUS job_waitfor(job *j) {
    PROC_STATUS ps;
    bool        all_completed = true;
    for (int i = 0; i < j->num_processes; i++) {
        ps = proc_waitfor(j->processes[i]);
        if (ps != PS_COMPLETED)
            all_completed = false; // process was stopped
    }

    return j->status = (all_completed) ? JS_COMPLETED : JS_STOPPED;
}

JOB_STATUS job_update_status(job *j) {

    PROC_STATUS ps;
    bool        all_completed = true;
    for (int i = 0; i < j->num_processes; i++) {
        ps = proc_update_status(j->processes[i]);
        if (ps != PS_COMPLETED)
            all_completed = false;
    }

    if (all_completed) {
        j->status = JS_COMPLETED;
        job_info_lite(j);
    } else if (ps == PS_STOPPED) {
        j->status = JS_STOPPED;
    }

    return j->status;
}

JOB_STATUS job_resume(job *j, bool bg) {

    // update the job and process data structures
    for (int i = 0; i < j->num_processes; i++) {
        j->processes[i]->is_bg  = bg;
        j->processes[i]->status = (bg) ? PS_RUNNING_BG : PS_RUNNING_FG;
    }
    j->is_bg  = bg;
    j->status = (bg) ? JS_RUNNING_BG : JS_RUNNING_FG;

    // if foreground
    if (!bg) {

        // remove this job from the list of background jobs
        shell_remove_job(j);

        // indicate we have resumed
        job_info_lite(j);

        // give it terminal access
        if (tcsetpgrp(cterminal_fd, j->pgid) == -1) {
            perror("tcsetpgrp error");
            return -1;
        }
    }

    // send continue signal
    if (killpg(j->pgid, SIGCONT) == -1) {
        perror("killpg error");
        return -1;
    }

    return j->status;
}

void job_cleanup(job *j) {
    // free up memory
    for (int i = 0; i < j->num_processes; i++) {
        free(j->processes[i]->argv);
        free(j->processes[i]);
    }

    free(j->processes);
    free(j->cmd);
    free(j->_cmd);
    free(j);
}

int job_start_pipe(job *j) {

    // check if redirection operators were present, and remap fd

    // the first process reads from a file instead of stdin
    if (j->redir_in != NULL) {
        int fd_in = open(j->redir_in, O_RDONLY);
        if (fd_in == -1) {
            perror("opening input file");
            job_cleanup(j);
            return -1;
        }
        j->processes[0]->stdin_fd = fd_in;
    }

    // the last process writes to a file instead of stdout
    if (j->redir_out != NULL) {
        int oflags = O_CREAT | O_WRONLY;
        if (j->redir_out)
            oflags |= O_APPEND;
        int fd_out = open(j->redir_out, oflags, PERM_BITS | S_IRGRP | S_IROTH);
        if (fd_out == -1) {
            perror("opening output file");
            job_cleanup(j);
            return -1;
        }
        j->processes[j->num_processes - 1]->stdout_fd = fd_out;
    }

    // set stdin/stdout for each process and launch it (with the same pgid)
    int pright[2], pleft[2];

    for (int i = 0; i < j->num_processes; i++) {

        // assign stdin and stdout to the correct pipe ends
        if (i > 0) {
            j->processes[i]->stdin_fd = pleft[0];
        }
        if (i < j->num_processes - 1) {
            if (pipe(pright) == -1) {
                perror("pipe creation");
                return -1;
            }
            j->processes[i]->stdout_fd = pright[1];
        }

        // ensure all have the same pgid and launch
        j->processes[i]->pgid = j->pgid;

        // launch
        if (proc_launch(j->processes[i]) == -1)
            return -1;
        if (i == 0)
            j->pgid = j->processes[i]->pgid; // set the pgid that the next processes need

        // close fds
        if (i > 0) {
            if (close(pleft[0]) == -1) {
                perror("closing pipe");
                return -1;
            }
        }
        if (i < j->num_processes - 1) {
            if (close(pright[1]) == -1) {
                perror("closing pipe");
                return -1;
            }
        }

        // prepare for next process
        pleft[0] = pright[0], pleft[1] = pright[1];
    }

    // close remapped fds, if any
    if (j->processes[0]->stdin_fd != STDIN_FILENO)
        if (close(j->processes[0]->stdin_fd) == -1)
            perror("closing input file");

    if (j->processes[j->num_processes - 1]->stdout_fd != STDOUT_FILENO)
        if (close(j->processes[j->num_processes - 1]->stdout_fd) == -1)
            perror("closing output file");

    return 0;
}

int job_start_msgq(job *j) {

    int mq = msgget(IPC_PRIVATE, PERM_BITS);
    if (mq == -1) {
        perror("msgget");
        return -1;
    }

    // setup the left process
    int fd[j->num_processes][2];
    if (pipe(fd[0]) == -1) {
        perror("pipe");
        return -1;
    }

    j->processes[0]->stdout_fd = fd[0][1];

    if (proc_launch(j->processes[0]) == -1)
        return -1;

    // update pgid
    j->pgid = j->processes[0]->pgid;

    // read into the msgq
    struct msgbuf buf = {
        .mtype = 1,
    };

    ssize_t n;
    close(fd[0][1]);
    n = read(fd[0][0], buf.mtext, sizeof buf.mtext - 1);
    if (n == -1) {
        perror("read error");
        return -1;
    }

    buf.mtext[n] = '\0';

    // add to the msgq
    if (msgsnd(mq, &buf, n, 0) == -1) {
        perror("msgq send");
        return -1;
    }

    close(fd[0][0]);

    // setup the rhs processes
    for (int i = 1; i < j->num_processes; i++) {

        if (pipe(fd[i]) == -1) {
            perror("pipe");
            return -1;
        }

        // update pgid
        j->processes[i]->pgid = j->pgid;

        // update stdin
        j->processes[i]->stdin_fd = fd[i][0];
    }

    // fill in the write ends
    for (;;) {
        n = msgrcv(mq, &buf, BUFLEN, 0, IPC_NOWAIT);
        if (n == -1) {
            if (errno == ENOMSG)
                break;
            else {
                perror("msgq receive");
                return -1;
            }
        }

        for (int i = 1; i < j->num_processes; i++) {
            if (write(fd[i][1], buf.mtext, n) == -1) {
                perror("writing to pipe");
                return -1;
            }
        }
    }

    // launch and close the pipes
    for (int i = 1; i < j->num_processes; i++) {
        close(fd[i][1]);
        proc_launch(j->processes[i]);
        close(fd[i][0]);
    }

    // destroy the message queue
    if (msgctl(mq, IPC_RMID, NULL) == -1)
        perror("Failed to delete message queue");

    return 0;
}

int job_start_shdm(job *j) {

    char shdm_name[BUFLEN];
    snprintf(shdm_name, BUFLEN, "shdm%d", shdm_num++);

    // create the shared memory object
    int fd = shm_open(shdm_name, O_RDWR | O_CREAT, PERM_BITS);
    if (fd == -1) {
        perror("shm open");
        return -1;
    }

    // lhs process
    j->processes[0]->stdout_fd = fd;
    if (proc_launch(j->processes[0]) == -1)
        return -1;
    j->pgid = j->processes[0]->pgid;
    if (close(fd) == -1) {
        perror("closing shdm");
        return -1;
    }

    // rhs processes
    for (int i = 1; i < j->num_processes; i++) {

        // open the shared memory for reading
        fd = shm_open(shdm_name, O_RDONLY, 0);
        if (fd == -1) {
            perror("shm open");
            return -1;
        }

        j->processes[i]->stdin_fd = fd;
        j->processes[i]->pgid     = j->pgid;
        if (proc_launch(j->processes[i]) == -1)
            return -1;

        if (close(fd) == -1) {
            perror("closing shdm");
            return -1;
        }
    }

    // unlink
    if (shm_unlink(shdm_name) == -1) {
        perror("shdm unlink");
        return -1;
    }

    return 0;
}

void job_info(const job *j) {
    // clang-format off
    printf("\n----------- JOB STATUS -----------\n");
    printf("PGID: %d. Status: %s. Command: \"%s\". Processes: %d.\n",
            j->pgid,
            job_status_string[j->status],
            j->cmd,
            j->num_processes);

    printf("\n------- PROCESS(es) STATUS -------\n");
    for (int i = 0; i < j->num_processes; i++) {
        proc_info(j->processes[i]);
    }
    // clang-format on
}

void job_info_lite(const job *j) {
    // clang-format off
    printf("[%d]\t%s\t%s\n",
            j->pgid,
            job_status_string[j->status],
            j->cmd);
    // clang-format on
}

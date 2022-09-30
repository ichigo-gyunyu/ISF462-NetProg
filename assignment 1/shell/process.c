#include "process.h"
#include "shell.h"

// for printing
static const char *proc_status_string[] = {
    [PS_RUNNING_FG] = "Running (fg)",
    [PS_RUNNING_BG] = "Running (bg)",
    [PS_STOPPED]    = "Stopped",
    [PS_COMPLETED]  = "Completed",

};

process *proc_make(char *file) {
    process *pr = malloc(sizeof *pr);

    *pr = (process){
        .pid       = -1,
        .pgid      = -1,
        .file      = file,
        .argc      = 1,
        .is_bg     = false,
        .stdin_fd  = STDIN_FILENO,
        .stdout_fd = STDOUT_FILENO,
        .argv      = malloc(sizeof(char *)),
    };

    pr->argv[0] = file;

    return pr;
}

int proc_addarg(process *pr, char *arg) {
    pr->argc++;
    // optimizable
    if ((pr->argv = realloc(pr->argv, pr->argc * sizeof(char *))) == NULL)
        perror_and_exit("no memory");
    pr->argv[pr->argc - 1] = arg;

    if (arg == NULL)
        pr->argc--; // null terminated args list

    return 0;
}

PROC_STATUS proc_waitfor(process *pr) {
    int status;
    waitpid(pr->pid, &status, WUNTRACED); // for stopped processes as well

    // exit status
    if (WIFEXITED(status)) {
        pr->status = PS_COMPLETED;
        // process terminated unsuccessfully
        if (WEXITSTATUS(status) != EXIT_SUCCESS)
            printf("\nProcess terminated with status %d\n", WEXITSTATUS(status));
    }

    // if a signal interrupted the process
    else if (WIFSIGNALED(status)) {
        printf("\nProcess killed by signal %d (%s)\n", WTERMSIG(status), strsignal(WTERMSIG(status)));
        pr->status = PS_COMPLETED;
    }

    // stop
    else if (WIFSTOPPED(status)) {
        printf("\nProcess stopped by signal %d (%s)\n", WSTOPSIG(status), strsignal(WSTOPSIG(status)));
        printf("Use fg or bg to resume\n");
        pr->status = PS_STOPPED;
    }

    // continue (may not be portable)
#ifdef WIFCONTINUED
    else if (WIFCONTINUED(status)) {
        printf("Process continued\n");
    }
#endif

    // should not be here!
    else {
        perror_and_exit("waitpid status");
    }

    return pr->status;
}

PROC_STATUS proc_update_status(process *pr) {
    int   status;
    pid_t p = waitpid(pr->pid, &status, WUNTRACED | WNOHANG);

    // if the process' status has changed
    if (p == pr->pid) {
        if (WIFSTOPPED(status)) {
            printf("\nProcess stopped by signal %d (%s)\n", WSTOPSIG(status), strsignal(WSTOPSIG(status)));
            return pr->status = PS_STOPPED;
        } else if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != EXIT_SUCCESS)
                printf("\nProcess terminated with status %d\n", WEXITSTATUS(status));
            return pr->status = PS_COMPLETED;
        } else if (WIFSIGNALED(status)) {
            printf("\nProcess killed by signal %d (%s)\n", WTERMSIG(status), strsignal(WTERMSIG(status)));
        } else
            printf("should not be here... (proc_update_status)\n");
    }

    return pr->status;
}

int proc_launch(process *pr) {

    pid_t child_pid = fork();

    switch (child_pid) {
    case -1:
        perror("fork error");
        return -1;

    case 0: {

        // child
        pid_t mypid = getpid();
        pr->pid     = mypid;
        if (pr->pgid == -1)
            pr->pgid = mypid;

        // put the process in it's own process group
        if (setpgid(pr->pid, pr->pgid) == -1)
            perror_and_exit("setpgid error");

        // if foreground give it control of the terminal
        if (!pr->is_bg) {
            if (tcsetpgrp(cterminal_fd, pr->pgid) == -1)
                perror_and_exit("tcsetpgrp error");
        }

        // restore jobcontrol signals
        for (int i = 0; i < num_jobcontrol_signals; i++) {
            if (signal(jobcontrol_signals[i], SIG_DFL) == SIG_ERR)
                perror_and_exit("signal restore error");
        }

        // adjust stdin
        if (pr->stdin_fd != STDIN_FILENO) {
            if (dup2(pr->stdin_fd, STDIN_FILENO) == -1) {
                perror("dup2");
                return -1;
            }
            if (close(pr->stdin_fd) == -1) {
                perror("closing pipe");
                return -1;
            }
        }

        // adjust stdout
        if (pr->stdout_fd != STDOUT_FILENO) {
            if (dup2(pr->stdout_fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                return -1;
            }
            if (close(pr->stdout_fd) == -1) {
                perror("closing pipe");
                return -1;
            }
        }

        // exec it!
        execvp(pr->file, pr->argv);

        // could not exec
        char tmpbuf[BUFLEN];
        snprintf(tmpbuf, BUFLEN - 1, "shell (%s)", pr->file);
        perror(tmpbuf);
        if (errno == ENOENT)
            exit(PROCESS_NOT_FOUND);
        exit(EXIT_FAILURE);
    }

    default: {

        pr->pid = child_pid;
        if (pr->pgid == -1)
            pr->pgid = child_pid;

        // set process group in parent as well to
        // avoid race conditions when signals are sent
        if (setpgid(pr->pid, pr->pgid) == -1 && errno != EACCES) {
            perror("setpgid error");
            return -1;
        }

        // if foreground too
        if (!pr->is_bg) {
            if (tcsetpgrp(cterminal_fd, pr->pgid) == -1)
                perror_and_exit("tcsetpgrp error");
        }
    }
    }

    return 0;
}

int proc_dmn(process *pr) {

    pid_t child_pid = fork();

    switch (child_pid) {
    case -1:
        perror("fork error");
        return -1;

    case 0: {

        /**
         * Perform a double fork so that the daemon
         * process can be reparented to init
         */
        child_pid = fork();
        switch (child_pid) {
        case -1:
            perror("fork error");
            return -1;
        case 0: {

            // restore signals to defaults
            for (int i = 0; i < num_jobcontrol_signals; i++) {
                if (signal(jobcontrol_signals[i], SIG_DFL) == SIG_ERR)
                    perror_and_exit("signal restore error");
            }

            // start a new session, free of any controlling terminal
            if (setsid() == -1)
                perror_and_exit("setsid");

            // ignore SIGHUP, because we are once again forking and killing the parent
            signal(SIGHUP, SIG_IGN);

            // if the daemon process opens a terminal
            // ensure it does not become a controlling terminal
            switch (child_pid = fork()) {
            case -1:
                perror_and_exit("fork error");
            case 0:
                break;
            default:
                exit(EXIT_SUCCESS);
            }

            // clear umask
            umask(0);

            // change dir to root
            chdir("/");

            // close fds
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            // remap to dev null
            int fd = open("/dev/null", O_RDWR);
            if (fd != STDIN_FILENO)
                exit(EXIT_FAILURE);
            if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
                exit(EXIT_FAILURE);
            if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
                exit(EXIT_FAILURE);

            // exec
            execvp(pr->file, pr->argv);

            // could not exec
            char tmpbuf[BUFLEN];
            snprintf(tmpbuf, BUFLEN - 1, "shell (%s)", pr->file);
            perror(tmpbuf);
            if (errno == ENOENT)
                exit(PROCESS_NOT_FOUND);
            exit(EXIT_FAILURE);
        }
        default:
            free(pr->argv);
            free(pr);
            exit(EXIT_SUCCESS); // exit so that daemon gets reparented to init
        }
    }

    default:
        free(pr->argv);
        free(pr);
        wait(NULL);
    }

    return 0;
}

void proc_info(const process *pr) {
    // clang-format off
    printf("Process: ");
    for(int i = 0; i < pr->argc; i++)
        printf("%s ", pr->argv[i]);
    printf("\n");

    printf("PGID: %d. PID: %d. Status: %s. STDIN fd: %d. STDOUT fd: %d.\n",
            pr->pgid,
            pr->pid,
            proc_status_string[pr->status],
            pr->stdin_fd,
            pr->stdout_fd);

    printf("\n");
    // clang-format on
}

#include "shell.h"

#define VERBOSE_OPT "-v"

static bool verbose = false; // for vebose output

// default data/state variables for the shell
pid_t          mypid;
pid_t          mypgid;
pid_t          termpgid;
int            num_jobcontrol_signals;
int            cterminal_fd;
struct termios cterminal_attr;

// linked list of background jobs
typedef struct job_node {
    job             *j;
    struct job_node *next;
} job_node;

// start with an empty list
static struct bg_jobs_list {
    int       num_jobs;
    job_node *head;
    job_node *tail;
} bg_jobs = {.num_jobs = 0, .head = NULL, .tail = NULL};

// initializing the shell
void init_controlling_terminal();
void init_shell_foreground();
void init_jobcontrol();

void      update_jobs();
job_node *remove_job_node(job_node *n, job_node *prev);

int main(int argc, char **argv) {

    // check if verbose mode needs to be enabled
    if (argc == 2 && strcmp(argv[1], VERBOSE_OPT) == 0)
        verbose = true;

    printf("\nSimple shell. Type exit or Ctrl+D to exit.\n\n");

    // init stuff
    mypid    = getpid();
    mypgid   = getpgid(mypid);
    termpgid = tcgetpgrp(cterminal_fd);
    init_controlling_terminal();
    init_shell_foreground();
    init_jobcontrol();

    char inbuf[BUFLEN];
    printf("\n");
    // shell loop
    for (;;) {

        printf(PROMPT);

        // read input
        if (fgets(inbuf, BUFLEN, stdin) == NULL)
            break;

        // strip trailing \n
        inbuf[strcspn(inbuf, "\n")] = '\0';

        // check if background jobs have completed (synchronous)
        update_jobs();

        // parse the command
        PARSE_STATUS parse_status;
        job         *j = parse_cmd(inbuf, &parse_status);

        // proceed
        JOB_STATUS job_status;
        switch (parse_status) {
        case PARSE_ERROR:
            printf("parse error\n");
            continue;
        case PARSE_BUILTIN_EXIT:
            shell_exit(false);
            continue;
        case PARSE_BUILTIN_CLEAR:
            printf("\e[1;1H\e[2J"); // clears the screen
            continue;
        case PARSE_BUILTIN_JOBS:
            shell_list_jobs();
            continue;
        case PARSE_EMPTY:
            continue; // user pressed return
        case PARSE_BUILTIN_BG:
            if (j == NULL)
                printf("No such job to resume\n");
            else
                job_status = job_resume(j, true);
            break;
        case PARSE_BUILTIN_FG:
            if (j == NULL)
                printf("No such job to resume\n");
            else
                job_status = job_resume(j, false);
            break;
        case PARSE_JOB:
            job_status = job_start(j);
            break;
        case PARSE_BUILTIN_DMN:
            if (proc_dmn((process *)j) != -1)
                printf("Success. Verify with ps ax\n");
            continue;
        }

        if (j == NULL)
            continue; // no job to continue further

        // next prompt if job is running in the background
        if (job_status == JS_RUNNING_BG) {

            // it's a new job put into the background
            if (parse_status == PARSE_JOB)
                shell_add_job(j);
            job_info_lite(j);
        }

        // wait if foreground
        else if (job_status == JS_RUNNING_FG) {
            job_status = job_waitfor(j);

            // printf("before restore%d\n", tcgetpgrp(cterminal_fd));
            // get shell into the foreground
            if (tcsetpgrp(cterminal_fd, mypid) == -1) {
                perror_and_exit("tcsetpgrp error");
            }

            if (verbose)
                job_info(j);
            printf("\n");
            job_info_lite(j);

            if (job_status == JS_COMPLETED)
                job_cleanup(j);
            else if (job_status == JS_STOPPED)
                shell_add_job(j);
        }

        // restore shell's terminal attributes
        if (tcsetattr(cterminal_fd, TCSADRAIN, &cterminal_attr) == -1)
            perror_and_exit("tcsetattr error");
    }

    shell_exit(true);
}

void shell_add_job(job *j) {

    // create a new job node
    job_node *node = malloc(sizeof *node);
    *node          = (job_node){.j = j, .next = NULL};

    // add it to the active jobs linked list
    if (bg_jobs.num_jobs == 0)
        bg_jobs.head = node;
    else
        bg_jobs.tail->next = node;

    bg_jobs.tail = node;
    bg_jobs.num_jobs++;
}

void shell_list_jobs() {

    // list out background jobs (running and stopped)
    printf("PGID\tStatus\tCommand\n");

    for (job_node *n = bg_jobs.head; n != NULL; n = n->next)
        job_info_lite(n->j);
}

void shell_exit(bool newline) {

    // (politely) kill any background jobs
    for (job_node *n = bg_jobs.head; n != NULL;) {
        killpg(n->j->pgid, SIGTERM); // request termination

        // free up node memory
        job_node *tmp = n->next;
        job_cleanup(n->j);
        free(n);
        n = tmp;
    }

    if (newline)
        printf("\n"); // aesthetics
    exit(EXIT_SUCCESS);
}

void init_controlling_terminal() {

    // getting the controlling terminal fd
    if ((cterminal_fd = open("/dev/tty", O_RDWR)) == -1)
        perror_and_exit("no controlling terminal");

    // save terminal attributes which will be restored in the event a screen
    // modifying program (like vim) modifies them without restoring it
    if (tcgetattr(cterminal_fd, &cterminal_attr) == -1)
        perror_and_exit("tcgetattr error");
}

void init_shell_foreground() {

    // verify shell is in the foreground

    // forcefully bring shell into the foreground
    if (mypid != mypgid || mypgid != termpgid) {
        printf("\nBringing shell to the foreground of the controlling terminal...\n");

        if (setpgid(mypid, mypid) == -1)
            perror_and_exit("error while changing pgid");

        if (tcsetpgrp(cterminal_fd, mypid) == -1)
            perror_and_exit("error while changing terminal control");

        mypgid   = getpgid(mypid);
        termpgid = tcgetpgrp(cterminal_fd);
    }

    printf("Shell pid                 %d\n", mypid);
    printf("Shell pgid                %d\n", mypgid);
    printf("Controlling terminal pgid %d\n", termpgid);
}

void init_jobcontrol() {

    // ignore job control related signals
    num_jobcontrol_signals = NUM_ELEM(jobcontrol_signals);
    for (int i = 0; i < num_jobcontrol_signals; i++)
        if (signal(jobcontrol_signals[i], SIG_IGN) == SIG_ERR)
            perror_and_exit("signal ignore error");
}

void update_jobs() {

    // request status update for each active job
    for (job_node *n = bg_jobs.head, *prev = NULL; n != NULL;) {
        JOB_STATUS js = job_update_status(n->j);

        // cleanup and remove from active jobs list
        if (js == JS_COMPLETED) {
            job_cleanup(n->j);
            n = remove_job_node(n, prev);
        }

        else {
            prev = n;
            n    = n->next;
        }
    }
}

job_node *remove_job_node(job_node *n, job_node *prev) {

    // removing the last job
    bg_jobs.num_jobs--;
    if (bg_jobs.num_jobs == 0) {
        free(n);
        return bg_jobs.head = bg_jobs.tail = NULL;
    }

    // removing the head
    if (n == bg_jobs.head) {
        bg_jobs.head = n->next;
        free(n);
        return bg_jobs.head;
    }

    // removing the tail
    if (n == bg_jobs.tail) {
        bg_jobs.tail = prev;
        prev->next   = NULL;
        free(n);
        return NULL;
    }

    // anything in-between
    prev->next = n->next;
    free(n);
    return prev->next;
}

job *shell_get_job(pid_t pgid) {

    if (pgid == 0)
        return (bg_jobs.head) ? bg_jobs.head->j : NULL;

    job_node *n;
    for (n = bg_jobs.head; n != NULL; n = n->next) {
        if (n->j->pgid == pgid)
            break;
    }

    return (n) ? n->j : NULL;
}

void shell_remove_job(job *j) {

    // find the job node containing job j and remove it
    for (job_node *n = bg_jobs.head, *prev = NULL; n != NULL;) {
        if (n->j == j) {
            n = remove_job_node(n, prev);
        } else {
            prev = n;
            n    = n->next;
        }
    }
}

#include "parser.h"
#include "shell.h"

// parser is a state machine
typedef enum PARSER_STATE {
    STATE_START,
    STATE_MED_PIPE,
    STATE_MED_MSGQ,
    STATE_MED_SHDM,
    STATE_REDIR_IN,
    STATE_REDIR_OUT,
    STATE_REDIR_APP,
    STATE_DONE_FOREGROUND,
    STATE_DONE_BACKGROUND,
    STATE_DONE_ERROR,

} PARSER_STATE;

// some internal helper functions
static bool     str_equal(const char *s1, const char *s2);
static bool     str_is_num(const char *s);
static bool     is_final_state(const PARSER_STATE state);
static bool     is_redir_state(const PARSER_STATE state);
static process *parse_args(char **cmd, char *file, PARSER_STATE *state);
static process *parse_dmn(char **cmd, PARSER_STATE *state);
static char    *parse_redir(char **cmd, PARSER_STATE *state);

job *parse_cmd(char *cmd, PARSE_STATUS *parse_status) {

    // initialize parser state
    PARSER_STATE state = STATE_START;

    // make a copy as strtok is destructive
    char *cmd_cpy = strdup(cmd);
    void *to_free = cmd_cpy;

    // parse the first word in cmd
    char *word;
    if ((word = strtok_r(cmd_cpy, " ", &cmd_cpy)) == NULL) {
        *parse_status = PARSE_EMPTY;
        free(to_free);
        return NULL;
    }

    // exit
    if (str_equal(word, CMD_EXIT)) {
        *parse_status = PARSE_BUILTIN_EXIT;
        free(to_free);
        return NULL;
    }

    // clear
    if (str_equal(word, CMD_CLEAR)) {
        *parse_status = PARSE_BUILTIN_CLEAR;
        free(to_free);
        return NULL;
    }

    // fg or bg
    if (str_equal(word, CMD_FG) || str_equal(word, CMD_BG)) {
        bool is_bg = str_equal(word, CMD_BG);

        // check if any args have been provided
        if ((word = strtok_r(cmd_cpy, " ", &cmd_cpy)) == NULL) {
            *parse_status = (is_bg) ? PARSE_BUILTIN_BG : PARSE_BUILTIN_FG;
            free(to_free);
            return shell_get_job(0);
        }

        if (!str_is_num(word)) {
            *parse_status = PARSE_ERROR;
            printf("Invalid PGID\n");
            free(to_free);
            return NULL;
        }

        *parse_status = (is_bg) ? PARSE_BUILTIN_BG : PARSE_BUILTIN_FG;
        job *j        = shell_get_job((pid_t)strtol(word, NULL, 10));
        free(to_free);
        return j;
    }

    // jobs
    if (str_equal(word, CMD_JOBS)) {
        *parse_status = PARSE_BUILTIN_JOBS;
        free(to_free);
        return NULL;
    }

    // daemonize
    if (str_equal(word, CMD_DMN)) {
        process *pr = parse_dmn(&cmd_cpy, &state);
        if (state == STATE_DONE_ERROR)
            *parse_status = PARSE_ERROR;
        else
            *parse_status = PARSE_BUILTIN_DMN;

        free(to_free);
        return (job *)pr; // a lie
    }

    // setup the job
    job *j = job_make(cmd, to_free);

    // first process (of possibly many) in the pipeline
    bool pm_sticky = false; // if the pipeline medium has changed
    while (!is_final_state(state)) {
        char *file = word;

        job_addprocess(j, parse_args(&cmd_cpy, file, &state));

        // stop parsing immediately if in error state
        if (state == STATE_DONE_ERROR)
            break;

        // if the job has | or ## or SS
        if (!pm_sticky) {
            pm_sticky = true;
            switch (state) {
            case STATE_MED_PIPE:
                j->pipeline_medium = PM_PIPE;
                break;
            case STATE_MED_MSGQ:
                j->pipeline_medium = PM_MSGQ;
                break;
            case STATE_MED_SHDM:
                j->pipeline_medium = PM_SHDM;
                break;
            default:
                break; // -Wall
            }
        }

        // < or > or >> encountered
        if (is_redir_state(state)) {
            bool  in = false, out = false, app = false;
            char *file;
            while (!is_final_state(state)) {
                switch (state) {

                    // >>
                case STATE_REDIR_APP: {
                    if (app || out) {
                        state = STATE_DONE_ERROR;
                        break;
                    }

                    app          = true;
                    file         = parse_redir(&cmd_cpy, &state);
                    j->redir_out = file;
                    j->redir_app = true;
                    break;
                }

                    // >
                case STATE_REDIR_OUT: {
                    if (out || app) {
                        state = STATE_DONE_ERROR;
                        break;
                    }

                    out          = true;
                    file         = parse_redir(&cmd_cpy, &state);
                    j->redir_out = file;
                    break;
                }

                    // <
                case STATE_REDIR_IN: {
                    if (in) {
                        state = STATE_DONE_ERROR;
                        break;
                    }

                    in          = true;
                    file        = parse_redir(&cmd_cpy, &state);
                    j->redir_in = file;
                    break;
                }

                default:
                    state = STATE_DONE_ERROR;
                    break;
                }
            }

            break;
        }

        // start parsing the next process, if any
        if ((word = strtok_r(cmd_cpy, " ", &cmd_cpy)) == NULL)
            break;
    }

    // parse error
    if (state == STATE_DONE_ERROR) {
        *parse_status = PARSE_ERROR;
        job_cleanup(j);
        return NULL;
    }

    // the command ended with an '&'
    if (state == STATE_DONE_BACKGROUND) {
        job_makebg(j);
    }

    *parse_status = PARSE_JOB;

    return j;
}

static process *parse_args(char **cmd, char *file, PARSER_STATE *state) {
    process *pr = proc_make(file);

    // begin parsing args
    char *arg;
    for (;;) {
        if ((arg = strtok_r(*cmd, " ", cmd)) == NULL) {
            *state = (*state == STATE_DONE_ERROR) ? STATE_DONE_ERROR : STATE_DONE_FOREGROUND;
            break;
        }

        if (str_equal(arg, BACKGROUND)) {
            *state = (*state == STATE_DONE_ERROR) ? STATE_DONE_ERROR : STATE_DONE_BACKGROUND;
            break;
        }

        if (str_equal(arg, SYMBOL_PIPE)) {
            /**
             * Ensure we are not mixing mediums (pipe with shared memory, etc.)
             * And there has not been any errors before
             */
            if (*state == STATE_MED_MSGQ || *state == STATE_MED_SHDM || *state == STATE_DONE_ERROR)
                *state = STATE_DONE_ERROR;
            else
                *state = STATE_MED_PIPE;

            break;
        }

        if (str_equal(arg, SYMBOL_MSGQ)) {

            // do not allow multiple ## operators (ls ## wc ## sort - not allowed)
            if (*state != STATE_START)
                *state = STATE_DONE_ERROR;
            else
                *state = STATE_MED_MSGQ;

            break;
        }

        if (str_equal(arg, SYMBOL_SHDM)) {

            // do not allow multiple SS operators (ls SS wc SS sort - not allowed)
            if (*state != STATE_START)
                *state = STATE_DONE_ERROR;
            else
                *state = STATE_MED_SHDM;

            break;
        }

        if (str_equal(arg, ",")) {
            // comma (,) is only valid for MSGQ and SHDM
            if (*state == STATE_MED_MSGQ || *state == STATE_MED_SHDM)
                break;
        }

        if (str_equal(arg, SYMBOL_REDIR_IN)) {

            // allow redirection only for single process/pipe
            if (*state != STATE_START && *state != STATE_MED_PIPE)
                *state = STATE_DONE_ERROR;
            else
                *state = STATE_REDIR_IN;

            break;
        }

        if (str_equal(arg, SYMBOL_REDIR_OUT)) {

            // allow redirection only for single process/pipe
            if (*state != STATE_START && *state != STATE_MED_PIPE)
                *state = STATE_DONE_ERROR;
            else
                *state = STATE_REDIR_OUT;

            break;
        }

        if (str_equal(arg, SYMBOL_REDIR_APP)) {

            // allow redirection only for single process/pipe
            if (*state != STATE_START && *state != STATE_MED_PIPE)
                *state = STATE_DONE_ERROR;
            else
                *state = STATE_REDIR_APP;

            break;
        }

        proc_addarg(pr, arg);
    }

    // null terminated argv
    proc_addarg(pr, NULL);

    return pr;
}

static process *parse_dmn(char **cmd, PARSER_STATE *state) {

    char *word = strtok_r(*cmd, " ", cmd);
    if (word == NULL) {
        *state = STATE_DONE_ERROR;
        printf("daemonize what?\n");
        return NULL;
    }

    process *pr = proc_make(word);

    // parse args
    for (;;) {

        if ((word = strtok_r(*cmd, " ", cmd)) == NULL) {
            *state = (*state == STATE_DONE_ERROR) ? STATE_DONE_ERROR : STATE_DONE_FOREGROUND;
            break;
        }

        proc_addarg(pr, word);
    }

    proc_addarg(pr, NULL);

    return pr;
}

static char *parse_redir(char **cmd, PARSER_STATE *state) {

    char *word = strtok_r(*cmd, " ", cmd);
    if (word == NULL) {
        *state = STATE_DONE_ERROR;
        printf("no file to redirect\n");
        return NULL;
    }

    char *file = word;

    // parse next
    word = strtok_r(*cmd, " ", cmd);

    // update parser state
    if (word == NULL)
        *state = STATE_DONE_FOREGROUND;
    else if (str_equal(word, BACKGROUND))
        *state = STATE_DONE_BACKGROUND;
    else if (str_equal(word, SYMBOL_REDIR_IN))
        *state = STATE_REDIR_IN;
    else if (str_equal(word, SYMBOL_REDIR_OUT))
        *state = STATE_REDIR_OUT;
    else if (str_equal(word, SYMBOL_REDIR_APP))
        *state = STATE_REDIR_APP;
    else
        *state = STATE_DONE_ERROR;

    return file;
}

static bool str_equal(const char *s1, const char *s2) { return !(strncmp(s1, s2, strlen(s1))); }

static bool is_final_state(const PARSER_STATE state) {
    return (state == STATE_DONE_BACKGROUND || state == STATE_DONE_FOREGROUND || state == STATE_DONE_ERROR);
}

static bool is_redir_state(const PARSER_STATE state) {
    return (state == STATE_REDIR_IN || state == STATE_REDIR_OUT || state == STATE_REDIR_APP);
}

static bool str_is_num(const char *s) {

    int n = strlen(s);
    for (int i = 0; i < n; i++) {
        if (!isdigit(s[i]))
            return false;
    }

    return true;
}

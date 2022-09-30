/**
 * Simulates a state machine to parse the user input
 */

#ifndef PARSER_H
#define PARSER_H

#include "job.h"
#include "process.h"
#include "utils.h"

// values for parse_status
typedef enum PARSE_STATUS {
    PARSE_ERROR,
    PARSE_EMPTY,
    PARSE_JOB,
    PARSE_BUILTIN_CLEAR,
    PARSE_BUILTIN_EXIT,
    PARSE_BUILTIN_JOBS,
    PARSE_BUILTIN_FG,
    PARSE_BUILTIN_BG,
    PARSE_BUILTIN_DMN,

} PARSE_STATUS;

/**
 * Takes in the user's input string and attempts to parse it.
 *
 * Returns a job corresponding to the input if successful.
 * Returns NULL if it could not parse the user's input
 * The parse status is returned as an out parameter
 */
job *parse_cmd(char *cmd, PARSE_STATUS *parse_status);

#endif

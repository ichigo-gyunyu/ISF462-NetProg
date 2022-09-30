#include <unistd.h>

void exit_msg(char *msg);
pid_t create_fork();
void log_entry(pid_t p, char *msg);

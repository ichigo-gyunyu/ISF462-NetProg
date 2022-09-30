#include <unistd.h>

void exit_msg(char *msg);
pid_t create_fork();
void delete_msgq(int msqid);

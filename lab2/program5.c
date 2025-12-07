#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int main()
{
    int pipefd[2];
    pipe(pipefd);
    
    if (pipe(pipefd) == -1)
    {
        perror("pipe failed");
        exit(1);
    }

    pid_t c1 = fork();

    if (c1 == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        execlp("who", "who", NULL);
        exit(1);
    }

    pid_t c2 = fork();

    if (c2 == 0)
    {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        execlp("sort", "sort", "-u", NULL);
        exit(1);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    wait(NULL);
    wait(NULL);

    return 0;
}
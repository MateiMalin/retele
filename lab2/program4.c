#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

void handle_sigusr1(int sig)
{
    printf("Am primit semnal.\n");
}
int main()
{
    signal(SIGINT, SIG_IGN);//ignoring the interrupt signal
    signal(SIGUSR1, handle_sigusr1);//handling the user defined signal
    int nr = 1;

    while (1)
    {
        sleep(3);
        printf("Asta este pid-ul curent: %d si la al catelea loop sunt acum: %d;\n", getpid(), nr);
        nr += 1;
        if (nr >= 20)
        {
            signal(SIGINT, SIG_DFL);//restoring the default behavior of the interrupt signal
            printf("Am terminat bucla, acum pot sa mor linistit.\n");
            exit(0);
        }
    }
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main()
{
    pid_t pid;

    if((pid=fork())<0)
    {
        perror("fork()");
        exit(1);
    }
    
    if(pid==0)
    {
        pid_t pid_fiu=getpid();

        sleep(2);
        printf ("Aici, fiul are pid-ul : %d iar tatal are pid-ul : %d", pid_fiu, getppid());
        exit(EXIT_SUCCESS);
    }
    else
    {
        if(pid%2==0)
        {
            printf("Tatal are pid par, deci nu trebuie sa moara inaintea fiului");

            waitpid(pid, NULL, 0);//wait for the child to finish
            printf("acuma pot sa mor in pace");
            fflush(stdout);//here to ensure the output is printed before because fflush does not work on exit

            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("am pid impar deci o sa mor inainte copilului ca sa il las orfan");
            fflush(stdout);

            exit(EXIT_SUCCESS);
        }
    }

    return 0;

}
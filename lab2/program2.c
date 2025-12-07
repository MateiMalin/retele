#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// program care executa ls -a -l
// fiu apeleaza exec
// procesul tata asteapta ca fiul sa termine si dupa afiseaza un msj

int main()
{
    pid_t pid = fork();

    if (pid < 0)
    {
        printf("eroare la fork()");
        exit(0);
    }

    if (pid == 0)
    {
        // execlp("ls", "ls", "-a", "-l", NULL);
        char *argv[] = {"ls", "-a", "-l", NULL};
        execvp("ls", argv);
        //o sa am un char in care am toti parametrii
    }
    else
    {
        wait(NULL);
        printf("fiul a reusit cu succes sa apeleze comanda");
    }
}
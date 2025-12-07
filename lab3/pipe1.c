#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

// 1 pipe, tatal scrie un numar
// fiul verifica daca numarul este prim si transmite prin pipe yes/no
// tatal afiseaza raspunsul primit

int main()
{
    int fd[2];

    if (pipe(fd) == -1)
    {
        printf("eroare la pipe");
        exit(-1);
    }

    int pid = fork();

    if (pid == 0)
    {
        int numar;
        char response[24];
        // suntem in copil
        read(fd[0], &numar, sizeof(numar));
        if (numar % 2 == 1)
            strcpy(response, "yes");
        else
            strcpy(response, "no");

        write(fd[1], response, sizeof(response));
        close(fd[0]);
        close(fd[1]);
        exit(0);
    }
    else
    {
        // parent process
        int num;
        char raspuns[24];

        scanf("%d", &num);
        write(fd[1], &num, sizeof(num));

        sleep(1);

        read(fd[0], raspuns, sizeof(raspuns));
        printf("Is the number %d prime ? : \n %s\n", num, raspuns);

        close(fd[0]);
        close(fd[1]);
    }

    return 0;
}
//i dont think you can, really
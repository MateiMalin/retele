#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PORT 2024

extern int errno;

int main()
{
    struct sockaddr_in server;
    struct sockaddr_in from;
    char msg[100];
    char msgrasp[100] = " ";
    int sd;
    int client_count = 0;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server]Eroare la socket().\n");
        return errno;
    }

    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server]Eroare la bind().\n");
        return errno;
    }

    if (listen(sd, 5) == -1)
    {
        perror("[server]Eroare la listen().\n");
        return errno;
    }

    while (1)
    {
        int client;
        int length = sizeof(from);

        printf("[server]Asteptam la portul %d...\n", PORT);
        fflush(stdout);

        client = accept(sd, (struct sockaddr *)&from, &length);
        
        if (client < 0)
        {
            perror("[server]Eroare la accept().\n");
            continue;
        }

        client_count++;

        int pid;
        if ((pid = fork()) == -1)
        {
            perror("[server-parinte] Eroare la fork().\n");
            close(client);
            continue;
        }
        else if (pid == 0)
        {
            close(sd);

            char msg[100];
            char msgrasp[100] = " ";
            long nr_primit;
            long nr_inc;

            bzero(msg, 100);
            printf("[server-fiu %d, ordinul %d] Asteptam mesajul...\n", getpid(), client_count);
            fflush(stdout);

            if (read(client, msg, 100) <= 0)
            {
                perror("[server-fiu] Eroare la read() de la client.\n");
                close(client);
                exit(1);
            }

            printf("[server-fiu %d] Mesajul a fost receptionat...%s\n", getpid(), msg);
            nr_primit = atol(msg);
            nr_inc = nr_primit + 1;

            snprintf(msgrasp, sizeof(msgrasp), "Clientul cu numarul de ordine:%d, a intors numarul %d incrementat cu 1 ajungand la %d", client_count, nr_primit, nr_inc);

            printf("[server-fiu %d] Trimitem mesajul inapoi...%s\n", getpid(), msgrasp);

            if (write(client, msgrasp, 100) <= 0)
            {
                perror("[server-fiu] Eroare la write() catre client.\n");
                close(client);
                exit(1);
            }
            else
                printf("[server-fiu %d] Mesajul a fost trasmis cu succes.\n", getpid());

            close(client);
            exit(0);
        }
        else
        {
            close(client);
            continue;
        }
        close(client);
    } 
}
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
    char msg[1000]; // Mărim bufferul pentru conținut
    char filename[100];
    char msgrasp[100] = " ";
    int sd;
    int client_count = 0;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server]Eroare la socket().\n");
        return errno;
    }

    int enable = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1)
    {
        perror("[server]Eroare la setsockopt(SO_REUSEADDR).\n");
        close(sd);
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

            char msg[1000];
            char filename[100];
            char msgrasp[100] = " ";

            // Primim mai întâi numele fișierului
            bzero(filename, 100);
            if (read(client, filename, 100) <= 0)
            {
                perror("[server-fiu] Eroare la read() pentru nume fisier.\n");
                close(client);
                exit(1);
            }

            printf("[server-fiu %d, ordinul %d] Nume fisier primit: %s\n", getpid(), client_count, filename);

            // Apoi primim conținutul fișierului
            bzero(msg, 1000);
            printf("[server-fiu %d] Asteptam continutul...\n", getpid());
            fflush(stdout);

            if (read(client, msg, 1000) <= 0)
            {
                perror("[server-fiu] Eroare la read() pentru continut.\n");
                close(client);
                exit(1);
            }

            printf("[server-fiu %d] Continut primit: %s\n", getpid(), msg);

            // Creăm fișierul cu conținutul primit
            FILE *file = fopen(filename, "w");
            if (file != NULL)
            {
                fputs(msg, file);
                fclose(file);
                printf("[server-fiu %d] File '%s' created with content successfully.\n", getpid(), filename);
                snprintf(msgrasp, sizeof(msgrasp), "File '%s' created successfully with content", filename);
            }
            else
            {
                perror("[server-fiu] Error creating file");
                snprintf(msgrasp, sizeof(msgrasp), "Error creating file '%s'", filename);
            }

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
        }
    }
}
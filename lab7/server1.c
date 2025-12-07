/* servTCPPreFork.c - Exemplu de server TCP PRE-FORK (modificat minimal)
   Creeaza un grup de fii (workers) in avans.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>

/* portul folosit */
#define PORT 2024
/* Numarul de procese fiu (workers) create in avans */
#define N_CHILDREN 2

extern int errno;

int main()
{
    struct sockaddr_in server; // structura folosita de server
    struct sockaddr_in from;
    int sd;
    pid_t pid;
    int i;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server]Eroare la socket().\n");
        return errno;
    }

    // configuram adresele
    // structura server primeste adresa
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server]Eroare la bind().\n");
        return errno;
    }

    if (listen(sd, 10) == -1)
    {
        perror("[server]Eroare la listen().\n");
        return errno;
    }

    /* Ignoram semnalul SIGCHLD pentru a preveni fiii zombie (varianta simpla) */
    signal(SIGCHLD, SIG_IGN);

    printf("[parinte %d] Serverul porneste. Se creeaza %d fii (workers)...\n", getpid(), N_CHILDREN);

    // facum numarul de fii
    for (i = 0; i < N_CHILDREN; i++)
    {
        pid = fork();

        if (pid < 0)
            perror("[parinte] Eroare la fork().\n");

        if (pid == 0)
            break;
    }

    if (pid > 0)
    {
        // === ROLUL PROCESULUI PARINTE ===
        // Parintele nu face altceva decat sa astepte (sa nu se inchida)
        printf("[parinte %d] Fiii au fost creati. Astept...\n", getpid());
        // Asteapta ca toti fiii sa se termine (nu se va intampla curand)
        while (wait(NULL) > 0)
            ;
        close(sd);
        printf("[parinte] Toti fiii s-au terminat. Serverul se inchide.\n");
    }
    else if (pid == 0)
    {
        printf("[fiu %d] Pornit. Asteptam la portul %d...\n", getpid(), PORT);
        /* Toti fiii concureaza pentru a accepta conexiuni pe acelasi socket 'sd' */
        while (1)
        {
            int client;
            int length = sizeof(from);
            char msg[100];
            char msgrasp[100] = " ";

            fflush(stdout);

            /* acceptam un client (blocant PENTRU ACEST FIU) */
            client = accept(sd, (struct sockaddr *)&from, &length);

            if (client < 0)
            {
                perror("[fiu] Eroare la accept().\n");
                continue;
            }
            printf("[fiu %d] Am preluat un client!\n", getpid());
            bzero(msg, 100);
            /* citirea mesajului */
            if (read(client, msg, 100) <= 0)
            {
                perror("[fiu] Eroare la read() de la client.\n");
                close(client);
                continue; /* Gata cu clientul, asteapta altul */
            }
            printf("[fiu %d] Mesajul a fost receptionat...%s\n", getpid(), msg);
            /*pregatim mesajul de raspuns */
            bzero(msgrasp, 100);
            strcat(msgrasp, "Hello ");
            strcat(msgrasp, msg);
            printf("[fiu %d] Trimitem mesajul inapoi...%s\n", getpid(), msgrasp);
            /* returnam mesajul clientului */
            if (write(client, msgrasp, 100) <= 0)
            {
                perror("[fiu] Eroare la write() catre client.\n");
                close(client);
                continue; /* Gata cu clientul, asteapta altul */
            }
            else
                printf("[fiu %d] Mesajul a fost transmis cu succes.\n", getpid());
            /* am terminat cu acest client, inchidem conexiunea */
            close(client);
        } /* sfarsit while(1) al fiului */
    } /* sfarsit logica fiu */
    return 0;
} /* main */
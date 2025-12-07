
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

extern int errno;

#define PORT 2024
#define NR_CHILDREN 5

int main()
{
    pid_t pid;
    int i;
    int sd;                    // the file descriptor for the socket
    struct sockaddr_in server; // the server struct
    struct sockaddr_in from;   // the client struct

    // creating the socket
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("there is an error at making the socket...try again...\n");
        return errno;
    }

    // making all the connections to the server, putting the adress and all that

    server.sin_family = AF_INET; // af_inet stands for adress family internet
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    // the sin_addr is a struct but it can differ from system to system, but surely it has the s_addr variable
    // so like, bind to all available ip on this machine
    // for specific i would call
    // server.sin_adrr.s_addr=inet_addr("192.168.1.100");
    // here basically it says that the adress could be aboslutely anything
    server.sin_port = htons(PORT);
    // htons=host to network short
    // htonl=host to network long
    // both serve to make the format network byte order

    // now i need to do the bind,so i can connect the fd of the socket to that of the server

    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("eroare la bind, ceva nu merge bine.\n");
        return errno;
    }

    if (listen(sd, 10) == -1)
    {
        perror("too many clients are trying to connect at the same time...\n");
        return errno;
    }

    printf("[parinte %d] Serverul porneste. Se creeaza %d fii (workers)...\n", getpid(), NR_CHILDREN);

    for (i = 0; i < NR_CHILDREN; i++)
    {
        pid = fork();

        if (pid < 0)
            perror("[parinte] Eroare la fork().\n");

        if (pid == 0)
            break;
    }

    if (pid == 0)
    {
        // inseamna ca suntem in procesul parinte, deci trebuie mainly doar sa astept ca procsele copil sa termine
    }
    else
    {
        // sunt in procesul fiu
        printf("sunt in copilul care are pid-ul cu numarul: %d", getpid());

        while (1)
        {
            // trebuie sa fac aici procesul din copil, sa citesc mesajul, si sa accept conexiunea
            char msj[100];
            char msjrsp[100] = " "; // il facem gol la inceput
            int length = sizeof(from);

            int client;
            if ((client = accept(sd, (struct sockaddr *)&from, &length)) == -1)
            {
                perror("eroare la accept..");
                continue;
                // we go to the next one basically
            }

            // now we have on the client the new fd
            if (read(client, msj, strlen(msj)) == -1)
            {
                perror("eroare la read");
                close(client);
                continue;
            }
            printf("[fiu %d] Mesajul a fost receptionat...%s\n", getpid(), msj);

            bzero(msjrsp, 100);
            strcat(msjrsp, "Hello ");
            strcat(msjrsp, msj);

            if (write(client, msjrsp, 100) <= 0)
            {
                perror("[fiu] Eroare la write() catre client.\n");
                close(client);
                continue; /* Gata cu clientul, asteapta altul */
            }
            else
                printf("[fiu %d] Mesajul a fost transmis cu succes.\n", getpid());
            /* am terminat cu acest client, inchidem conexiunea */
            close(client);
        }
    }
}
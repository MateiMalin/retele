// this is the server
// so the server needs to have 2 structs
// serverul primeste un numar, il incrementeaza si scrie numarul ala si numarul de ordine al clientului
// care o scris msj

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
    struct sockaddr_in server; // informatiile pentru server, where i am listening
    struct sockaddr_in from;   // from where i get information
    char msg[100];             // msg de la client
    char msgback[100] = " ";   // mesajul catre client
    int sd;
    // folosim af_inet pentru a arata ca folosim un socket pt comunicare intre servere

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server]Eroare la socket().\n");
        return errno;
    }

    // inainte sa scriem in structurile de date, pentru safety dam clear la memorie
    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    // trebuie sa umplem structura de server cu informatiile necesare
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
}
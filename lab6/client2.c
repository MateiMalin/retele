#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

extern int errno;
int port;

int main(int argc, char *argv[])
{
    int sd;
    struct sockaddr_in server;
    char filename[100];
    char content[1000];
    char response[100];

    if (argc != 3)
    {
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    port = atoi(argv[2]);

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Eroare la socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Eroare la connect().\n");
        return errno;
    }

    // Citire nume fișier
    printf("[client]Introduceti numele fisierului: ");
    fflush(stdout);
    if (fgets(filename, sizeof(filename), stdin) == NULL)
    {
        perror("[client]Eroare la citirea numelui fisierului.\n");
        close(sd);
        return -1;
    }

    // Eliminăm newline-ul
    size_t len = strlen(filename);
    if (len > 0 && filename[len - 1] == '\n')
    {
        filename[len - 1] = '\0';
    }

    // Citire conținut fișier
    printf("[client]Introduceti continutul pentru fisier: ");
    fflush(stdout);
    if (fgets(content, sizeof(content), stdin) == NULL)
    {
        perror("[client]Eroare la citirea continutului.\n");
        close(sd);
        return -1;
    }

    // Eliminăm newline-ul din conținut (opțional)
    len = strlen(content);
    if (len > 0 && content[len - 1] == '\n')
    {
        content[len - 1] = '\0';
    }

    printf("[client]Trimit nume fisier: '%s'\n", filename);
    printf("[client]Trimit continut: '%s'\n", content);

    // Trimitem mai întâi numele fișierului
    if (write(sd, filename, 100) <= 0)
    {
        perror("[client]Eroare la write() pentru nume fisier.\n");
        close(sd);
        return errno;
    }

    // Apoi trimitem conținutul
    if (write(sd, content, 1000) <= 0)
    {
        perror("[client]Eroare la write() pentru continut.\n");
        close(sd);
        return errno;
    }

    // Primim răspunsul de la server
    bzero(response, 100);
    if (read(sd, response, 100) < 0)
    {
        perror("[client]Eroare la read() de la server.\n");
        close(sd);
        return errno;
    }

    printf("[client]Raspuns de la server: %s\n", response);

    close(sd);
    return 0;
}
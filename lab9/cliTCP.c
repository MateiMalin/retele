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
char my_id[32];
float current_lat = 0.0;
float current_long = 0.0;
int current_speed = 0;
int is_initialized = 0;

// argumentele o sa fie domeniu, port si id-ul vehiculului
int main(int argc, char *argv[])
{
    int sd;                    // descriptorul de socket
    struct sockaddr_in server; // structura folosita pentru conectare
    char msg[100];             // mesajul trimis

    /* exista toate argumentele in linia de comanda? */
    if (argc != 4)
    {
        printf("[client] Sintaxa: %s <adresa_server> <port> <id-masina>\n", argv[0]);
        return -1;
    }

    /* stabilim portul */
    port = atoi(argv[2]);
    strcpy(my_id, argv[3]);

    /* cream socketul */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[client] Eroare la socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    /* ne conectam la server */
    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Eroare la connect().\n");
        return errno;
    }

    bzero(msg, 100);
    printf("[client]Introduceti datele de conectare ale masinii: <port> <domeniu> <id_masina>");
    fflush(stdout);
    read(0, msg, 100);

    /* trimiterea mesajului la server */
    if (write(sd, msg, 100) <= 0)
    {
        perror("[client]Eroare la write() spre server.\n");
        return errno;
    }

    /* citirea raspunsului dat de server
       (apel blocant pina cind serverul raspunde) */
    if (read(sd, msg, 100) < 0)
    {
        perror("[client]Eroare la read() de la server.\n");
        return errno;
    }
    /* afisam mesajul primit */
    printf("[client]Mesajul primit este: %s\n", msg);

    /* inchidem conexiunea, am terminat */
    close(sd);
}

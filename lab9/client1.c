/* cliTCP.c - Exemplu de client TCP, modificat pentru chat/broadcast
   Clientul poate trimite mesaje de la tastatura si primeste mesaje de la server (broadcast)
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h> // Necesara pentru select()
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>

#define MAX_MSG 100 // Trebuie sa fie la fel ca pe server

extern int errno;

int port;

int main(int argc, char *argv[])
{
    int sd;                    // descriptorul de socket
    struct sockaddr_in server; // structura folosita pentru conectare
    char msg_buffer[MAX_MSG];  // mesajul trimis/primit
    int nfds;                  // numarul maxim de descriptori
    fd_set readfds;            // multimea descriptorilor de citire

    /* exista toate argumentele in linia de comanda? */
    if (argc != 3)
    {
        printf("[client] Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    /* stabilim portul */
    port = atoi(argv[2]);

    /* cream socketul */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[client] Eroare la socket().\n");
        return errno;
    }

    /* umplem structura folosita pentru realizarea conexiunii cu serverul */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    /* ne conectam la server */
    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Eroare la connect().\n");
        return errno;
    }

    printf("[client] Conectat la server. Puteti trimite mesaje.\n");
    printf("Introduceti mesajul (si apasati ENTER): ");
    fflush(stdout);

    // Initializam multimea de descriptori
    FD_ZERO(&readfds);

    // Numarul maxim de descriptori va fi cel mai mare dintre 0 (stdin) si sd
    nfds = (0 > sd ? 0 : sd);

    /* Bucla principala de chat */
    while (1)
    {
        // 1. Pregatim setul de descriptori pentru select()
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);  // Monitorizam tastatura (Standard Input)
        FD_SET(sd, &readfds); // Monitorizam socketul (mesaje de la server)

        // 2. Apelul select()
        // Punem timeout NULL: asteptam la infinit (blocant) pana se intampla ceva
        if (select(nfds + 1, &readfds, NULL, NULL, NULL) < 0)
        {
            perror("[client] Eroare la select().\n");
            break;
        }

        // 3. Verificam Sursa de Eveniment

        /* A. EVENIMENT PE SOCKET (Mesaj Primit de la Server/Broadcast) */
        if (FD_ISSET(sd, &readfds))
        {
            bzero(msg_buffer, MAX_MSG);
            int bytes_received = read(sd, msg_buffer, MAX_MSG - 1);

            if (bytes_received <= 0)
            {
                // Conexiunea s-a inchis sau eroare
                printf("\n[client] Serverul s-a deconectat. Iesire.\n");
                break;
            }
            // Terminare cu null-byte
            msg_buffer[bytes_received] = '\0';

            // Afisam mesajul primit
            printf("\n--> %s", msg_buffer);

            // Reafisam promptul pentru a nu fi acoperit de mesaj
            printf("Introduceti mesajul (si apasati ENTER): ");
            fflush(stdout);
        }

        /* B. EVENIMENT PE TASTATURA (Mesaj de trimis) */
        if (FD_ISSET(0, &readfds))
        {
            bzero(msg_buffer, MAX_MSG);
            // Citim de la tastatura
            if (read(0, msg_buffer, MAX_MSG - 1) < 0)
            {
                perror("[client] Eroare la citirea de la tastatura.\n");
                break;
            }

            // Daca utilizatorul a apasat doar ENTER, nu trimitem
            if (strlen(msg_buffer) <= 1)
            {
                printf("Introduceti mesajul (si apasati ENTER): ");
                fflush(stdout);
                continue;
            }

            // Trimiterea mesajului la server
            if (write(sd, msg_buffer, strlen(msg_buffer)) <= 0)
            {
                perror("[client] Eroare la write() spre server.\n");
                break;
            }

            // Reafisam promptul
            printf("Introduceti mesajul (si apasati ENTER): ");
            fflush(stdout);
        }
    } /* end while */

    /* inchidem conexiunea */
    close(sd);
    return 0;
}
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>

#define PORT 2728
#define MAX_MSG 100

extern int errno;

/* functie de convertire a adresei IP a clientului in sir de caractere */
char *conv_addr(struct sockaddr_in address)
{
    static char str[25];
    char port[7];

    /* adresa IP a clientului */
    strcpy(str, inet_ntoa(address.sin_addr));
    /* portul utilizat de client */
    bzero(port, 7);
    sprintf(port, ":%d", ntohs(address.sin_port));
    strcat(str, port);
    return (str);
}

int read_client_message(int fd, char *buffer)
{
    int bytes = 0;
    bzero(buffer, MAX_MSG);

    bytes = read(fd, buffer, MAX_MSG);

    if (bytes < 0)
    {
        perror("Eroare la citirea din client");
        return -1;
    }
    if (bytes > 0)
        buffer[bytes] = '\0';

    return bytes;
}

int main()
{
    struct sockaddr_in server; /* structurile pentru server si clienti */
    struct sockaddr_in from;
    fd_set readfds;    /* multimea descriptorilor de citire */
    fd_set actfds;     /* multimea descriptorilor activi */
    struct timeval tv; /* structura de timp pentru select() */
    int sd, client;    /* descriptori de socket */
    int optval = 1;    /* optiune folosita pentru setsockopt()*/
    int fd;            /* descriptor folosit pentru
                      parcurgerea listelor de descriptori */
    int nfds;          /* numarul maxim de descriptori */
    int len;           /* lungimea structurii sockaddr_in */

    // Variabile adaugate pentru broadcast:
    char client_msg_buffer[MAX_MSG]; // Buffer pentru a stoca mesajul citit
    int bytes_read = 0;
    int sender_fd = -1; // Retine descriptorul clientului care a trimis mesajul

    /* creare socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server] Eroare la socket().\n");
        return errno;
    }

    /*setam pentru socket optiunea SO_REUSEADDR */
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    /* pregatim structurile de date */
    bzero(&server, sizeof(server));

    /* umplem structura folosita de server */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    /* atasam socketul */
    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server] Eroare la bind().\n");
        return errno;
    }

    /* punem serverul sa asculte daca vin clienti sa se conecteze */
    if (listen(sd, 5) == -1)
    {
        perror("[server] Eroare la listen().\n");
        return errno;
    }

    FD_ZERO(&actfds); // golim setul actual de file descriptors
    FD_SET(sd, &actfds);

    nfds = sd; // setare valoare maxima file descriptors

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    printf("[server] Asteptam la portul %d...\n", PORT);
    fflush(stdout);

    /* servim in mod concurent (!) clientii... */
    while (1)
    {
        /* ajustam multimea descriptorilor activi (efectiv utilizati) */
        bcopy((char *)&actfds, (char *)&readfds, sizeof(readfds));
        // facem copie pentru ca select altereaza file decriptors, avem nevoie
        // de actfds intact
        /* reinitializam tv inainte de select(), deoarece select o poate modifica */
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        // Resetam sender_fd la inceputul fiecarei iteratii
        sender_fd = -1;

        // select are rolul de a astepta si a intercepta file descriptors care sa fie gata
        // cele 2 null-uri sunt pentru file descirptors scriere si citire
        if (select(nfds + 1, &readfds, NULL, NULL, &tv) < 0)
        {
            perror("[server] Eroare la select().\n");
            return errno;
        }

        /* I. GESTIONARE CONEXIUNI NOI (sd) */
        if (FD_ISSET(sd, &readfds))
        {
            /* pregatirea structurii client */
            len = sizeof(from);
            bzero(&from, sizeof(from));

            /* a venit un client, acceptam conexiunea */
            client = accept(sd, (struct sockaddr *)&from, &len);

            /* eroare la acceptarea conexiunii de la un client */
            if (client < 0)
            {
                perror("[server] Eroare la accept().\n");
                continue;
            }

            if (nfds < client) /* ajusteaza valoarea maximului */
                nfds = client;

            /* includem in lista de descriptori activi si acest socket */
            FD_SET(client, &actfds);

            printf("[server] S-a conectat clientul cu descriptorul %d, de la adresa %s.\n", client, conv_addr(from));
            fflush(stdout);
        }

        /* II. GESTIONARE MESAJ (read) */
        for (fd = 0; fd <= nfds; fd++)
        {
            /* Este un socket de citire pregatit si nu este socketul serverului? */
            if (fd != sd && FD_ISSET(fd, &readfds))
            {
                // Citim mesajul de la clientul 'fd' (folosind functia read_client_message)
                bytes_read = read_client_message(fd, client_msg_buffer);

                if (bytes_read == 0 || bytes_read == -1)
                {
                    // Deconectare sau eroare
                    printf("[server] S-a deconectat clientul cu descriptorul %d.\n", fd);
                    fflush(stdout);
                    close(fd);           /* inchidem conexiunea cu clientul */
                    FD_CLR(fd, &actfds); /* scoatem si din multime */

                    // Ajustam nfds daca a fost deconectat socketul maxim
                    if (fd == nfds)
                    {
                        while (nfds > 0 && !FD_ISSET(nfds, &actfds))
                            nfds--;
                    }
                }
                else // bytes_read > 0 (Mesaj primit)
                {
                    // Mesaj primit. Retinem sursa si mergem la broadcast.
                    sender_fd = fd;
                    printf("[server] Mesaj primit de la clientul %d: %s", fd, client_msg_buffer);
                    goto broadcast_section;
                }
            }
        }

        // Daca nu s-a citit niciun mesaj, continuam (fara broadcast)
        continue;

        /* III. BROADCAST (write) */

    broadcast_section:

        // Construim mesajul de broadcast final (include sursa)
        char broadcast_msg[MAX_MSG + 20]; // 20 caractere extra pentru prefix
        bzero(broadcast_msg, sizeof(broadcast_msg));
        sprintf(broadcast_msg, "[Client %d]: %s", sender_fd, client_msg_buffer);

        printf("[server] Se trimite broadcast catre toti, EXCEPTAND Clientul %d: %s", sender_fd, broadcast_msg);

        // Bucla pentru a trimite mesajul tuturor clientilor activi
        for (int i = 0; i <= nfds; i++)
        {
            // CONDITIE EXCLUSIVA: Trimite mesajul doar daca:
            // 1. Descriptorul 'i' este activ (FD_ISSET(i, &actfds)).
            // 2. Descriptorul 'i' NU este socketul server (i != sd).
            // 3. Descriptorul 'i' NU este clientul care a trimis mesajul (i != sender_fd).
            if (i != sd && i != sender_fd && FD_ISSET(i, &actfds))
            {
                if (write(i, broadcast_msg, strlen(broadcast_msg)) < 0)
                {
                    perror("[server] Eroare la write() in broadcast.\n");
                }
            }
        }

        // Resetam bufferul de mesaj si contorul de octeti cititi
        bzero(client_msg_buffer, MAX_MSG);
        bytes_read = 0;

    } /* while */

    return 0; // Adaugat pentru conformitatea functiei main
} /* main */
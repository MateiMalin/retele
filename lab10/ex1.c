/* servTCPIt.c - Exemplu de server UDP
   Asteapta un nume de la clienti; intoarce clientului sirul
   cautare adresa dupa nume intr-un struct hardcodat

   Autor: Lenuta Alboaie  <adria@info.uaic.ro> (c)
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>

/* portul folosit */
#define PORT 2728
#define MAX_DOMAIN_LEN 50 // Mărit pentru nume mai lungi
#define MAX_IP_LEN 16     // Suficient pentru IPv4
#define MAX_RSP_LEN 150   // Bufferul de raspuns
/* codul de eroare returnat de anumite apeluri */
extern int errno;

struct dns_record
{
    char domain[MAX_DOMAIN_LEN];
    char ip_address[MAX_IP_LEN];
};

// Definitie globala, vizibila in tot fisierul
struct dns_record local_records[] =
    {
        {"uaic.ro", "193.23.30.5"},
        {"server.local", "10.0.0.100"},
        {"google.com", "216.58.207.14"},
        {"yahoo.com", "74.6.143.25"},
};

const int NUM_RECORDS = sizeof(local_records) / sizeof(local_records[0]);

char *get_ip(char *domain_name)
{
    // local_records si NUM_RECORDS sunt globale, nu avem nevoie de 'extern'
    for (int i = 0; i < NUM_RECORDS; i++)
    {
        // Compara numele primit cu numele din baza de date
        if (strcmp(local_records[i].domain, domain_name) == 0)
        {
            return local_records[i].ip_address;
        }
    }

    return "Eroare, nu s-a gasit o adresa ip pentru domeniul oferit";
}

int main()
{
    struct sockaddr_in server; // structura folosita de server
    struct sockaddr_in client;
    char msg[MAX_RSP_LEN];     // mesajul primit de la client
    char msgrasp[MAX_RSP_LEN]; // mesaj de raspuns pentru client
    int sd;                    // descriptorul de socket

    /* crearea unui socket */
    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("[server]Eroare la socket().\n");
        return errno;
    }

    /* pregatirea structurilor de date */
    bzero(&server, sizeof(server));
    bzero(&client, sizeof(client));

    /* umplem structura folosita de server */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    /* atasam socketul */
    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server]Eroare la bind().\n");
        return errno;
    }

    /* servim in mod iterativ clientii... */
    while (1)
    {
        int msglen;
        int length = sizeof(client);

        printf("[server]Astept la portul %d...\n", PORT);
        fflush(stdout);

        bzero(msg, MAX_RSP_LEN);

        /* citirea mesajului primit de la client */
        if ((msglen = recvfrom(sd, msg, MAX_RSP_LEN - 1, 0, (struct sockaddr *)&client, &length)) <= 0)
        {
            perror("[server]Eroare la recvfrom() de la client.\n");
            return errno;
        }

        // Asigurăm că mesajul este un șir terminat corect
        msg[msglen] = '\0';

        // 1. Elimina newline-ul (daca inputul vine de la tastatura clientului)
        msg[strcspn(msg, "\n")] = 0;

        printf("[server]Mesajul a fost receptionat: %s\n", msg);

        /*pregatim mesajul de raspuns */
        bzero(msgrasp, MAX_RSP_LEN); // Resetează bufferul de răspuns

        char *res_ip = get_ip(msg);

        // Buffer temporar pentru a construi răspunsul
        char temp_buffer[MAX_RSP_LEN];
        bzero(temp_buffer, MAX_RSP_LEN);

        // Cazul 1: Eroare
        if (strcmp(res_ip, "Eroare, nu s-a gasit o adresa ip pentru domeniul oferit") == 0)
        {
            sprintf(temp_buffer, "DNS: %s", res_ip);
        }
        // Cazul 2: Succes
        else
        {
            sprintf(temp_buffer, "DNS: Adresa IP pentru domeniul %s este: %s", msg, res_ip);
        }

        // 5. Adăugarea Adresei IP a Clientului (Folosind structura 'client' completată de recvfrom)
        strcat(temp_buffer, " | Sursa clientului (IP:Port): ");

        // inet_ntoa returneaza char* catre adresa IP (string)
        char client_ip_port[40];
        sprintf(client_ip_port, "%s:%d", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        strcat(temp_buffer, client_ip_port);

        // Copiază răspunsul final în msgrasp
        strcpy(msgrasp, temp_buffer);

        printf("[server]Trimitem mesajul inapoi: %s\n", msgrasp);

        /* returnam mesajul clientului */
        // Trimite înapoi lungimea reală a șirului de caractere, nu 100
        if (sendto(sd, msgrasp, strlen(msgrasp) + 1, 0, (struct sockaddr *)&client, length) <= 0)
        {
            perror("[server]Eroare la sendto() catre client.\n");
            continue;
        }
        else
            printf("[server]Mesajul a fost trasmis cu succes.\n");

    } /* while */
} /* main */
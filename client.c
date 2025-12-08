#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define BUFFER_SIZE 1024

extern int errno;

int port;
char my_id[32];
float current_lat = 0.0;
float current_long = 0.0;
int current_speed = 0;
int is_initialized = 0;

void show_prompt()
{
    printf("\r[%s] Comanda > ", my_id);
    fflush(stdout);
}

// argumentele o sa fie domeniu, port si id-ul vehiculului
int main(int argc, char *argv[])
{
    int sd;                         // descriptorul de socket
    struct sockaddr_in server;      // structura folosita pentru conectare
    char buffer[BUFFER_SIZE];       // mesajul trimis
    char input_buffer[BUFFER_SIZE]; // mesaju de la tastatura

    fd_set readfds;
    struct timeval tv;
    int ret;
    /* exista toate argumentele in linia de comanda? */
    if (argc != 4)
    {
        printf("[client] Sintaxa: %s <adresa_server> <port> <id-masina>\n", argv[0]);
        return -1;
    }

    /* stabilim portul */
    port = atoi(argv[2]);
    strcpy(my_id, argv[3]);
    srand(time(NULL));

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

    // trimitem imediat connect la server ca sa inregistram o masina
    snprintf(buffer, sizeof(buffer), "{\"cmd\":\"CONNECT\", \"id\":\"%s\"}", my_id);
    write(sd, buffer, strlen(buffer));

    // --- INTERFATA UTILIZATOR (Simulata in Terminal) ---
    printf("\n=================================================\n");
    printf("   TRAFFIC MONITOR CLIENT - %s\n", my_id);
    printf("=================================================\n");
    printf("1. Initializare:  set <lat> <long> <viteza>\n");
    printf("   Exemplu:       set 47.16 27.58 60\n");
    printf("2. Raportare:     report\n");
    printf("3. Abonare:       sub / unsub\n");
    printf("4. Iesire:        exit\n");
    printf("=================================================\n");
    printf("Nota: Dupa 'set', datele se trimit automat la fiecare 5 secunde.\n\n");

    show_prompt();

    // accept doar accepta o conexiune cand esti conectat cu un file descriptor
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);  // Monitorizam Tastatura (stdin)
        FD_SET(sd, &readfds); // Monitorizam Serverul (socket)

        tv.tv_sec = 10;
        tv.tv_usec = 0;

        ret = select(sd + 1, &readfds, NULL, NULL, &tv);
        // select e ala care o sa si modifice readfds
        // ret e return value de la select si imi spune in ce stadiu sunt
        if (ret == -1)
        {
            perror("Eroare la select");
            break;
        }

        if (ret == 0)
        {
            // inseamna ca au trecut alea 60 de secunde si trebuie sa trimit telemetry din nou
            if (is_initialized)
            {
                current_speed += (rand() % 5) - 2;
                if (current_speed < 0)
                    current_speed = 0;
                current_lat += 0.1;
                current_long += 0.1;

                snprintf(buffer, sizeof(buffer), "{\"cmd\":\"TELEMETRY\", \"lat\":\"%.4f\", \"long\":\"%.4f\", \"speed\":\"%d\", \"id\":\"%s\"}", current_lat, current_long, current_speed, my_id);

                write(sd, buffer, strlen(buffer));
            }
            continue;
        }

        // daca primim de la server mesaje(alerte sau confirmari)
        // verificam daca socket-ul e inca on
        if (FD_ISSET(sd, &readfds))
        {
            memset(buffer, 0, sizeof(buffer));
            int bytes = read(sd, buffer, sizeof(buffer) - 1);
            buffer[bytes] = '\0';

            if (bytes <= 0)
            {
                printf("\n[SISTEM] Serverul s-a inchis. Iesire...\n");
                break;
            }

            if (strstr(buffer, "ALERT") && strstr(buffer, "depasit"))
            {
                int new_limit = 0;

                char *p = strstr(buffer, "limita de");
                if (p)
                {
                    sscanf(p + 10, "%d", &new_limit);

                    if (new_limit > 0 && current_speed > new_limit)
                    {
                        current_speed = new_limit;
                        printf("\n[AUTO-PILOT] Viteza redusa automat la %d km/h conform alertei.\n", current_speed);
                    }
                }
            }
            else
            {
                printf("\n[Server ACK]: %s\n", buffer);
            }
            show_prompt();
        }

        if (FD_ISSET(0, &readfds))
        {
            // verificam daca citim ceva de la tastatura
            fgets(input_buffer, sizeof(input_buffer), stdin);
            char *aux = strchr(input_buffer, '\n');
            if (aux)
                *aux = '\0';

            if (strcmp(input_buffer, "exit") == 0)
            {
                break;
            }
            else if (strncmp(input_buffer, "set", 3) == 0)
            {
                // avem comanda de set, deci trebuie sa mai citim de la tastatura si parametrii
                if (sscanf(input_buffer, "set %f %f %d", &current_lat, &current_long, &current_speed) == 3)
                {
                    is_initialized = 1;

                    printf("[CLIENT] Locatie setata. Pornire telemetrie...\n");

                    snprintf(buffer, sizeof(buffer), "{\"cmd\":\"TELEMETRY\", \"lat\":\"%.4f\", \"long\":\"%.4f\", \"speed\":\"%d\", \"id\":\"%s\"}", current_lat, current_long, current_speed, my_id);
                    write(sd, buffer, strlen(buffer));
                }
                else
                {
                    printf("[EROARE] Format gresit. Foloseste: set 47.16 27.58 60\n");
                }
            }
            else if (strncmp(input_buffer, "report", 6) == 0)
            {
                // scriem id-ul masiniim, ca sa ne luam dupa asta pentru locatia strazii
                snprintf(buffer, sizeof(buffer), "{\"cmd\":\"REPORT\", \"type\":\"ACCIDENT\", \"id\":\"%s\"}", my_id);
                write(sd, buffer, strlen(buffer));
            }
            else if (strcmp(input_buffer, "sub") == 0)
            {
                snprintf(buffer, sizeof(buffer), "{\"cmd\":\"SUBSCRIBE\", \"id\":\"%s\"}", my_id);
                write(sd, buffer, strlen(buffer));
            }
            else if (strcmp(input_buffer, "unsub") == 0)
            {
                snprintf(buffer, sizeof(buffer), "{\"cmd\":\"UNSUBSCRIBE\", \"id\":\"%s\"}", my_id);
                write(sd, buffer, strlen(buffer));
            }
            else
            {
                printf("[CLIENT] Comanda necunoscuta.\n");
            }
            show_prompt();
        }
    }

    close(sd);
    return 0;
}

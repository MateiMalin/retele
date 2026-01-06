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

#define STEP_LAT 0.5
#define STEP_LONG 0.5

extern int errno;

int port;
char my_id[32];
float current_lat = 0.0;
float current_long = 0.0;
int current_speed = 0;
int is_initialized = 0;

char current_street_name[64] = "Unknown";

int current_axis = 0;      // axa pe care ma aflu
int current_direction = 1; // directia , spre plus sau minus

float step_lat = 0.0;
float step_long = 0.0;

const float telemtry_interval_seconds = 10.0;

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

    float prev_lat = 0.0;
    float prev_long = 0.0;

    fd_set readfds;
    struct timeval tv;
    int ret;
    if (argc != 4)
    {
        printf("[CLIENT] Sintaxa: %s <adresa_server> <port> <id-masina>\n", argv[0]);
        return -1;
    }

    port = atoi(argv[2]);
    strcpy(my_id, argv[3]);
    // cream seed-ul random dupa ora
    srand(time(NULL));

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[client] Eroare la socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    // ne conectam prin sd si facem handshake tcp
    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Eroare la connect().\n");
        return errno;
    }

    // trimitem imediat connect la server ca sa inregistram o masina
    snprintf(buffer, sizeof(buffer), "{\"cmd\":\"CONNECT\", \"id\":\"%s\"}", my_id);
    write(sd, buffer, strlen(buffer));

    printf("   TRAFFIC MONITOR CLIENT - %s\n", my_id);
    printf("1. Initializare:  set <lat> <long> <viteza>\n");
    printf("   Exemplu:       set 47.16 27.58 60\n");
    printf("2. Raportare:     report\n");
    printf("3. Abonare:       sub / unsub\n");
    printf("4. Iesire:        exit\n");

    show_prompt();

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);
        FD_SET(sd, &readfds);

        tv.tv_sec = (long)telemtry_interval_seconds;
        tv.tv_usec = 0;

        ret = select(sd + 1, &readfds, NULL, NULL, &tv);
        // select e ala care o sa si modifice readfds
        // ret e return value de la select si imi spune in ce stadiu sunt
        // the number of file descriptors care sunt gata de citire
        // ret se schimba doar daca am alte fd-uri
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
                // back up in caz ca am iesit de pe harta
                prev_lat = current_lat;
                prev_long = current_long;

                int fluctuation = (rand() % 5) - 2;
                current_speed += fluctuation;

                if (current_speed < 0)
                    current_speed = 0;
                if (current_speed > 200)
                    current_speed = 200;

                // calculam distanta ca viteza * timp
                float time_hours = telemtry_interval_seconds / 3600.0;
                float multiplier = 50; // for example purposes
                float dist_km = current_speed * multiplier * time_hours;

                // selectam pe ce axa sa miscam
                if (current_axis == 0)
                {
                    current_lat += dist_km * STEP_LAT * current_direction;
                }
                else
                {
                    current_long += dist_km * STEP_LONG * current_direction;
                }

                // ii trimit telemetry din nou
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

            if (strstr(buffer, "\"axis\":"))
            {
                int new_axis = -1;
                char *p = strstr(buffer, "\"axis\":");
                if (p)
                    sscanf(p + 7, "%d", &new_axis);

                int server_limit = 0;
                char *p_limit = strstr(buffer, "\"limit\":");
                if (p_limit)
                    sscanf(p_limit + 8, "%d", &server_limit);

                char new_street_name[64];
                char *p_street = strstr(buffer, "\"street\":\"");
                if (p_street)
                    sscanf(p_street + 10, "%[^\"]", new_street_name);
                else
                    strcpy(new_street_name, "Unknown");

                char raw_msg[256];
                char street_state[100] = "Normal";

                char *p_msg = strstr(buffer, "\"msg\":\"");
                if (p_msg)
                {
                    sscanf(p_msg + 7, "%[^\"]", raw_msg);

                    char *first_pipe = strchr(raw_msg, '|');
                    if (first_pipe)
                    {
                        char *second_pipe = strchr(first_pipe + 1, '|');
                        if (second_pipe)
                        {
                            int len = second_pipe - (first_pipe + 1);
                            strncpy(street_state, first_pipe + 1, len);
                            street_state[len] = '\0';
                        }
                        else
                        {
                            strcpy(street_state, first_pipe + 1);
                        }
                    }
                }

                if (strcmp(new_street_name, "Unknown") != 0)
                {
                    strcpy(current_street_name, new_street_name);
                }
                if (new_axis != -1 && new_axis != current_axis)
                {
                    current_axis = new_axis;
                }

                int brake_applied = 0;
                if (server_limit > 0 && current_speed > server_limit)
                {
                    current_speed = server_limit;
                    brake_applied = 1;
                }

                printf("\n[ BORD MASINA ]\n");
                printf("  GPS       : %.4f N, %.4f E\n", current_lat, current_long);
                printf("  Viteza    : %d km/h  %s\n", current_speed, brake_applied ? "(FRANARE AUTOMATA!)" : "");
                printf("\n");
                printf("  Strada    : %s\n", current_street_name);
                printf("  Stare     :%s\n", street_state);
                printf("  Limita    : %d km/h\n", server_limit);
            }
            else if (strstr(buffer, "UNKNOWN") || strstr(buffer, "Unknown"))
            {
                current_lat = prev_lat;
                current_long = prev_long;
                current_direction = current_direction * -1;

                if (current_axis == 0)
                    current_lat += (0.002 * current_direction);
                else
                    current_long += (0.002 * current_direction);

                printf("\n[ALERTA] !!! CAPAT DE HARTA !!! Intoarcere efectuata.\n");
            }
            else if (strstr(buffer, "ALERT"))
            {
                char alert_msg[256];
                char *p_msg = strstr(buffer, "\"msg\":\"");
                if (p_msg)
                {
                    sscanf(p_msg + 7, "%[^\"]", alert_msg);
                }
                else
                {
                    strcpy(alert_msg, "Alerta necunoscuta");
                }

                if (strstr(buffer, "depasit") || strstr(buffer, "WARNING"))
                {
                    int new_limit = 0;
                    char *p = strstr(buffer, "limita actuala e");
                    if (!p)
                        p = strstr(buffer, "limit");

                    if (p)
                        sscanf(p, "%*[^0-9]%d", &new_limit);

                    printf("\n[ALERTA VITEZA] %s\n", alert_msg);

                    if (new_limit > 0 && current_speed > new_limit)
                    {
                        current_speed = new_limit;
                        printf("[AUTO-PILOT] Viteza redusa fortat la %d km/h.\n", current_speed);
                    }
                }
                else if (strstr(buffer, "CRITIC") || strstr(buffer, "ACCIDENT"))
                {
                    printf("\n[!!! ALERTA ACCIDENT !!!] %s\n", alert_msg);

                    if (current_speed > 10)
                    {
                        current_speed = 10;
                        printf("[AUTO-PILOT] Pericol iminent! Franare de urgenta la 10 km/h.\n");
                    }
                }
                else
                {
                    printf("\n[ALERTA TRAFIC] %s\n", alert_msg);
                }
            }
            else
            {
                printf("\n[Server MSG]: %s\n", buffer);
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

                    prev_lat = current_lat;
                    prev_long = current_long;

                    // ca sa randomizam directia de mers,sa fie mai realistic
                    current_direction = (rand() % 2 == 0) ? 1 : -1;

                    int fluctuation = (rand() % 5) - 2;

                    current_speed += fluctuation;

                    if (current_speed < 0)
                        current_speed = 0;
                    if (current_speed > 200)
                        current_speed = 200;
                    printf("[CLIENT] Locatie setata. Sincronizare cu serverul...\n");

                    // daca am comanda set, o sa ii trimit un telemetry
                    snprintf(buffer, sizeof(buffer), "{\"cmd\":\"TELEMETRY\", \"lat\":\"%.4f\", \"long\":\"%.4f\", \"speed\":\"%d\", \"id\":\"%s\"}", current_lat, current_long, current_speed, my_id);
                    write(sd, buffer, strlen(buffer));

                    // asteptare blocanta, handshake
                    memset(buffer, 0, sizeof(buffer));

                    int bytes = read(sd, buffer, sizeof(buffer) - 1);

                    if (strstr(buffer, "\"axis\":"))
                    {
                        int new_axis = -1;
                        char *p = strstr(buffer, "\"axis\":");
                        sscanf(p + 7, "%d", &new_axis);
                        if (new_axis != -1)
                            current_axis = new_axis;
                    }
                    char *msg = strstr(buffer, "\"msg\":\"");
                    if (msg)
                    {
                        char text[256];
                        sscanf(msg + 7, "%[^\"]", text);
                        printf("\n[SERVER INITIAL] %s\n", text);
                    }
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

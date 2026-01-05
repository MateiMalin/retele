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

float prev_lat = 0.0;
float prev_long = 0.0;

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

    fd_set readfds;
    struct timeval tv;
    int ret;
    if (argc != 4)
    {
        printf("[CLIENT] Sintaxa: %s <adresa_server> <port> <id-masina>\n", argv[0]);
        return -1;
    }

    /* stabilim portul */
    port = atoi(argv[2]);
    strcpy(my_id, argv[3]);
    // cream seed-ul random dupa ora
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
        FD_SET(0, &readfds);  // monitorizam tastatura (stdin)
        FD_SET(sd, &readfds); // monitorizam serverul (socket)

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

                printf("[AUTO] Moving... Lat: %.4f Long: %.4f \n", current_lat, current_long);
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
                sscanf(p + 7, "%d", &new_axis);

                int server_limit = 0;
                char *p_limit = strstr(buffer, "\"limit\":");
                if (p_limit)
                {
                    sscanf(p_limit + 8, "%d", &server_limit);
                }

                // vedem daca strada pe care intram este noua sau nu
                char new_street_name[64];
                char *p_street = strstr(buffer, "\"street\":\"");
                if (p_street)
                {
                    sscanf(p_street + 10, "%[^\"]", new_street_name);
                }
                else
                {
                    strcpy(new_street_name, "UNKOWN");
                }

                if (strcmp(new_street_name, current_street_name) != 0 && strcmp(new_street_name, "Unkown") != 0)
                {
                    // inseamna ca am intrat pe o strada noua
                    if (new_axis != current_axis && new_axis != -1)
                    {
                        // inseamna ca am schimbat directia
                        printf("[GPS] Viraj detecat, schimbat directia de mers..\n");
                    }
                    else
                    {
                        printf("[GPS] Continuam pe directia inainte..\n");
                    }

                    // actualizam numele strazii
                    strcpy(current_street_name, new_street_name);
                }

                if (server_limit > 0 && current_speed > server_limit)
                {
                    printf("\n[AUTO-PILOT] Limita de %d km/h detectata! Franez automat de la %d km/h.\n", server_limit, current_speed);
                    current_speed = server_limit;
                }

                char *msg = strstr(buffer, "\"msg\":\"");
                if (msg)
                {
                    char text[256];
                    sscanf(msg + 7, "%[^\"]", text);
                    printf("[INFO] %s\n", text);
                }
            }
            else if (strstr(buffer, "UNKNOWN") || strstr(buffer, "Unknown"))
            {
                // logica sa ma intorc pe strada de unde am plecat
                printf("[Alerta] Am ajuns la capatul strazii! Ma intorc...\n");

                current_lat = prev_lat;
                current_long = prev_long;

                current_direction = current_direction * -1;
            }
            else if (strstr(buffer, "ALERT") && strstr(buffer, "depasit"))
            {
                // asta merge doar daca am intrat pe strada aia ???
                // nu merge, auto pilotul nu imi spune ca am depsait viteza, cred
                int new_limit = 0;

                char *p = strstr(buffer, "limit:");
                if (p)
                {
                    sscanf(p + 7, "%d", &new_limit);

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

                    prev_lat = current_lat;
                    prev_long = current_long;

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

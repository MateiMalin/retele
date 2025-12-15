#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#define PORT 2728
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

extern int errno;

pthread_mutex_t data_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

int max_long = -2e9;
int max_lat = -2e9;

struct client_info
{
    char id[32];
    int active;           // if the car is active
    int subscription;     // if it has an ongoing subscription for weather etc etc
    float lat;            // coord strada
    float lng;            // coord strada
    char street_name[64]; // nume strada
    int speed;            // the speed
} clients[MAX_CLIENTS];

// functie ca sa scriu in json toate comenzile apelate, un history la tot ce se intampla
void log_event(const char *json_string)
{
    pthread_mutex_lock(&log_lock);

    FILE *f = fopen("server_log.json", "r+");

    if (!f)
    {
        f = fopen("server_log.json", "w");
        if (f)
        {
            fprintf(f, "[\n%s\n]", json_string);
            fclose(f);
        }
    }
    else
    {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);

        if (size > 0)
        {
            long pos = size - 1;
            int found_end_bracket = 0;

            while (pos > 0)
            {
                fseek(f, pos, SEEK_SET);
                char c = fgetc(f);
                if (c == '}')
                {
                    found_end_bracket = 1;
                    break;
                }
                pos--;
            }

            if (found_end_bracket)
            {
                fseek(f, pos + 1, SEEK_SET);
                fprintf(f, ",\n%s\n]", json_string);
            }
            else
            {
                fprintf(f, "%s\n]", json_string);
            }
        }
        else
        {
            fprintf(f, "[\n%s\n]", json_string);
        }
        fclose(f);
    }

    pthread_mutex_unlock(&log_lock);
}

void save_car_data()
{
    FILE *f = fopen("cars.json", "w");
    if (!f)
        return;

    pthread_mutex_lock(&data_lock);
    fprintf(f, "[\n");

    int first = 1;

    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active)
        {
            if (!first)
            {
                fprintf(f, ",\n");
            }
            fprintf(f, "  {\n");
            fprintf(f, "    \"id\": \"%s\",\n", clients[i].id);
            fprintf(f, "    \"lat\": %.4f,\n", clients[i].lat);
            fprintf(f, "    \"lng\": %.4f,\n", clients[i].lng);
            fprintf(f, "    \"speed\": %.1f,\n", clients[i].speed);
            fprintf(f, "    \"street\": \"%s\"\n", clients[i].street_name);
            fprintf(f, "  }");

            first = 0;
        }
    }

    fprintf(f, "\n]\n");

    fclose(f);

    pthread_mutex_unlock(&data_lock);
}

typedef struct
{
    // datele statice din json
    int id;
    char name[64];
    float lat_min, lat_max;
    float long_min, long_max;
    int default_speed_limit;

    // datele dinamice
    int current_speed_limit;
    int has_accident;
    int traffic_level;
    int car_count;

} RoadSegment;

RoadSegment city_map[100];
int map_size = 100;

// functie care sa returneze indexul unei strazi sau -1 daca
// masina nu se afla pe o strada
int find_street_index(float lat, float lng)
{
    int i;
    for (i = 0; i < map_size; i++)
    {
        if (city_map[i].id == 0)
            continue;

        if (lat >= city_map[i].lat_min && lat <= city_map[i].lat_max &&
            lng >= city_map[i].long_min && lng <= city_map[i].long_max)
        {
            return i;
        }
    }
    return -1;
}

// functie care sa alerteze masinile daca trebuie sa reduca viteza
void alert_speed(int s_index, int new_speed_limit, int alert_case, int sender_fd)
{
    char alert_msg[512];
    char s_name[63];

    if (s_index != -1)
    {
        strcpy(s_name, city_map[s_index].name);
    }
    else
    {
        strcpy(s_name, "Strada nu exista");
    }

    if (alert_case == 1 || alert_case == 2)
    {
        // s-a petrecut accident pe strada cu s_index sau e trafic
        if (s_index != -1)
            city_map[s_index].current_speed_limit = new_speed_limit;

        char *reason;
        if (alert_case == 1)
            reason = "ACCIDENT GRAV";
        else
            reason = "TRAFIC CRESCUT";

        snprintf(alert_msg, sizeof(alert_msg), "{\"cmd\":\"ALERT\", \"type\":\"CRITIC\", \"msg\":\"%s pe %s. Noua limita: %d km/h\"}\n", reason, s_name, new_speed_limit);

        // ii lueam pe toti la rand, care sunt pe aceeasi strada
        int fd;
        for (fd = 0; fd < MAX_CLIENTS; fd++)
        {
            if (clients[fd].active)
                if (strcmp(clients[fd].street_name, s_name) == 0)
                    write(fd, alert_msg, strlen(alert_msg));
        }
    }
    else if (alert_case == 3)
    {
        // doar conduce cu prea multa viteza
        snprintf(alert_msg, sizeof(alert_msg), "{\"cmd\":\"ALERT\", \"type\":\"WARNING\", \"msg\":\"Atentie %s! Ati depasit limita de %d km/h!\"}\n",
                 clients[sender_fd].id, new_speed_limit);
        write(sender_fd, alert_msg, strlen(alert_msg));
    }
}

void *client_thread(void *arg)
{
    int fd = *((int *)arg);
    free(arg);

    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    while (1)
    {
        int bytes = read(fd, buffer, sizeof(buffer) - 1);

        if (bytes <= 0)
        {
            // daca ne deconectam de la server
            printf("Clientul cu fd : %d s-a deconectat\n", fd);

            pthread_mutex_lock(&data_lock);

            int old_street = find_street_index(clients[fd].lat, clients[fd].lng);
            if (old_street != -1 && city_map[old_street].car_count > 0)
                city_map[old_street].car_count--;

            clients[fd].active = 0;
            pthread_mutex_unlock(&data_lock);

            save_car_data();

            close(fd);
            break;
        }

        buffer[bytes] = '\0';

        if (strstr(buffer, "CONNECT"))
        {
            char *ptr = strstr(buffer, "\"id\":\"");
            if (ptr)
            {
                char id[32];
                sscanf(ptr + 6, "%[^\"]", id);

                // protejam scrierea datelor noului client
                pthread_mutex_lock(&data_lock);
                strcpy(clients[fd].id, id);
                clients[fd].active = 1;
                clients[fd].lat = 0;
                clients[fd].lng = 0;
                clients[fd].speed = 0;
                clients[fd].subscription = 0;
                strcpy(clients[fd].street_name, "Unknown");
                pthread_mutex_unlock(&data_lock);

                printf("[CONNECT] S-a conectat un client nou: %s (socket fd: %d)\n", id, fd);

                save_car_data();

                char log_buf[BUFFER_SIZE];
                snprintf(log_buf, sizeof(log_buf), "{\"event\":\"CONNECT\", \"id\":\"%s\", \"ts\":%ld}", id, time(NULL));
                log_event(log_buf);

                snprintf(response, sizeof(response), "{\"status\":\"OK\", \"msg\":\"Welcome %s\"}\n", id);
                write(fd, response, strlen(response));
            }
        }
        else if (strstr(buffer, "UNSUBSCRIBE"))
        {
            pthread_mutex_lock(&data_lock);
            clients[fd].subscription = 0;
            pthread_mutex_unlock(&data_lock);

            printf("[UNSUBSCRIBE] Clientul cu id-ul: %s s-a dezabonat\n", clients[fd].id);
            snprintf(response, sizeof(response), "{\"status\":\"OK\", \"msg\":\"Unsubscribed\"}\n");
            write(fd, response, strlen(response));
        }
        else if (strstr(buffer, "SUBSCRIBE"))
        {
            pthread_mutex_lock(&data_lock);
            clients[fd].subscription = 1;
            pthread_mutex_unlock(&data_lock);

            printf("[SUBSCRIBE] Clientul cu id-ul: %s s-a abonat\n", clients[fd].id);
            snprintf(response, sizeof(response), "{\"status\":\"OK\", \"msg\":\"Subscribed\"}\n");
            write(fd, response, strlen(response));

            snprintf(response, sizeof(response), "Vreme: Prognoză azi, Iași: 10°C, Noros, șanse de ploaie ușoară la ora 16:00.\n Sport: Fotbal: FC BOTOSANI a învins Steaua cu scorul de 3-1. Mâine, meciul de baschet dintre Steaua și Dinamo la ora 20:00.\n Combustibili: Preț Mediu Benzină (azi): 7.25 RON/L. Cea mai mică variație înregistrată la stația Rompetrol (+0.02 RON).");
            write(fd, response, strlen(response));
        }
        else if (strstr(buffer, "TELEMETRY"))
        {
            float temp_lat = 0;
            float temp_long = 0;
            int temp_speed = 0;
            char *p;

            p = strstr(buffer, "\"lat\":\"");
            if (p)
                sscanf(p + 7, "%f", &temp_lat);

            p = strstr(buffer, "\"long\":\"");
            if (p)
                sscanf(p + 8, "%f", &temp_long);

            p = strstr(buffer, "\"speed\":\"");
            if (p)
                sscanf(p + 9, "%d", &temp_speed);

            pthread_mutex_lock(&data_lock);

            // intainte sa dam udpate, vedem care era indexul strazii vechi
            int old_index = find_street_index(clients[fd].lat, clients[fd].lng);
            int street_index = find_street_index(temp_lat, temp_long);

            if (old_index != -1 && old_index != street_index)
            {
                if (city_map[old_index].car_count > 0)
                    city_map[old_index].car_count--;
            }

            clients[fd].lat = temp_lat;
            clients[fd].lng = temp_long;
            clients[fd].speed = temp_speed;

            printf("[UPDATE] Clientul %s: lat: %.4f, long: %.4f, speed: %d\n",
                   clients[fd].id, temp_lat, temp_long, temp_speed);

            if (street_index != -1)
            {
                strcpy(clients[fd].street_name, city_map[street_index].name);

                // calculam inaltimea si lungimea strazii
                float inaltime = city_map[street_index].lat_max - city_map[street_index].lat_min;
                float latime = city_map[street_index].long_max - city_map[street_index].long_min;

                // vedem unde e mai viable sa mergem, pe lat sau long
                // 0 -> nord/sud    1->est/vest
                int recc_axis;
                if (inaltime > latime)
                    recc_axis = 0;
                else
                    recc_axis = 1;

                // trebuie sa trimitem un mesaj nou catre client ca sa il anuntam in ce directie sa o ia
                snprintf(response, sizeof(response), "{\"cmd\":\"INFO\", \"street\":\"%s\", \"limit\":%d, \"axis\":%d, \"msg\":\"Va aflati pe %s\"}\n", city_map[street_index].name, city_map[street_index].current_speed_limit, recc_axis, city_map[street_index].name);
                write(fd, response, strlen(response));

                if (street_index != old_index)
                    city_map[street_index]
                        .car_count++;

                snprintf(response, sizeof(response),
                         "Va aflati pe strada: %s , conduceti cu %d km/h iar limita de viteza este de: %d\n",
                         city_map[street_index].name,
                         clients[fd].speed,
                         city_map[street_index].current_speed_limit);
                write(fd, response, strlen(response));

                if (city_map[street_index].has_accident)
                {
                    alert_speed(street_index, 10, 1, fd);
                    snprintf(response, sizeof(response), "{\"cmd\":\"ALERT\", \"msg\":\"Accident pe %s! Limita 10 km/h\"}\n", city_map[street_index].name);
                    write(fd, response, strlen(response));
                }

                if (city_map[street_index].car_count > 5)
                {
                    city_map[street_index].traffic_level = 3;
                    city_map[street_index].current_speed_limit = 30;

                    alert_speed(street_index, 30, 2, fd);
                }
                else if (city_map[street_index].car_count > 3)
                {
                    city_map[street_index].traffic_level = 2;
                    city_map[street_index].current_speed_limit = 40;

                    alert_speed(street_index, 40, 2, fd);
                }

                if (clients[fd].speed > city_map[street_index].current_speed_limit)
                {
                    alert_speed(street_index, city_map[street_index].current_speed_limit, 3, fd);
                }
            }
            else
            {
                strcpy(clients[fd].street_name, "Unknown");
                snprintf(response, sizeof(response), "Alert, the street name is not known...please try something else...\n");
                write(fd, response, strlen(response));
            }

            pthread_mutex_unlock(&data_lock);

            char log_buf[BUFFER_SIZE];
            snprintf(log_buf, sizeof(log_buf),
                     "{\"event\":\"TELEMETRY\", \"id\":\"%s\", \"lat\":%.4f, \"lng\":%.4f, \"speed\":%d, \"ts\":%ld}",
                     clients[fd].id, temp_lat, temp_long, temp_speed, (long)time(NULL));
            log_event(log_buf);

            save_car_data();
        }
        else if (strstr(buffer, "REPORT"))
        {
            printf("[REPORT] Clientul %s raporteaza: accident\n", clients[fd].id);

            char log_buf[BUFFER_SIZE];
            snprintf(log_buf, sizeof(log_buf),
                     "{\"event\":\"REPORT\", \"type\":\"ACCIDENT\", \"reporter\":\"%s\", \"ts\":%ld}",
                     clients[fd].id, time(NULL));
            log_event(log_buf);

            int street_idx = find_street_index(clients[fd].lat, clients[fd].lng);

            if (street_idx != -1)
            {
                pthread_mutex_lock(&data_lock);
                city_map[street_idx].has_accident = 1;
                pthread_mutex_unlock(&data_lock);

                alert_speed(street_idx, 10, 1, fd);

                snprintf(response, sizeof(response), "{\"status\":\"RECEIVED\", \"msg\":\"Raport accident inregistrat pe %s. Multumim!\"}\n", city_map[street_idx].name);
            }
            else
            {
                snprintf(response, sizeof(response), "{\"status\":\"ERROR\", \"msg\":\"Nu te putem localiza pe o strada cunoscuta.\"}\n");
            }
            write(fd, response, strlen(response));
        }
    }
    return NULL;
}

void load_map_from_json()
{
    FILE *f = fopen("streets.json", "r");
    if (!f)
    {
        printf("[WARNING] streets.json lipseste. Folosesc harta goala.\n");
        return;
    }

    char line[512];
    map_size = -1;
    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "{"))
        {
            map_size++;
            city_map[map_size].has_accident = 0;
            city_map[map_size].car_count = 0;
        }
        if (strstr(line, "\"id\":"))
            sscanf(strstr(line, ":") + 1, "%d", &city_map[map_size].id);
        if (strstr(line, "\"name\":"))
        {
            char *start = strchr(line, ':');
            if (start)
                sscanf(start + 2, "\"%[^\"]\"", city_map[map_size].name);
        }
        if (strstr(line, "\"lat_min\":"))
            sscanf(strstr(line, ":") + 1, "%f", &city_map[map_size].lat_min);
        if (strstr(line, "\"lat_max\":"))
            sscanf(strstr(line, ":") + 1, "%f", &city_map[map_size].lat_max);
        if (strstr(line, "\"lon_min\":"))
            sscanf(strstr(line, ":") + 1, "%f", &city_map[map_size].long_min);
        if (strstr(line, "\"lon_max\":"))
            sscanf(strstr(line, ":") + 1, "%f", &city_map[map_size].long_max);
        if (strstr(line, "\"speed_limit\":"))
        {
            sscanf(strstr(line, ":") + 1, "%d", &city_map[map_size].default_speed_limit);
            city_map[map_size].current_speed_limit = city_map[map_size].default_speed_limit;
        }

        if (city_map[map_size].lat_max > max_lat)
            max_lat = city_map[map_size].lat_max;
        if (city_map[map_size].long_max > max_long)
            max_long = city_map[map_size].long_max;
    }
    map_size++;
    fclose(f);
    printf("[server] Harta incarcata: %d strazi.\n", map_size);
}

int main()
{
    load_map_from_json();
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sd;
    int optval = 1;
    socklen_t len = sizeof(from);

    memset(clients, 0, sizeof(clients));

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server] Eroare la creare socket");
        return errno;
    }

    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server] Eroare la bind");
        return errno;
    }

    if (listen(sd, 10) == -1)
    {
        perror("[server] Eroare la listen");
        return errno;
    }

    printf("[server] (Multithreaded) Asteptam la portul %d...\n", PORT);
    fflush(stdout);

    while (1)
    {
        int client_sock = accept(sd, (struct sockaddr *)&from, &len);

        if (client_sock < 0)
        {
            perror("[server] Eroare la accept");
            continue;
        }

        printf("[server] Client nou conectat: %d. Lansez thread...\n", client_sock);

        // thread safety,alocam pe heap memorie
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, (void *)new_sock) < 0)
        {
            perror("Eroare la creare thread");
            free(new_sock);
            close(client_sock);
        }
        else
        {
            pthread_detach(tid);
        }
    }
    return 0;
}
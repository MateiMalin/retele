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
#include <signal.h>

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

// functie care imi scrie intr-un json, starea la fiecare masina de pe harta
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
            fprintf(f, "    \"speed\": %d,\n", clients[i].speed);
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

// aici e functie ca sa anunt de accident,  atunci cand se intamapla
void broadcast_accident_event(int s_index)
{
    if (s_index == -1)
        return;
    char msj_local[512];
    char msj_global[512];
    char s_name[64];

    strcpy(s_name, city_map[s_index].name);

    // formatez cele 2 msj
    snprintf(msj_local, sizeof(msj_local), "{\"cmd\":\"ALERT\", \"type\":\"CRITIC\", \"msg\":\"ACCIDENT AICI! Limita redusa la 10 km/h pe %s!\"}\n", s_name);
    snprintf(msj_global, sizeof(msj_global), "{\"cmd\":\"ALERT\", \"type\":\"INFO\", \"msg\":\"Atentie: Accident raportat pe %s. Evitati zona if possible.\"}\n", s_name);

    pthread_mutex_lock(&data_lock);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active)
        {
            int client_s_index = find_street_index(clients[i].lat, clients[i].lng);

            if (client_s_index == s_index)
                write(i, msj_local, strlen(msj_local));
            else
            {
                write(i, msj_global, strlen(msj_global));
            }
        }
    }
    pthread_mutex_unlock(&data_lock);

    printf("[BROADCAST] Accident pe %s anuntat la toti clientii.\n", s_name);
}

void *client_thread(void *arg)
{
    // arg e un pointer la void, ii spun sa il tratez ca un pointer la int, * la *
    // o sa imi dea valoarea lui
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

            char log_buf[BUFFER_SIZE];
            snprintf(log_buf, sizeof(log_buf), "{\"event\":\"DISCONNECT\", \"id\":\"%s\", \"ts\":%ld}", clients[fd].id, time(NULL));
            log_event(log_buf);

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
                // intializam un client nou, blank
                strcpy(clients[fd].id, id);
                clients[fd].active = 1;
                clients[fd].lat = 0;
                clients[fd].lng = 0;
                clients[fd].speed = 0;
                clients[fd].subscription = 0;
                strcpy(clients[fd].street_name, "Unknown");
                pthread_mutex_unlock(&data_lock);

                printf("[CONNECT] S-a conectat un client nou: %s (socket fd: %d)\n", id, fd);

                // dam un mic reset la fisierul cu car data
                save_car_data();

                char log_buf[BUFFER_SIZE];
                snprintf(log_buf, sizeof(log_buf), "{\"event\":\"CONNECT\", \"id\":\"%s\", \"ts\":%ld}", id, time(NULL));
                log_event(log_buf);

                // la asta, nu stiu daca mai am nevoie de el, sau daca nu cumva ar trebui sa il formatez mai frumos
                snprintf(response, sizeof(response), "{\"status\":\"OK\", \"msg\":\"Welcome %s\"}\n", id);
                write(fd, response, strlen(response));
            }
        }
        else if (strstr(buffer, "UNSUBSCRIBE"))
        {
            pthread_mutex_lock(&data_lock);
            clients[fd].subscription = 0;
            pthread_mutex_unlock(&data_lock);

            char log_buf[BUFFER_SIZE];
            snprintf(log_buf, sizeof(log_buf), "{\"event\":\"UNSUBSCRIBE\", \"id\":\"%s\", \"ts\":%ld}", clients[fd].id, time(NULL));
            log_event(log_buf);

            printf("[UNSUBSCRIBE] Clientul cu id-ul: %s s-a dezabonat\n", clients[fd].id);
            snprintf(response, sizeof(response), "{\"status\":\"OK\", \"msg\":\"Unsubscribed\"}\n");
            write(fd, response, strlen(response));
        }
        else if (strstr(buffer, "SUBSCRIBE"))
        {
            pthread_mutex_lock(&data_lock);
            clients[fd].subscription = 1;
            pthread_mutex_unlock(&data_lock);

            char log_buf[BUFFER_SIZE];
            snprintf(log_buf, sizeof(log_buf), "{\"event\":\"SUBSCRIBE\", \"id\":\"%s\", \"ts\":%ld}", clients[fd].id, time(NULL));
            log_event(log_buf);

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

            // imi iau toate datele din telemetry
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
                if (old_index != -1)
                {
                    if (city_map[old_index].car_count > 0)
                        city_map[old_index].car_count--;

                    printf("[TRAFFIC] %s a iesit de pe %s. Count: %d\n",
                           clients[fd].id, city_map[old_index].name, city_map[old_index].car_count);
                }

                if (street_index != -1)
                {
                    city_map[street_index].car_count++;

                    printf("[TRAFFIC] %s a intrat pe %s. Count: %d\n",
                           clients[fd].id, city_map[street_index].name, city_map[street_index].car_count);
                }
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
                city_map[street_index].current_speed_limit = city_map[street_index].default_speed_limit;
                city_map[street_index].traffic_level = 1;

                if (city_map[street_index].car_count >= 5)
                {
                    city_map[street_index].traffic_level = 3;
                    city_map[street_index].current_speed_limit = 30;
                }
                else if (city_map[street_index].car_count >= 3)
                {
                    city_map[street_index].traffic_level = 2;
                    city_map[street_index].current_speed_limit = 40;
                }

                if (city_map[street_index].has_accident)
                {
                    city_map[street_index].traffic_level = 4;
                    city_map[street_index].current_speed_limit = 10;
                }

                char text_mesaj[256];
                char status_drum[50];

                // facem un status pentru fiecare drum in parte
                if (city_map[street_index].has_accident)
                    strcpy(status_drum, "ACCIDENT! (limit 10)");
                else if (city_map[street_index].traffic_level == 3)
                    strcpy(status_drum, "Trafic Intens (Limit 30)");
                else if (city_map[street_index].traffic_level == 2)
                    strcpy(status_drum, "Trafic Mediu (Limit 40)");
                else
                    strcpy(status_drum, "Drum Liber");

                // facem un mesaj pentru strada
                snprintf(text_mesaj, sizeof(text_mesaj),
                         "Strada: %s | %s | Viteza ta: %d km/h",
                         city_map[street_index].name,
                         status_drum,
                         clients[fd].speed);

                // facem comanda
                snprintf(response, sizeof(response),
                         "{\"cmd\":\"INFO\", \"street\":\"%s\", \"limit\":%d, \"axis\":%d, \"msg\":\"%s\"}\n",
                         city_map[street_index].name,
                         city_map[street_index].current_speed_limit,
                         recc_axis,
                         text_mesaj);

                write(fd, response, strlen(response));

                if (clients[fd].speed > city_map[street_index].current_speed_limit)
                {
                    char alert_buf[512];
                    snprintf(alert_buf, sizeof(alert_buf),
                             "{\"cmd\":\"ALERT\", \"type\":\"WARNING\", \"msg\":\"DEPASIRE VITEZA! Limita actuala e %d km/h\"}\n",
                             city_map[street_index].current_speed_limit);

                    write(fd, alert_buf, strlen(alert_buf));
                }
            }
            else
            {
                strcpy(clients[fd].street_name, "Unknown");
                snprintf(response, sizeof(response), "{\"cmd\":\"UNKNOWN\", \"msg\":\"Ai parasit zona monitorizata!\"}\n");
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
            int street_idx = find_street_index(clients[fd].lat, clients[fd].lng);
            char affected_street[64] = "Unknown";

            if (street_idx != -1)
            {
                strcpy(affected_street, city_map[street_idx].name);
            }

            printf("[REPORT] Clientul %s raporteaza accident pe strada: %s\n", clients[fd].id, affected_street);

            char log_buf[BUFFER_SIZE];
            snprintf(log_buf, sizeof(log_buf), "{\"event\":\"REPORT\", \"type\":\"ACCIDENT\", \"reporter\":\"%s\", \"street\":\"%s\", \"ts\":%ld}", clients[fd].id, affected_street, time(NULL));
            log_event(log_buf);

            if (street_idx != -1)
            {
                pthread_mutex_lock(&data_lock);
                city_map[street_idx].has_accident = 1;
                city_map[street_idx].current_speed_limit = 10;
                pthread_mutex_unlock(&data_lock);

                broadcast_accident_event(street_idx);

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
    signal(SIGPIPE, SIG_IGN);
    load_map_from_json();
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sd;
    int optval = 1;
    socklen_t len = sizeof(from);

    memset(clients, 0, sizeof(clients));

    // aici ar trebui sa resetez fisierul cu masini in prealabil
    save_car_data();

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server] Eroare la creare socket");
        return errno;
    }

    // sol socket e nivelul de setari, ca sa pot refolosi adresa locala
    // optval e doar un switch on and off
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    // adauga adresa la sd
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

        // thread safety,alocam pe heap memorie 4bytes mai exact
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        pthread_t tid;
        // client_thread e functia care da start la rutina
        if (pthread_create(&tid, NULL, client_thread, (void *)new_sock) < 0)
        {
            perror("Eroare la creare thread");
            free(new_sock);
            close(client_sock);
        }
        else
        {
            // once it finishes, it gets detached
            pthread_detach(tid);
        }
    }
    return 0;
}
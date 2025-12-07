#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <utmp.h>
#include <time.h>

#define MAX 9999
#define SERVER_FIFO_PATH "/retele/tema1/fifo_to_server"
#define USER_DATA_FILE "users_config.txt"

int main()
{
    unlink(SERVER_FIFO_PATH);
    if (mkfifo(SERVER_FIFO_PATH, 0666) == -1)
    {
        perror("mkfifo failed");
        return 1;
    }

    printf("Server started. Listening...\n");

    int fd = open(SERVER_FIFO_PATH, O_RDONLY);

    if (fd < 0)
    {
        perror("open fifo_to_server");
        exit(1);
    }

    char all_users[100][50];
    int cnt = 0;
    FILE *file = fopen(USER_DATA_FILE, "r");
    if (file)
    {
        while (fgets(all_users[cnt], 50, file))
        {
            char *aux = strchr(all_users[cnt], '\n');
            if (aux != NULL)
                *aux = '\0';
            cnt++;
        }
        fclose(file);
    }
    else
    {
        printf("Warning: users file not found the login will not be sucesful.\n");
    }

    int is_logged = 0;
    char c_user[50] = "";

    while (1)
    {
        char line[MAX];
        int bytes_read = read(fd, line, sizeof(line) - 1);
        if (bytes_read <= 0)
            continue;
        line[bytes_read] = 0;

        char *aux = strchr(line, '\n');
        if (aux != NULL)
            *aux = 0;

        char *sep = strchr(line, '|');
        if (!sep)
            continue;

        *sep = 0;
        char response_fifo[256];
        strcpy(response_fifo, line);

        char *user_command = sep + 1;

        int use_pipe_comm = 0, use_socket_comm = 0;
        if (strncmp(user_command, "get-logged-users", 16) == 0)
            use_socket_comm = 1;
        else
            use_pipe_comm = 1;

        int pipe_fds[2], socket_fds[2];
        if (use_pipe_comm)
            pipe(pipe_fds);
        if (use_socket_comm)
            socketpair(AF_UNIX, SOCK_STREAM, 0, socket_fds);

        pid_t pid = fork();
        if (pid == 0)
        {
            if (use_pipe_comm)
                close(pipe_fds[0]);
            if (use_socket_comm)
                close(socket_fds[0]);

            char output_buffer[MAX];
            int output_length = 0;

            if (strncmp(user_command, "login", 5) == 0)
            {
                char *cln_ptr = strchr(user_command, ':');

                if (cln_ptr != NULL)
                {
                    char *username_input = cln_ptr + 1;
                    while (*username_input == ' ')
                        username_input++;

                    if (*username_input != '\0')
                    {
                        if (is_logged)
                        {
                            if (strcmp(c_user, username_input) == 0)
                            {
                                output_length = snprintf(output_buffer, sizeof(output_buffer), "Login attempt does nothing, you are already logged in as %s.\n", c_user);
                                if (use_pipe_comm)
                                    write(pipe_fds[1], output_buffer, output_length);
                            }
                            else
                            {
                                strcpy(output_buffer, "no se puede logando mi amigo...\nThere is already somebody logged in.\n");
                                output_length = strlen(output_buffer);
                                if (use_pipe_comm)
                                    write(pipe_fds[1], output_buffer, output_length);
                            }
                        }
                        else
                        {
                            for (int i = 0; i < cnt; i++)
                            {
                                // printf("DEBUG: User[%d] = '%s'", i, all_users[i]);
                                if (strcmp(username_input, all_users[i]) == 0)
                                {
                                    is_logged = 1;
                                    break;
                                }
                            }
                            if (is_logged)
                            {
                                strcpy(output_buffer, "Login is successful.\n");
                                output_length = strlen(output_buffer);
                            }
                            else
                            {
                                strcpy(output_buffer, "No such username was found in the users.txt file, please, try again.\n");
                                output_length = strlen(output_buffer);
                            }

                            if (use_pipe_comm)
                                write(pipe_fds[1], output_buffer, output_length);
                        }
                    }
                    else
                    {
                        strcpy(output_buffer, "Error, no username after the colon.\n");
                        output_length = strlen(output_buffer);
                        if (use_pipe_comm)
                            write(pipe_fds[1], output_buffer, output_length);
                    }
                }
                else
                {
                    strcpy(output_buffer, "Missing colon, use 'login : username'format\n");
                    output_length = strlen(output_buffer);
                    if (use_pipe_comm)
                        write(pipe_fds[1], output_buffer, output_length);
                }
            }
            else if (strncmp(user_command, "get-logged-users", 16) == 0)
            {
                if (!is_logged)
                {
                    strcpy(output_buffer, "Error, you are not logged in.\n");
                    output_length = strlen(output_buffer);
                }
                else
                {
                    struct utmp *utmp_entry;
                    setutent();
                    output_length = 0;
                    while ((utmp_entry = getutent()) != NULL)
                    {
                        if (utmp_entry->ut_type == USER_PROCESS)
                        {
                            char time_string[64];
                            time_t timestamp = (time_t)utmp_entry->ut_tv.tv_sec;
                            struct tm *time_data = localtime(&timestamp);
                            strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", time_data);
                            output_length += snprintf(output_buffer + output_length, sizeof(output_buffer) - output_length, "%s\t%s\t%s\n",
                                                      utmp_entry->ut_user, utmp_entry->ut_host, time_string);
                        }
                    }
                    endutent();
                }
                if (use_socket_comm)
                    write(socket_fds[1], output_buffer, output_length);
            }
            else if (strncmp(user_command, "get-proc-info :", 15) == 0)
            {
                if (!is_logged)
                {
                    strcpy(output_buffer, "Error, you are not logged in.\n");
                    output_length = strlen(output_buffer);
                }
                else
                {
                    char proc_path[128];
                    snprintf(proc_path, sizeof(proc_path), "/proc/%s/status", user_command + 16);
                    FILE *proc_file = fopen(proc_path, "r");
                    if (proc_file)
                    {
                        output_length = fread(output_buffer, 1, sizeof(output_buffer) - 1, proc_file);
                        output_buffer[output_length] = 0;
                        fclose(proc_file);
                    }
                    else
                    {
                        strcpy(output_buffer, "Invalid PID");
                        output_length = strlen(output_buffer);
                    }
                }
                if (use_pipe_comm)
                    write(pipe_fds[1], output_buffer, output_length);
            }
            else if (strcmp(user_command, "logout") == 0)
            {
                if (is_logged == 1)
                {
                    strcpy(output_buffer, "Logged out\n");
                    output_length = strlen(output_buffer);
                }
                else
                {
                    strcpy(output_buffer, "Cannot log out if no user is logged in...\n");
                    output_length = strlen(output_buffer);
                }
                if (use_pipe_comm)
                    write(pipe_fds[1], output_buffer, output_length);
            }
            else if (strcmp(user_command, "quit") == 0)
            {
                strcpy(output_buffer, "Goodbye.");
                output_length = strlen(output_buffer);
                if (use_pipe_comm)
                    write(pipe_fds[1], output_buffer, output_length);
            }
            else
            {
                strcpy(output_buffer, "Unkown command");
                output_length = strlen(output_buffer);
                if (use_pipe_comm)
                    write(pipe_fds[1], output_buffer, output_length);
            }

            if (use_pipe_comm)
                close(pipe_fds[1]);
            if (use_socket_comm)
                close(socket_fds[1]);
            exit(0);
        }
        else
        {
            char response_data[MAX];
            int response_size = 0;

            if (use_pipe_comm)
            {
                close(pipe_fds[1]);
                response_size = read(pipe_fds[0], response_data, sizeof(response_data) - 1);
                if (response_size > 0)
                    response_data[response_size] = 0;
                close(pipe_fds[0]);
            }
            else if (use_socket_comm)
            {
                close(socket_fds[1]);
                response_size = read(socket_fds[0], response_data, sizeof(response_data) - 1);
                if (response_size > 0)
                    response_data[response_size] = 0;
                close(socket_fds[0]);
            }

            if (strncmp(user_command, "login", 5) == 0 && strstr(response_data, "Login is successful"))
            {
                is_logged = 1;

                char *colon_ptr = strchr(user_command, ':');
                if (colon_ptr != NULL)
                {
                    char *username = colon_ptr + 1;
                    while (*username == ' ')
                        username++;
                    strcpy(c_user, username);
                }
                else
                {
                    printf("Error in Parent: No colon found in command: '%s'\n", user_command);
                }
            }
            else if (strcmp(user_command, "logout") == 0)
            {
                is_logged = 0;
                c_user[0] = 0;
            }

            int response_fd = open(response_fifo, O_WRONLY);
            if (response_fd >= 0)
            {
                write(response_fd, &response_size, sizeof(response_size));
                write(response_fd, response_data, response_size);
                close(response_fd);
            }

            wait(NULL);
            if (strcmp(user_command, "quit") == 0)
            {
                printf("Server shutting down...\n");
                break;
            }
        }
    }

    close(fd);
    unlink(SERVER_FIFO_PATH);
    return 0;
}
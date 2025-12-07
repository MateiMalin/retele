#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define LMAX 9999
#define fifo_to_server "/retele/tema1/fifo_to_server"

int main()
{
    pid_t pid = getpid();
    char resp_fifo[256];
    snprintf(resp_fifo, sizeof(resp_fifo), "/tmp/client_resp_%d", (int)pid);

    unlink(resp_fifo);
    if (mkfifo(resp_fifo, 0666) < 0)
    {
        perror("mkfifo resp_fifo");
        return 1;
    }

    int cmfd = open(fifo_to_server, O_WRONLY);
    if (cmfd < 0)
    {
        perror("open fifo_to_server");
        return 1;
    }

    printf("Client started.\n");

    char line[LMAX];
    while (1)
    {
        if (!fgets(line, LMAX, stdin))
            break;
        char *aux = strchr(line, '\n');
        if (aux != NULL)
            *aux = '\0';

        char msj[LMAX * 2];
        snprintf(msj, sizeof(msj), "%s|%s\n", resp_fifo, line);

        if (write(cmfd, msj, strlen(msj)) != strlen(msj))
        {
            perror("write cmd fifo");
            break;
        }

        int rf = open(resp_fifo, O_RDONLY);
        if (rf < 0)
        {
            perror("open resp_fifo");
            break;
        }

        int lg;
        if (read(rf, &lg, sizeof(lg)) != sizeof(lg))
        {
            perror("read length");
            break;
        }

        char buf[LMAX];
        if (lg > 0 && lg < LMAX)
        {
            if (read(rf, buf, lg) != (ssize_t)lg)
            {
                perror("read payload");
                break;
            }
            buf[lg] = 0;
            printf("%s\n", buf);
        }
        else if (lg >= LMAX)
        {
            printf("Response too large\n");
        }

        close(rf);

        if (strcmp(line, "quit") == 0)
            break;
    }

    close(cmfd);
    unlink(resp_fifo);
    return 0;
}

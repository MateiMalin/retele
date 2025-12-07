#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

int my_execvp(const char *file, char *const argv[])
{
    char *path_env = getenv("PATH");
    if (!path_env)
    {
        execv(file, argv);
        return -1;
    }

    if (strchr(file, '/'))
    {
        execv(file, argv);
        return -1;
    }

    char *path_copy = strdup(path_env);
    if (!path_copy)
    {
        perror("Error strdup");
        return -1;
    }

    char *dir = strtok(path_copy, ":");
    while (dir != NULL)
    {
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, file);

        execv(fullpath, argv);
        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return -1;
}

int main()
{
    char line[MAX_LINE];

    printf("Enter command: ");
    if (!fgets(line, sizeof(line), stdin)) {
        perror("fgets");
        return 1;
    }

    // line[strcspn(line, "\n")] = 0;

    char *args[MAX_ARGS];
    int argc = 0;

    char *token = strtok(line, " ");
    while (token != NULL && argc < MAX_ARGS - 1) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }
    args[argc] = NULL;

    if (argc == 0) {
        fprintf(stderr, "No command entered\n");
        return 1;
    }

    my_execvp(args[0], args);

    perror("my_execvp failed");
    return 1;
}

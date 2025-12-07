#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main() {
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        // ----- CHILD -----
        int num;
        char answer[4];

        // Read number from parent
        read(fd[0], &num, sizeof(num));

        // Check if "prime" (odd = yes, even = no)
        if (num % 2 == 1)
            strcpy(answer, "yes");
        else
            strcpy(answer, "no");

        // Write response back to parent
        write(fd[1], answer, strlen(answer) + 1); // include null terminator

        // Close pipe ends
        close(fd[0]);
        close(fd[1]);

        exit(0);
    } else {
        // ----- PARENT -----
        int num;
        char answer[4];

        // Read number from keyboard
        printf("Enter a number: ");
        fflush(stdout);
        scanf("%d", &num);

        // Send number to child
        write(fd[1], &num, sizeof(num));

        // Now read response from child
        read(fd[0], answer, sizeof(answer));

        // Print result
        printf("Is the number %d prime? %s\n", num, answer);

        // Close pipe ends
        close(fd[0]);
        close(fd[1]);
    }

    return 0;
}

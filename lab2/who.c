#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

int main()
{
  const char *fifo_path = "myfifo";
  mkfifo(fifo_path, 0666); // create FIFO if not exists

  pid_t pid = fork();

  if (pid == -1)
  {
    perror("fork failed");
    exit(1);
  }

  if (pid == 0)
  {
    // --- Child process: reads from FIFO ---
    int fd = open(fifo_path, O_RDONLY);
    char buf[100];
    read(fd, buf, sizeof(buf));
    printf("Child received: %s\n", buf);
    close(fd);
  }
  else
  {
    // --- Parent process: writes to FIFO ---
    int fd = open(fifo_path, O_WRONLY);
    char msg[] = "Hello from parent!";
    write(fd, msg, strlen(msg) + 1);
    close(fd);
    wait(NULL);
  }

  unlink(fifo_path); // delete the FIFO file
  return 0;
}
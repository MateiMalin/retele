#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main()
{
    int sockets[2], child;
    char buf[1024];

    socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    child=fork();

    if(child)//suntem in parinte aici
    {
        close(sockets[0]);

        read(sockets[1], buf, 1024);
        printf("Parent process: %s\n", buf);
        write(sockets[1], "first", 5);
        
        close(sockets[1]);
    }
    else
    {
        //child
        close(sockets[1]);

        write(sockets[0], "second", 6);
        read(sockets[0], buf, 1024);
        printf("Child procces: %s\n", buf);

        close(sockets[0]);
    }
    return 0;
}
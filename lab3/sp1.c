#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

//tatal trimite fiului un sir, fiul concateneaza la primul sir 
//un alt sir si intoarce tatalui raspunsul obtinut
//se foloseste socketpair
int main()
{
    int sockets[2], child;// 0 pentru parinte, 1 pentru copil

    socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    child=fork();


    if(child==0)
    {
        //i am in the child
        close(sockets[0]);//inchidem canalul de la parinte

        char msg_parent[1024];
        char msg_child[1024];
        char combined[1024];

        read(sockets[1], msg_parent,sizeof(msg_parent));
        fgets(msg_child, sizeof(msg_child), stdin);

        size_t len = strlen(msg_parent);
        if (len > 0 && msg_parent[len - 1] == '\n') {
            msg_parent[len - 1] = '\0';
        }

        len = strlen(msg_child);
        if (len > 0 && msg_child[len - 1] == '\n') {
            msg_child[len - 1] = '\0';
        }

        snprintf(combined, sizeof(combined), "%s %s", msg_parent, msg_child);

        write(sockets[1], combined, strlen(combined)+1);

        close(sockets[1]);
    }
    else
    {
        //i am in the parent
        close(sockets[1]);

        char msg_to_child[100];
        char msg_from_child[200];

        fgets(msg_to_child, sizeof(msg_to_child), stdin);

        write(sockets[0], msg_to_child, strlen(msg_to_child));

        read(sockets[0], msg_from_child, sizeof(msg_from_child));
        printf("Mesajul va fi: %s\n", msg_from_child);

        close(sockets[0]);
    }

    return 0;
}
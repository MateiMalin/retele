#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

int main(int argc, char *argv[])
{
  int sd;                    // descriptorul de socket
  struct sockaddr_in server; // structura folosita pentru conectare
  char msg[100];             // mesajul trimis

  /* exista toate argumentele in linia de comanda? */
  if (argc != 4)
  {
    printf("Sintaxa: %s <nume_fisier> <adresa_server> <port>\n", argv[0]);
    return -1;
  }

  /* stabilim portul */
  port = atoi(argv[3]);

  /* cream socketul */
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("Eroare la socket().\n");
    return errno;
  }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[2]);
  /* portul de conectare */
  server.sin_port = htons(port);

  /* ne conectam la server */
  if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[client]Eroare la connect().\n");
    return errno;
  }

  
  int fd = open(argv[1], O_CREAT | O_WRONLY, 0666);
  if(fd == -1){
    perror("Error creating the file!");
    return errno;
  }
  int bytes;
  char buf[2048];
  while(read(sd, &bytes, sizeof(int))){
    if(bytes == -1){
      perror("Error at reading from socket");
      return errno;
    }
    if(read(sd, buf, bytes) == -1){
      perror("Error at reading from socket");
      return errno;
    }
    write(fd, buf, bytes);
  }
  printf("[client]Finished writing the file!\n");
  close(fd);
  /* inchidem conexiunea, am terminat */
  close(sd);
}
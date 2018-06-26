#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void err_func(char *msg)
{
perror(msg);
exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
  int sockfd, len;
  char buf[BUFSIZ];
  struct sockaddr_in serv;
  unsigned short port;
  if(argc != 3){
    printf("usage: progname serv_ip serv_port\n");
    exit(EXIT_FAILURE);
  }
  if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    err_func("socket");
  serv.sin_family = PF_INET;
  port = (unsigned short)strtol(argv[2] , NULL , 10);
  serv.sin_port = htons(port);
  inet_aton(argv[1], &(serv.sin_addr));

  if(connect(sockfd, (struct sockaddr *)&serv, sizeof(struct sockaddr_in)) < 0)
    err_func("connect");

  do {
    printf("-> ");
    fgets(buf, BUFSIZ, stdin);
    len = strlen(buf);
    buf[len] = '\0';
    len = strlen(buf);
    len = send(sockfd, buf, len + 1, 0);
    len = recv(sockfd, buf, len, 0);
    printf("<- %s", buf);
    } while (strncasecmp(buf, "EXIT\r\n", 6) != 0 && strncasecmp(buf, "EXIT\n", 5) != 0);
 
  close(sockfd);
  return 0;
}


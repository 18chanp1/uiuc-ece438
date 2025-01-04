/*
** server.c -- a stream socket server demo
*/

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10  // how many pending connections queue will hold

#define MAX_URL_LENGTH 2048
#define MAX_BUFFER_LENGTH 1024

#define HTTP_BAD_REQUEST "HTTP/1.1 400 Bad Request\r\n\r\n"
#define HTTP_NOT_FOUND "HTTP/1.1 404 Not Found\r\n\r\n"

void sigchld_handler(int s) { while (waitpid(-1, NULL, WNOHANG) > 0); }

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int send_response(int fd) {
  // Read the first 4 bytes to determine the method
  char *method = calloc(sizeof(char), 5);
  if (read(fd, method, 4) == -1) {
    write(fd, HTTP_BAD_REQUEST, strlen(HTTP_BAD_REQUEST));
    perror("failed to read from socket");
    free(method);
    return 0;
  }
  if (strcasecmp(method, "GET ") != 0) {
    write(fd, HTTP_BAD_REQUEST, strlen(HTTP_BAD_REQUEST));
    free(method);
    return 0;
  }
  free(method);

  char *url = calloc(sizeof(char), MAX_URL_LENGTH + 1);
  if (read(fd, url, MAX_URL_LENGTH) == -1) {
    write(fd, HTTP_BAD_REQUEST, strlen(HTTP_BAD_REQUEST));
    free(url);
    return 0;
  }
  char *path = strsep(&url, " ?");
  if (path == NULL) {
    write(fd, HTTP_BAD_REQUEST, 28);
    free(url);
    return 0;
  }
  path = path + 1;  // remove leading '/'
  FILE *read_fd = fopen(path, "r");
  if (read_fd == NULL) {
    write(fd, HTTP_NOT_FOUND, strlen(HTTP_NOT_FOUND));
    free(url);
    return 0;
  }

  write(fd, "HTTP/1.1 200 OK\r\n\r\n", 19);
  char *buffer = calloc(sizeof(char), MAX_BUFFER_LENGTH + 1);
  while (fgets(buffer, MAX_BUFFER_LENGTH, read_fd) != NULL) {
    write(fd, buffer, strlen(buffer));
  }
  fclose(read_fd);
  free(url);
  free(buffer);
  return 0;
}

int main(int argc, char *argv[]) {
  int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;  // connector's address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  char *port = PORT;
  if (argc > 1) {
    port = argv[1];
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;  // use my IP

  if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    return 2;
  }

  freeaddrinfo(servinfo);  // all done with this structure

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  sa.sa_handler = sigchld_handler;  // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf("server: waiting for connections...\n");

  while (1) {  // main accept() loop
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);
    printf("server: got connection from %s\n", s);

    if (!fork()) {    // this is the child process
      close(sockfd);  // child doesn't need the listener
      if (send_response(new_fd) == -1) {
        perror("failed to send response");
      }
      close(new_fd);
      exit(0);
    }
    close(new_fd);  // parent doesn't need this
  }

  return 0;
}

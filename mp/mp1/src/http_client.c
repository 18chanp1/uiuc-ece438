/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>


#include <stdbool.h>


#define PORT "80" // the port client will be connecting to 

#define MAXDATASIZE 65540 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

typedef struct {
  char* hostname;
  char* port;
  char* path;
} URL;

void freeURL(URL* url)
{
  free(url->hostname);
  free(url->path);
  free(url->port);
  free(url);
}


URL* parse_hostname(char* arg_in)
{
  //check that it starts with http:// or https://
  bool is_http = strncmp(arg_in, "http://", 7) == 0;
  bool is_https = strncmp(arg_in, "https://", 8) == 0;
  if (!is_http && !is_https)
  {
    fprintf(stderr, "Invalid URL: %s\n", arg_in);
    exit(1);
  }

  //parse the hostname
  int charsToHostname = (is_http ? 7 : 8);

  //parse port, if applicable
  int charsToColon = strcspn(arg_in + charsToHostname, ":") + charsToHostname; //+ charsToHostname + 1;
  bool hasPort = charsToColon != strlen(arg_in);

  //parse path.
  int charsToPath;
  if(!hasPort)  charsToPath = charsToColon = strcspn(arg_in + charsToHostname, "/") + charsToHostname;
  else          charsToPath = strcspn(arg_in + charsToColon, "/") + charsToColon;

  //allocate memory for the URL struct
  URL* url = (URL*)malloc(sizeof(URL));

  char* hostname = (char*) malloc(charsToColon - charsToHostname + 1);
  memcpy(hostname, arg_in + charsToHostname, charsToColon - charsToHostname);
  hostname[charsToColon - charsToHostname] = '\0';
  url->hostname = hostname;

  char* path = (char*) malloc(strlen(arg_in) - charsToPath + 1);
  memcpy(path, arg_in + charsToPath, strlen(arg_in) - charsToPath);
  path[strlen(arg_in) - charsToPath] = '\0';
  url->path = path;

  if(hasPort)
  {
    //note: there is a colon in the port string, so we need to allocate one more byte than the difference in the two lengths
    char* port_str = (char*) malloc(charsToPath - charsToColon);
    memcpy(port_str, arg_in + charsToColon + 1, charsToPath - charsToColon - 1);
    port_str[charsToPath - charsToColon] = '\0';
    url->port = port_str;
  }
  else
  {
    char* port_str = (char*) malloc(3);
    url->port = strcpy(port_str, "80");
  }

  fprintf(stderr, "file: %s\n", url->path);

  return url;
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

  URL* url = parse_hostname(argv[1]);

  printf("hostname: %s\n", url->hostname);

	if ((rv = getaddrinfo(url->hostname, url->port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

  char* request = (char*) malloc(strlen(url->path) + 100);
  sprintf(request, "GET %s HTTP/1.1\nUser-Agent:  Wget/1.12 (linux-gnu)\nHost:%s:%s\nConnection:  close\r\n\r\n", url->path, url->hostname, url->port);
  printf("%s\n", request);
  send(sockfd, request, strlen(request), 0);

  //setup ouput file
  FILE* out_fptr = fopen("output", "w");

  char* bufCurr = buf;
  char* bodyStart = NULL;
  int totalBytes = 0;
  bool headerDone = false;

  //read header
  while(!headerDone && (numbytes = recv(sockfd, bufCurr + totalBytes, MAXDATASIZE-1, 0)) > 0)
  {
    bufCurr[numbytes] = '\0';
    totalBytes += numbytes;

    char* headerEnd = strstr(buf, "\r\n\r\n");
    if(headerEnd != NULL)
    {
      headerDone = true;
      bodyStart = headerEnd + 4;
      fputs(bodyStart, out_fptr);
      bufCurr = buf;
    }
  }

  // read body
  while((numbytes = recv(sockfd, bufCurr, MAXDATASIZE-1, 0)) > 0)
  {
    bufCurr[numbytes] = '\0';
    fputs(bufCurr, out_fptr);
  }

  if(numbytes == -1)
  {
    perror("recv");
    exit(1);
  }


  printf("client: received '%s'\n",buf);

  fclose(out_fptr);
	close(sockfd);
  freeURL(url);

	return 0;
}


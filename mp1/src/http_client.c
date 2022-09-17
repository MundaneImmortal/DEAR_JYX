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

#define NEWLINE 		"\r\n"
#define SEPARATOR 		"\r\n\r\n"
#define HTTP_GET 		"GET"
#define HTTP            "http"
#define HTTP_PROTOCOL 	"HTTP/1.1"
#define HTTP_PORT 		"80"

#define OUTPUT_FILE 	"output"

#define BUF_SIZE 2048

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int get_http_status(char *response)
{
	strtok(response, " ");
	char *status = strtok(NULL, " ");
	return atoi(status);
}
/* Steps
	1. Parse input from commandline
 */
int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char *protocol, *host, *path, *port, *host_ip;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	
	// input parsing and validation
	if (argc != 2) {
	    fprintf(stderr,"usage: client url\n");
	    exit(1);
	}
	protocol = strtok(argv[1], "://");
	host = strtok(NULL, "/");
	path = strtok(NULL, "");

	host_ip = strtok(host, ":");
	port = strtok(NULL, ""); if (port == NULL) {
		port = HTTP_PORT;
	}

	if (strcmp(protocol, HTTP) || host == NULL || path == NULL || 
			atoi(port) < 0 || atoi(port) > 65535) {
			
		fprintf(stderr, "Valid URL format: http://host<:port>/path\n");
		return 1;
	}

	// get address info
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(host_ip, port, &hints, &servinfo)) != 0) {
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

	// done connecting to host
	freeaddrinfo(servinfo); 

	// form http request
	long int request_len = strlen(HTTP_GET) + strlen(" /") + strlen(path) + strlen(" ") + strlen(HTTP_PROTOCOL) + 
			strlen(NEWLINE) + strlen(NEWLINE) + 1;
	
	char *request = malloc(request_len);
	snprintf(request, request_len, "%s /%s %s%s%s", HTTP_GET, path, HTTP_PROTOCOL, NEWLINE, NEWLINE);
	printf("request: %s", request);
	
	// send request
	if (send(sockfd, request, strlen(request), 0) == -1) {
		perror("send");
		free(request);
		close(sockfd);
		return 1;
	}
	free(request);

	// TODO: the actual number of bytes sent should always be checked before proceeding
	// receive response
	char buf[BUF_SIZE] = {'\0'};
	char buf_copy[BUF_SIZE] = {'\0'};
	if ((numbytes = recv(sockfd, buf, BUF_SIZE-1, 0)) == -1) {
		perror("recv");
		close(sockfd);
		return 1;
	}
	strcpy(buf_copy, buf);
	
	// take the response header and print it out
	char *response_header = strtok(buf, SEPARATOR);
	printf("response: %s\n", buf);
	printf("response header: %s\n", response_header);
	char *response_body = &buf_copy[strlen(response_header) + strlen(SEPARATOR)];
	printf("response body: %s", response_body);

	if (get_http_status(response_header) != 200) {
		// no need to print the body
		close(sockfd);
		return 1;
	}

	// write the response body to a file
	FILE *output_file = fopen(OUTPUT_FILE, "wb");
	if (output_file == NULL) {
		perror("fopen");
		close(sockfd);
		return 1;
	}

	fwrite(response_body, 1, strlen(response_body), output_file);
	while ((numbytes = recv(sockfd, buf, BUF_SIZE-1, 0)) > 0) {
		fwrite(buf, 1, numbytes, output_file);
	}
	printf("server closed connection\n");

	fclose(output_file);
	close(sockfd);
	return 0;
}


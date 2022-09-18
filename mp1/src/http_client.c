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

int headlen(char* origin, char* sub){
	int result;
	char* tail = strstr(origin,sub);
	if (tail == NULL){
		result = strlen(origin);
	}
	else{
		result = strlen(origin) - strlen(tail);
	}

	return result;
}
void substr(char* dest, char* src, unsigned int cnt){
	strncpy(dest, src, cnt);
	dest[cnt] = '\0';

}

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

	int temp = headlen(argv[1], "//");
	protocol = malloc(temp+1);
	substr(protocol, argv[1], temp-1);

	char* url = strstr(argv[1], "//") + 2;
	temp = headlen(url, "/");
	path = strstr(url, "/");
	host = malloc(temp+1);
	substr(host, url, temp);

	if(headlen(host,":") == temp){
		port = HTTP_PORT;
		host_ip = malloc(temp+1);
		substr(host_ip, host, temp);
	}
	else{
		temp = headlen(host,":");
		port = strstr(host, ":")+1;
		host_ip = malloc(temp+1);
		substr(host_ip, host, temp);
	}
	printf("\nprotocol:%s,host:%s, host_ip:%s, port:%s,path:%s",protocol,host,host_ip,port,path);
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
		free(protocol);
		free(host);
		free(host_ip);

		return 1;
	}
	free(protocol);
	free(host);
	free(host_ip);

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
	snprintf(request, request_len, "%s %s %s%s%s", HTTP_GET, path, HTTP_PROTOCOL, NEWLINE, NEWLINE);
	printf("request: %s", request);
	
	// send request
	if (send(sockfd, request, strlen(request), 0) == -1) {
		perror("send");
		free(request);
		close(sockfd);
		return 1;
	}
	free(request);

	// receive response
	char buf[BUF_SIZE] = {'\0'};

	// write the response body to a file
	FILE *output_file = fopen(OUTPUT_FILE, "wb");
	if (output_file == NULL) {
		perror("fopen");
		close(sockfd);
		return 1;
	}
	
    int pkg_1 = 0;
	int cnt = 0;
	while(1){
		memset(buf, '\0', BUF_SIZE);
		if ((numbytes = recv(sockfd, buf, BUF_SIZE-1, 0)) <= 0) {
			perror("recv");
			close(sockfd);
			return 1;
		}
		else{
			cnt += 1;
			if (pkg_1 == 1){
				pkg_1 = 0;
				char* resp_body = strstr(buf, SEPARATOR)+4;
				fwrite(resp_body, 1, strlen(resp_body), output_file);
			}
			else{
				fwrite(buf, 1, BUF_SIZE, output_file);
			}
			printf("cnt:%d",cnt);
		}
	}


	printf("server closed connection\n");

	fclose(output_file);
	close(sockfd);
	return 0;
}


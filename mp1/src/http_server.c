
/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXREQSIZE 2048 // max size of http request allowed in bytes

#define NEWLINE 			"\r\n"
#define HTTP_GET 			"GET"
#define HTTP_PROTOCOL 		"HTTP/1.1"
#define HTTP_OK 			"HTTP/1.1 200 OK\r\n\r\n"
#define HTTP_NOT_FOUND 		"HTTP/1.1 404 Not Found\r\n\r\n"
#define HTTP_BAD_REQUEST 	"HTTP/1.1 400 Bad Request\r\n\r\n"

void sigchld_handler() //int s
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

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

void serve_http(int sockfd)
{
	long int numbytes, fsize;
	char buf[MAXREQSIZE];
	char *req_ptr;
	char *method, *path, *protocol;
	FILE *fp;
	

	// Receive the request
	numbytes = recv(sockfd, buf, MAXREQSIZE-1, 0);
	if (numbytes == -1) {
		perror("recv");
		exit(1);
	} else if (numbytes == 0) {
		printf("Connection closed by client"); // TODO: add client info here
		return;
	}
	buf[numbytes] = '\0';	
	printf("Received request: %s\n", buf);
	
	// parse request
	int temp = headlen(buf, "\r\n");
	req_ptr = malloc(temp+1);
	substr(req_ptr, buf, temp);
	// printf("\nreq_ptr:%s",req_ptr);

	temp = headlen(req_ptr," ");
	method = malloc(temp+1);
	substr(method, req_ptr, temp);
	// printf("\nmethod:%s",method);

	char* url = strstr(req_ptr, " ")+1;
	protocol = strstr(url, " ")+1;
	// printf("\nprotocol:%s",protocol);


	temp = headlen(url, " ");
	path = malloc(temp+1);
	substr(path, url, temp);
	// printf("\npath:%s",path);
	// validate request
	if (req_ptr == NULL || method == NULL || path == NULL || protocol == NULL || 
		strcmp(method, HTTP_GET) != 0 || strcmp(protocol, HTTP_PROTOCOL) != 0 || strcmp(path, "/") == 0) {
			// TODO: Invalid request
			send(sockfd, HTTP_BAD_REQUEST, strlen(HTTP_BAD_REQUEST), 0);
			return;
	}

	// parse path and serve file
	path = path + 1; // remove leading slash

	fp = fopen(path, "rb"); 
	
	
	if (fp == NULL) {
		// TODO: File not found
		send(sockfd, HTTP_NOT_FOUND, strlen(HTTP_NOT_FOUND), 0);
		return;
	}
	
	// if path is a directory
	struct stat st;
	fstat(fileno(fp), &st); if (!S_ISREG(st.st_mode)) {
		printf("Requested path %s is a directory\n", path);
		fclose(fp);
		send(sockfd, HTTP_NOT_FOUND, strlen(HTTP_NOT_FOUND), 0);
		return;
	}

	send(sockfd, HTTP_OK, strlen(HTTP_OK), 0);	

	// send the actual file
	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	printf("Sending file %s of size %ld bytes\n", path, fsize);

	while (fsize > 0) {
		numbytes = fread(buf, 1, MAXREQSIZE, fp);
		if (numbytes == -1) {
			perror("fread");
			exit(1);
		}
		fsize -= numbytes;
		
		int bytes_sent = 0;
		while (numbytes > 0) {
			bytes_sent = send(sockfd, &buf[bytes_sent], numbytes - bytes_sent, 0);
			if (bytes_sent == -1) {
				perror("send");
				exit(1);
			}
			numbytes -= bytes_sent;
		}
	}
	free(path-1);
	free(req_ptr);
	free(method);
	// done sending file
	fclose(fp);
}


int main(int argc, char *argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	char *port;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	// step 1: parse input from commandline to get port number
	if (argc != 2) {
		fprintf(stderr, "usage: server port\n");
		exit(1);
	}
	port = argv[1];
	// step 2: validate if the port number is valid
	if (atoi(port) < 0 || atoi(port) > 65535) {
		fprintf(stderr, "port number should be between 0 and 65535");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
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

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			serve_http(new_fd);
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}


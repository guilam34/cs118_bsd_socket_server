#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>

int MAX_REQUEST_SIZE = 2048;

void handle(int client_fd){
	char request_buffer[MAX_REQUEST_SIZE];
	int request_size = read(client_fd, request_buffer, MAX_REQUEST_SIZE);
	if(request_size == -1){
	  fprintf(stderr, "Error reading request from client.\n");
  	exit(1);
	}
	else if(request_size == 0){
  	fprintf(stderr, "Connection closed before reading request\n");	
  	exit(1);
	}
	else {
  	fprintf(stdout,"%s\n",request_buffer);
	}
}

int main(int argc, char *argv[]){
	if(argc < 2){
  	fprintf(stderr, "No port number provided for server.\n");
  	exit(1);
	}

	//Create socket
	int port_no = atoi(argv[1]);
	int sock_fd = socket(PF_INET, SOCK_STREAM, 0);
	
	if(sock_fd == -1){
  	fprintf(stderr, "Unable to create server socket at port %d.\n", port_no);	
  	exit(1);
	}

	//Bind socket to port and IP
	struct sockaddr_in server_addr, client_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_no);
	server_addr.sin_addr.s_addr = INADDR_ANY;		
	
	int client_addr_size = sizeof(client_addr);

	int result = bind(sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));
	if(result == -1){
  	fprintf(stderr, "Unable to bind socket to server address.\n");
  	exit(1);
	}
	
	//Listen indefinitely for connections
	listen(sock_fd, 10);	

	//Fork for each new connection that arrives, children handles requests
	int client_fd, pid;
	while(1){
  	client_fd = accept(sock_fd, 
  			   (struct sockaddr *) &client_addr, 					   &client_addr_size);  	
  	if (client_fd == -1) {
  		fprintf(stderr, "Unable to accept new connection.\n");
  	}			

  	pid = fork(); 
  	if (pid < 0) // Error
  	    error("ERROR on fork");
  	 
  	if (pid == 0)  { // child process
  	    close(sock_fd);
  	    handle(client_fd);			       
  	    exit(0);
  	}
  	else // parent process
  	    close(client_fd);  
	}
}

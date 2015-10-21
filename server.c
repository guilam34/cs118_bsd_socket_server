#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

int MAX_REQUEST_SIZE = 2048;
int MAX_DATA_SIZE = 256;

char* response_ok = 
	"%s 200 OK\n"
	"Content-Type: %s\n"
	"\n";

char* response_badrequest = 
	"HTTP/1.1 400 Bad Request\n"
	"Content-Type: text/html\n"
	"\n"
	"<html>\n"
	"	<body>\n"
	"		<h1 400>Bad Request</h1>\n"
	"		<p>This server did not understand your request</p>\n"
	"	</body>\n"
	"</html>\n";

char* response_notfound =
	"HTTP/1.1 404 Not Found\n"
	"Content-Type: text/html\n"
	"\n"
	"<html>\n"
	"	<body>\n"
	"		<h1>404 Not Found</h1>\n"
	"		<p>This server could not find your requested file</p>\n"
	"	</body>\n"
	"</html>\n";	

char* response_notimplemented = 
	"HTTP/1.1 501 Not Implemented\n"
	"Content-Type: text/html\n"
	"\n"
	"<html>\n"
	"	<body>\n"
	"		<h1>501 Not Implemented</h1>\n"
	"		<p>This server could not support the request method</p>\n"
	"	</body>\n"
	"</html>\n";	

//Function to write error message to client based on error code passed in
void send_request_error(int client_fd, int code){
	int response_buffer_size;
	char* response, *response_buffer;
	if(code == 400){
		response = response_badrequest;	
		fprintf(stderr,"Client request was unable to be processed.\n");
	}
	else if(code == 404){
		response = response_notfound;	
		fprintf(stderr,"Requested file could not be served.\n");
	}
	else{
		response = response_notimplemented;
		fprintf(stderr,"Request method is not supported by this server.\n");
	}
	response_buffer_size = snprintf(NULL, 0, response);
	response_buffer = (char*)malloc(response_buffer_size + 1);
	snprintf(response_buffer, response_buffer_size + 1, response);	
	write(client_fd, response_buffer, strlen(response_buffer));
}

//Handles HTTP requests and send response message
void handle(int client_fd){
	int current_data_size, response_fd, response_buffer_size, request_size;
	char current_char;
	char request_buffer[MAX_REQUEST_SIZE], response_data[MAX_DATA_SIZE];
	char *buffer_copy, *response_buffer;
	char *content_type, *file_name, *file_type, *http_version, *method_type;

	//Read HTTP request into a buffer for parsing
	request_size = read(client_fd, request_buffer, MAX_REQUEST_SIZE);
	if(request_size == -1){
		send_request_error(client_fd, 400);
		exit(1);
	}
	else if(request_size == 0){
		send_request_error(client_fd, 400);
		exit(1);
	}
	else {
		//Parses HTTP request buffer
		fprintf(stdout,"%s\n",request_buffer);
		buffer_copy = (char*)malloc(sizeof(request_buffer)+1);
		strcpy(buffer_copy,request_buffer);
		method_type = strtok(buffer_copy," ");				
		file_name = strtok(NULL, " ");		
		http_version = strtok(NULL, " \r\n");	

		if(file_name[0] == '/'){
			file_name++;
		}
		
		fprintf(stdout, "New %s request from client %d for %s.\n", method_type, client_fd, file_name);

		//Get requested file's extension
		file_type = strrchr(file_name, '.');
		if(file_type == NULL){
			file_name = strcat(file_name, ".html");
			file_type = "html";
		}
		else{
			file_type = file_type + 1;			
		}

		//Set content-type field based on file extension
		if(strstr(file_type, "html") != NULL){
			content_type = "text/html";
		}
		else if(strstr(file_type, "jpeg") != NULL || strstr(file_type, "jpg") != NULL){
			content_type = "image/jpeg";
		}
		else{
			content_type = "image/gif";
		}
			
		//Check if method is supported
		if(strstr("GET", method_type) == NULL){				
			send_request_error(client_fd, 501);
			exit(1);
		}

		//Opens file and reads to buffer
		response_fd = open(file_name, O_RDONLY);
		if(response_fd < 0){
			send_request_error(client_fd, 404);					
			exit(1);
		}	

		//Write and send header for HTTP OK response
		fprintf(stdout, "Beginning response transmission...");
		response_buffer_size = snprintf(NULL, 0, response_ok, http_version, content_type);
		response_buffer = (char*)malloc(response_buffer_size+1);
		snprintf(response_buffer, response_buffer_size+1, response_ok, http_version, content_type);	
		write(client_fd, response_buffer, strlen(response_buffer));
		
		//Break down data transmission in smaller packets for larget files
		current_data_size = 0;
		while(read(response_fd, &current_char, 1) > 0)	{				
			response_data[current_data_size] = current_char;
			current_data_size++;

			if(current_data_size == MAX_DATA_SIZE){
				write(client_fd, response_data, MAX_DATA_SIZE);
				current_data_size = 0;
			}			
		}			
		if(current_data_size != 0){
			write(client_fd, response_data, current_data_size);	
		}
		fprintf(stdout, "completed.\n");
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
		client_fd = accept(sock_fd, (struct sockaddr *) &client_addr, &client_addr_size);  	
		fprintf(stdout,"Client attempting to connect to server...");
		
		if (client_fd == -1) {
			fprintf(stderr, "\nUnable to accept new connection.\n");
		}			

		fprintf(stdout,"connection established.\n");
         
		pid = fork(); 
		if (pid < 0) // Error
		    error("ERROR on fork");
		 
		if (pid == 0)  { // child process
		    close(sock_fd);
		    handle(client_fd);			       		    
		    exit(0);
		}
		else{ // parent process
			fprintf(stdout,"Closing client connection...");
		    close(client_fd);  
		    fprintf(stdout,"closed.\n");
		}
	}
}

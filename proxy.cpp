#include "stdio.h"
#include "netdb.h"
#include "string.h"
#include "stdlib.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "unistd.h"
#include <errno.h>

#define BUFSIZE 30000
#define LISTENQ 1024


/*
 * Function prototypes
 */
int handleRequest(int server_fd); //get resource from server, save to file
int parse_uri(char *uri, char *target_addr, int  *port); //parse uri
void error(int client_fd);
ssize_t socket_write(int fd, char* write_buf, size_t n);
ssize_t socket_read(int fd, char* write_buf, size_t n);
int host_not_found = 0;
int open_listen_fd(int port);
int open_client_fd(char* hostname, int port);

//global variables
int port;
char buf[BUFSIZE];
char uri[BUFSIZE];
char version[BUFSIZE];
char method[BUFSIZE];
char hostname[BUFSIZE];
int client_fd, server_fd;
size_t n;
char log_string[BUFSIZE];
const char* error_str = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
const char* http200 = "HTTP/1.1 200 Connection Established\r\nProxy-agent: DennyProxy1.0\r\n\r\n";


int open_listen_fd(int port) 
{
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;
 
    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* Listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
}


int open_client_fd(char *hostname, int port) 
{
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1; /* check errno for cause of error */

    /* Fill in the server's IP address and port */
    if ((hp = gethostbyname(hostname)) == NULL)
        return -2; /* check h_errno for cause of error */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0], 
          (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);

    /* Establish a connection with the server */
    if (connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;
    return clientfd;
}


ssize_t socket_write(int fd, char* write_buf, size_t n)
{
    size_t size_to_write = n;
    ssize_t size_written = 0;
    char* buf_pos = write_buf;

    while (size_to_write > 0)
    {
	if ( (size_written = write(fd, buf_pos, size_to_write) ) <= 0 )
	{
	    if (errno == EINTR)
		size_written = 0;
	    else
		return -1;

	}

	size_to_write -= size_written;
	buf_pos += size_written;

    }

    return size_written;
}

ssize_t socket_read(int fd, char* read_buf, size_t n)
{
    size_t left_to_read = n;
    if (left_to_read > BUFSIZE)
	left_to_read = BUFSIZE - 1;

    ssize_t size_read = 0;
    char* buf_ptr = read_buf;

    
    while (left_to_read > 0)
    {
	if( (size_read = read(fd, buf_ptr, left_to_read)) < 0 )
	{
	    if (errno == EINTR)
		size_read = 0;
	    else
		return -1;
	}

	else if (size_read == 0)
	    break;

    
	left_to_read -= size_read;
	buf_ptr += size_read;
    }
    
    return (n - left_to_read);
}

void error(int client_fd)
{
	//Rio_writen(client_fd, error_str, strlen(error_str));
	socket_write(client_fd, (char*)error_str, strlen(error_str));
	

	close(client_fd);
	printf("%s", log_string);
	exit(0);
}

int main(int argc, char **argv)
{
        int listen_fd, clientlen;
        struct sockaddr_in clientaddr;

	port = 8080; //hardcoded port

	listen_fd = open_listen_fd(port);   //open listening descriptor
	int x;
	//forever alive
        while(1) 
	{
		//listen for client connection
                clientlen = sizeof(clientaddr);
                client_fd = accept(listen_fd, (struct sockaddr *)&clientaddr, (socklen_t *) &clientlen);
              	if (fork() == 0)
		{
		    	strcat(log_string, "conn: ");
			close(listen_fd); //in child process
			bzero(buf, sizeof(char) * BUFSIZE);
			x = recv(client_fd, buf, sizeof(char) * BUFSIZE, 0);
			
			if (x < 0)
			{
				strcat(log_string, "fail 1\n");
				error(client_fd);
			}
			printf("%s", buf);
			sscanf(buf, "%s %s %s", method, uri, version);  //scan input from client and extract method, uri, and version
			strcat(log_string, "Method - ");
			strcat(log_string, method);
			strcat(log_string, ": ");
			// proxy can only accepts GET method
	        		
			    int parse_rtn = parse_uri(uri, hostname, &port);  //call parse_uri to extract host name, path name, and port
			    if (parse_rtn < 0 || hostname == NULL) //check for parse errors
			    {
				    strcat(log_string, "parse uri error\n\"");
				    strcat(log_string, buf); strcat(log_string, "\"");
				    error(client_fd);
			    }
	    
		        if (strcmp(method, "CONNECT") == 0)
        	        {
			        //printf("hostname: %s\nport: %d\n", hostname, port);
				if ((server_fd = open_client_fd(hostname, port)) < 0)
				{
					strcat(log_string, "bad connection to server\n");
					close(server_fd);
					error(client_fd);
				}
				
				char message[1]; //buffer for server response
				size_t x;
				//Rio_writen(server_fd, buf, strlen(buf));
				//Rio_writen(server_fd, "\r\n\r\n", 4);
				//Rio_writen(client_fd, http200, strlen(http200));
				socket_write(client_fd, (char*) http200, strlen(http200));
				strcat(log_string, "Staring SSL proxy...\n");
				printf("%s", log_string);
				//fork for client--> server
				if(fork() == 0)
				{
				    //while ( (x = Rio_readn(client_fd, message, sizeof(char))) > 0)
				    while ( (x = socket_read(client_fd, message, sizeof(char))) > 0)
				    {
					    //Rio_writen(server_fd, message, x);
					    socket_write(server_fd, message, x);
				    }
				    //printf("done");
				    close(client_fd);
				    close(server_fd);
				    exit(0);

				}
				else //server --> client
				{
				    //while ( (x = Rio_readn(server_fd, message, sizeof(char))) > 0)
				    while ( (x = socket_read(server_fd, message, sizeof(char))) > 0)
				    {
					    //Rio_writen(client_fd, message, x);
					    socket_write(client_fd, message, x);
				    }
				    close(client_fd);
				    close(server_fd);
				    exit(0);
				}
			}
	    //if we can resolve the host name, attempt to connect to the server
			    else if ((server_fd = open_client_fd(hostname, port)) < 0)
			    {
				    strcat(log_string, "bad connection to server\n");
				    close(server_fd);
				    error(client_fd);
			    } 
			    //if we get here, we have a connection to the server
			    //  now we just need to get the resource
			    else
			    {
//					printf("pre handleReq\n");
				    //handeRequest forward request to server and saves the response
				    if (handleRequest(server_fd) < 0)
				    {
					    strcat(log_string, "Error in Handling Request\n");
					    close(server_fd);
					    error(client_fd);
				    }
//						printf("post handleReq\n");
					
				    close(server_fd); //finished with the server
				    close(client_fd); //finished with this connection
				    strcat(log_string, "Success!\n");
				    printf("%s", log_string);
				    exit(0); //finished with everything (for this child)
			    }

		}
		else//parent
		{
                	close(client_fd);  //close connection to client (we are in parent)
                }
                    	    	
        }//while 1


    exit(0);   
}


/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least BUFSIZE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, int *port)
{
    char *hostbegin;
    char *hostend;
    int len;


    memset(hostname, 0, sizeof(hostname));
    //pathname[0] = NULL;
    if (strncasecmp(uri, "http://", 7) == 0) {
        hostbegin = uri + 7;
    }
    else if (strncasecmp(uri, "https://", 8) == 0) {
	hostbegin = uri + 8;
    }
    else
	hostbegin = uri;
       
    /* Extract the host name */
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    if (hostend == NULL)
	    return -1;
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
        *port = atoi(hostend + 1);
    
    return 0;
}


int handleRequest(int server_fd)
{
	char message[1]; //buffer for server response
    	size_t x;
	//Rio_writen(server_fd, buf, strlen(buf));
	socket_write(server_fd, buf, strlen(buf));
	//Rio_writen(server_fd, "\r\n", 2);
	socket_write(server_fd, (char*)"\r\n", 2);

	//while ( (x = Rio_readn(server_fd, message, sizeof(char))) > 0)
	while ( (x = socket_read(server_fd, message, sizeof(char))) > 0)
	{
		//Rio_writen(client_fd, message, x);
		socket_write(client_fd, message, x);
       	}
    return 0;
}

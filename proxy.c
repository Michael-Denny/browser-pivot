#include "csapp.h"
#include "stdio.h"
#include "netdb.h"
#include "string.h"
#include "stdlib.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "unistd.h"

#define BUFSIZE 30000


/*
 * Function prototypes
 */
int handleRequest(int server_fd); //get resource from server, save to file
int parse_uri(char *uri, char *target_addr, int  *port); //parse uri
void error(int client_fd);

int host_not_found = 0;

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
char* error_str = "HTTP/1.1 500 Internal Server Error";


void error(int client_fd)
{
	Rio_writen(client_fd, error_str, strlen(error_str));
	Rio_writen(client_fd, "\r\n\r\n", 4);
	close(client_fd);
	printf("%s", log_string);
	exit(0);
}

int main(int argc, char **argv)
{
        int listen_fd, clientlen;
        struct sockaddr_in clientaddr;

	port = 12345; //hardcoded port

	listen_fd = Open_listenfd(port);   //open listening descriptor
	int x;
	//forever alive
        while(1) 
	{
		//listen for client connection
                clientlen = sizeof(clientaddr);
                client_fd = Accept(listen_fd, (SA *)&clientaddr, (socklen_t *) &clientlen);
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
			//printf("%s", buf);
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
				if ((server_fd = Open_clientfd(hostname, port)) < 0)
				{
					strcat(log_string, "bad connection to server\n");
					close(server_fd);
					error(client_fd);
				}
				
				char message[1]; //buffer for server response
				size_t x;
				//Rio_writen(server_fd, buf, strlen(buf));
				//Rio_writen(server_fd, "\r\n\r\n", 4);
				char* http200 = "HTTP/1.1 200 Connection Established\r\nProxy-agent: DennyProxy1.0\r\n\r\n";
				Rio_writen(client_fd, http200, strlen(http200));
				strcat(log_string, "Staring SSL proxy...\n");
				printf("%s", log_string);
				//fork for client--> server
				if(fork() == 0)
				{
				    while ( (x = Rio_readn(client_fd, message, sizeof(char))) > 0)
				    {
					    Rio_writen(server_fd, message, x);
				    }
				    //printf("done");
				    close(client_fd);
				    close(server_fd);
				    exit(0);

				}
				else //server --> client
				{
				    while ( (x = Rio_readn(server_fd, message, sizeof(char))) > 0)
				    {
					    Rio_writen(client_fd, message, x);
				    }
				    close(client_fd);
				    close(server_fd);
				    exit(0);
				}
			}
	    //if we can resolve the host name, attempt to connect to the server
			    else if ((server_fd = Open_clientfd(hostname, port)) < 0)
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
					
				    Close(server_fd); //finished with the server
				    Close(client_fd); //finished with this connection
				    strcat(log_string, "Success!\n");
				    printf("%s", log_string);
				    exit(0); //finished with everything (for this child)
			    }

		}
		else//parent
		{
                	Close(client_fd);  //close connection to client (we are in parent)
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
	Rio_writen(server_fd, buf, strlen(buf));
	Rio_writen(server_fd, "\r\n", 2);

	while ( (x = Rio_readn(server_fd, message, sizeof(char))) > 0)
	{
		Rio_writen(client_fd, message, x);
       	}
    return 0;
}

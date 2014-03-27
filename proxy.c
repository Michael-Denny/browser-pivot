
/* CS:APP Web proxy
 *
 * TEAM MEMBERS: 
 *     Chris Lott, cnlo223@uky.edu 
 *     Michael Denny, msdenn3@g.uky.edu 
 
 	Caching (both dns and page) are done using linked lists of structs.

	This program opens up a socket on port 15213 and waits for connections.

	When it recieves a connection, it does 5 things in this order:
		1.)Get request from client
		2.)Parse request, checking for correct format as well as extracting information
		3.)Check page cache
		4.)If not in page cache, check dns cache
		5.)Fork a child to handle getting page from server (if nessecary) and
			to handle sending the webpage back to the client

	Web cache files are simply numbered. The relation between hostname/pathname and filename is
		tracked using a linked list (of webPageCache structs)
 
 
 */ 


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


int main(int argc, char **argv)
{
        int listen_fd, clientlen;
        struct sockaddr_in clientaddr;

	port = 8080; //hardcoded port

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
			close(listen_fd); //in child process
			
			bzero(buf, sizeof(char) * BUFSIZE);
			x = recv(client_fd, buf, sizeof(char) * BUFSIZE, 0);

			if (x < 0)
			{
				close(client_fd);
				exit(0);
			}

			sscanf(buf, "%s %s %s", method, uri, version);  //scan input from client and extract method, uri, and version

			// proxy can only accepts GET method
	                if (strcmp(method, "GET") != 0)
        	        {
//              	        printf("Invalid Method:%s\n",method);
				close(client_fd);
				exit(0); //child
                	}
		
			else //GET request
			{
	        	        int parse_rtn = parse_uri(uri, hostname, &port);  //call parse_uri to extract host name, path name, and port
				if (parse_rtn < 0 || hostname == NULL) //check for parse errors
				{
					printf("error\n");
					close(client_fd);
					exit(0); //child
				}
		
				//if we can resolve the host name, attempt to connect to the server
				else if ((server_fd = Open_clientfd(hostname, port)) < 0)
				{
					host_not_found = 1;
                       	    		printf("bad connection to server. serverfd:%d\n", server_fd);
					close(client_fd);
					exit(0); //child
         		    	}
				//if we get here, we have a connection to the server
				//  now we just need to get the resource
		    		else
				{
//					printf("pre handleReq\n");
					//handeRequest forward request to server and saves the response
                     			if (handleRequest(server_fd) < 0)
					{
	                       			host_not_found = 1;
						printf("Error Handling Request");
	                	        }
//						printf("post handleReq\n");
        				    
					Close(server_fd); //finished with the server
					Close(client_fd); //finished with this connection
					exit(0); //finished with everything (for this child)
				}

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
    if (strncasecmp(uri, "http://", 7) != 0) {
        hostname[0] = '\0';
        return -1;
    }
       
    /* Extract the host name */
    hostbegin = uri + 7;
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

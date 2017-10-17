#include "csapp.h"

/*
*Cody Burchett
*cburchet
*
* simple proxy server capable of handling multiple concurrent requests
*/

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void doit(int fd);
int parse_uri(char *uri, char *hostName, char *portNum, char* pathName); 
void read_requesthdrs(rio_t *rp, char* host, char* headers);
void* threadInstructions(void* arg);
void StartRequestHeader(char* method, char* address, char* requestBuffer);
void ReadRequestHeaders(rio_t* rp, char* host, char* headers);
void ServerResponse(rio_t* rp, int serverfd, int fd);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";



int main(int argc, char** argv)
{
	int* connfd;;
	int listenfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
	pthread_t threadId;
	
	Signal(SIGPIPE,SIG_IGN);
	
    /* Check command line args */
    if (argc != 2) 
	{
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
    }
	
	//open listener connection
	listenfd = Open_listenfd(argv[1]);
	if(listenfd < 0)
	{
		printf("%d Open_listenfd failed\n", listenfd);
		exit(1);
	}
	while (1) 
	{
		connfd = (int*)malloc(sizeof(int));
		*connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
		if (connfd >= 0)
		{
			//create new thread to handle request
			Pthread_create(&threadId, NULL, threadInstructions, connfd); 
		}   
		else
		{
			printf("Connection %d not accepted\n", *connfd);
		}			
	}
    return 0;
}

//instructions for each thread to follow
void* threadInstructions(void* arg)
{
	int fd = *(int*)(arg);
	Pthread_detach(Pthread_self());
	free(arg);
	doit(fd);
	Close(fd);
	return NULL;
}

//main body: connect to server, parse the request line,
//headers, and the server's response
void doit(int fd) 
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE];
	char portNumber[MAXLINE], address[MAXLINE];
	int serverfd = -1;
    rio_t rio, serverRio;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
	int parseRet = -1;
	int methodRet = -1;
	//loop while input is put in incorrectly
	while(parseRet < 0 || methodRet < 0 || serverfd < 0)
	{
		if (!Rio_readlineb(&rio, buf, MAXLINE))  
		{
			printf("no readlineb!\n");
		}
		sscanf(buf, "%s %s %s", method, uri, version);   
		printf("%s", buf);
		if (strncasecmp(method, "GET", 3)) 
		{                     
			printf("Not Implemented. Proxy does not implement this method");
			methodRet = -1;
		}
		else
		{
			methodRet = 0;
		}			

		/* Parse URI from GET request */
		parseRet = parse_uri(uri, filename, portNumber, address);
		if (parseRet < 0)
		{
			printf("parse failed");
		}
		//open connection to server
		if (parseRet >= 0 && methodRet >= 0)
		{
			serverfd = Open_clientfd(filename, portNumber);
			if (serverfd < 0)
			{
				printf("%d Open_clientfd didn't work\n", serverfd);
			}
		}
	}
	
	char requestBuffer[MAXLINE];
	char requestHeaders[MAXLINE];
	read_requesthdrs(&rio, filename, requestHeaders); 

	//build and write request headers 
	StartRequestHeader(method, address, requestBuffer);
	Rio_writen(serverfd, requestBuffer, strlen(requestBuffer));
	Rio_writen(serverfd, requestHeaders, strlen(requestHeaders));
    
	ServerResponse(&serverRio, serverfd, fd);
	
    Close(serverfd);
	return;
}

void ServerResponse(rio_t* rp, int serverfd, int fd)
{
	//read and write server's response
	Rio_readinitb(rp, serverfd);
	char responseBuffer[MAXLINE];
	int size;
	size = Rio_readnb(rp, responseBuffer, MAXLINE);
	while (size > 0) 
	{
        Rio_writen(fd, responseBuffer, size);
		size = Rio_readnb(rp, responseBuffer, MAXLINE);
    }
	return;
}

//start building a header with the method, path, http/1.0
void StartRequestHeader(char* method, char* address, char* requestBuffer)
{
	strcat(requestBuffer, method);
	strcat(requestBuffer, " ");
	strcat(requestBuffer, address);
	strcat(requestBuffer, " ");
	strcat(requestBuffer, "HTTP/1.0\r\n");
	return;
}

//parse the uri to get the host, port, and path
int parse_uri(char *uri, char *hostName, char *portNum, char* addressName)
{
	char* startHostName;
	char* startAddress;
	char* startPort;
	//skip http:// to start of useful string
	int offset = strlen("http://");
	if (strncasecmp(uri, "http://", offset))
	{
		printf("invalid uri: needs to start with http://\n");
		return -1;
	}
	startHostName = uri + offset;
	
	//check for path after host
	startAddress = strstr(startHostName, "/");
	char tempHostHolder[MAXLINE];
	if (startAddress)
	{
		//find # of chars between start of address and host
		int hostOffset = startAddress - startHostName;
		//copy address to addressName and host to the temp holder
		strcpy(addressName, startAddress);
		strncpy(tempHostHolder, startHostName, hostOffset);
		strcat(tempHostHolder, "\0");
	}
	else
	{
		strcpy(tempHostHolder, startHostName);
		strcpy(addressName, "/0");
	}
	//check for specific port
	startPort = strstr(tempHostHolder, ":"); 
	if(startPort)
	{
		//find # of chars between start of port and host
		int portOffset = startPort - tempHostHolder;
		//copy host to hostName and port to portNum
		strncpy(hostName, tempHostHolder, portOffset);
		strcat(hostName, "\0");
		strcpy(portNum, ++startPort);
	}
	//no port specified return the default port 80
	else
	{
		strcpy(portNum, "80"); 
		strcpy(hostName, tempHostHolder);
	}
	
	return 0;	
}

//build the request headers to pass along
void read_requesthdrs(rio_t *rp, char* host, char* headers) 
{
	//append required headers to passed in header
	strcat(headers, user_agent_hdr);
	strcat(headers, "Connection: close\r\n");
	strcat(headers, "Proxy-Connection: close\r\n");

	//read requests from client and append any new headers
	ReadRequestHeaders(rp, host, headers);
	//end with empty line
	strcat(headers, "\r\n");
	return;
}

void ReadRequestHeaders(rio_t* rp, char* host, char* headers)
{
	char headersBuffer[MAXLINE]; 
	int needsHost = 1;
	if (Rio_readlineb(rp, headersBuffer, MAXLINE) < 0)
	{
		printf("readlineb in ReadRequestHeaders failed\n");
		return;
	}		
	while (strcmp(headersBuffer, "\r\n"))
	{
		//check if headersBuffer contains new headers to be added to the return headers
		if(!strstr(headersBuffer, "User-Agent") && !strstr(headersBuffer, "Connection") && !strstr(headersBuffer, "Proxy-Connection"))
		{
			strcat(headers, headersBuffer);
			if(strstr(headersBuffer, "Host"))
			{
				needsHost = 0;
			}
		}
		if (Rio_readlineb(rp, headersBuffer, MAXLINE) < 0)
		{
			printf("readlineb in ReadRequestHeaders failed\n");
			return;
		}
	}
	//host was not specified append the host from parse_uri
	char hostHeader[MAXLINE];
	if(needsHost)
	{
		strcat(hostHeader, "Host: ");
		strcat(hostHeader, host);
		strcat(hostHeader, "\r\n");
		strcat(headers, hostHeader);
	}
}
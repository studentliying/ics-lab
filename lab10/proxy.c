/*
 * proxy.c - ICS Web proxy
 * ID: 516030910444 
 * Name: Li Ying
 */

#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>

/*
 * Function prototypes
 */

int parse_uri(char *uri, char *target_addr, char *path, char* port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);
void doit(int connfd, struct sockaddr_in clientaddr);
void *thread(void* vargp);

sem_t mutex_file;

struct info
{
	int connfd;
	struct sockaddr_in clientaddr;
};


/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
	int listenfd, connfd;
    char* port;
	socklen_t clientlen;
	struct sockaddr_in clientaddr;
	pthread_t tid;
    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

	signal(SIGPIPE, SIG_IGN);
	sem_init(&mutex_file, 0, 1);
	
	port = argv[1];
	listenfd = open_listenfd(port);
	while (1)
	{
		clientlen = sizeof(struct sockaddr_storage);
		connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
		struct info *mes = (struct info*)malloc(sizeof(struct info));
		mes->connfd = connfd;
		mes->clientaddr = clientaddr;
		pthread_create(&tid, NULL, thread, (void*)mes);
	}
	close(listenfd);
    exit(0);
}


void doit(int connfd, struct sockaddr_in clientaddr)
{
	char buf[MAXLINE], method[16], uri[MAXLINE], version[16];
	char hostname[256], pathname[MAXLINE], logstring[MAXLINE];
	char port[8];
	rio_t rio_client;
	rio_t rio_server;
    
	//read the request line from the client
	rio_readinitb(&rio_client, connfd);
	rio_readlineb(&rio_client, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);
	if (strcmp(version,"HTTP/1.1"))
		return;
	if (parse_uri(uri, hostname, pathname, port) < 0)
		return;
	sprintf(buf, "%s /%s %s\r\n",method,pathname,version);
	
	//establish the connection to the target server
	int clientfd= open_clientfd(hostname, port);
	if (rio_writen(clientfd, buf, strlen(buf)) <= 0)
	{
		close(clientfd);
		return;
	}
	
	int m = 0;
	int n = 0;
	int body = 0;
	int body_len = 0;
	int size = 0;
	
    //read headers from client and forward to server by line
	while ((n = rio_readlineb(&rio_client, buf, MAXLINE)))
	{
		if ((n <= 0) || (rio_writen(clientfd, buf, n)) <= 0)
		{
			close(clientfd);
			return;
		}
		if (!strncasecmp(buf, "Content-Length:", 15))
		{
			body = 1;
			sscanf(buf, "%*s %d", &body_len);		
		}
		if (!strcmp(buf, "\r\n")) 
			break;
	}
	strcpy(buf, "");

    //read body from client and forward to server by char
	if (body)
	{
		while (body_len > 0)
		{
			m = rio_readnb(&rio_client, buf, 1);
			if ((m <= 0)||(rio_writen(clientfd, buf, 1)) <= 0)
			{
				close(clientfd);
				return;
			}
			body_len -- ;
		}
	}

	//receive response from server and forward to the client
	rio_readinitb (&rio_server, clientfd);
	body = 0;
	body_len = 0;
	while ((n=rio_readlineb(&rio_server, buf, MAXLINE)))
	{
		if (n<=0 || rio_writen(connfd, buf, n) <= 0)
		{
			close(clientfd);
			return;
		}
		size += n;
		if(!strncasecmp(buf, "Content-Length:",15))
		{
			body = 1;
			sscanf(buf, "%*s %d", &body_len);
			size += body_len;
		}
		if (!strcmp(buf, "\r\n"))
			break;
	}

	// process body if header has "Content-Length"
	if(body)
	{
		while(body_len > 0)
		{
		   	m = rio_readnb(&rio_server, buf, 1);
			if  ((m <= 0)||(rio_writen(connfd, buf, 1) <= 0))
			{
				close(clientfd);
				return;
			}
			body_len --;
		}
	}

	P(&mutex_file);
	format_log_entry(logstring, &clientaddr, uri, size);
	printf("%s\n",logstring);
	V(&mutex_file);
	close(clientfd);
}


void *thread(void *vargp)
{
	struct info *mes = (struct info *)vargp;
	pthread_detach(pthread_self());
	doit(mes->connfd, mes->clientaddr);
	//free(mes);
	close(mes->connfd);
	free(mes);
	return NULL;
}


/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, char* port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

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
    if (*hostend == ':') {
        char *p = hostend + 1;
        while (isdigit(*p))
            *port++ = *p++;
        *port = '\0';
    } else {
        strcpy(port, "80");
    }

    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
        pathname[0] = '\0';
    }
    else {
        pathbegin++;
        strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), the number of bytes
 * from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                      char *uri, size_t size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /*
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 12, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %zu", time_str, a, b, c, d, uri, size);
}



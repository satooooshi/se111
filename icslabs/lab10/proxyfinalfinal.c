#include "csapp.h"
#include "stdlib.h"
#include "string.h"

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void serve(int connfd, struct sockaddr_in *client_addr);
void *thread(void *vargp);
void generate_request(char* method, char* pathname,
    char* version, char *hostname, char *request);
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
void Rio_writen_w(int fd, void *usrbuf, size_t n);
int Open_clientfd_ts(char *hostname, int port);

/* Definition of struct */
struct serve_arg{
    int connfd;
    struct sockaddr_in client_addr;
};

//Global variables
sem_t log_lock;//used in serve to lock the log file thread
sem_t dns_lock;//used in open_clientfd_ts to lock the thread
FILE *logfp;
/*
struct client{
    int fd;
    struct sockaddr_in sockaddr;
};
*/
/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    char* logfile = "proxy.log";
    int listenfd, connfd, port, client_len;
    struct sockaddr_in client_addr;
    pthread_t tid;

    /* Set lock */
    Sem_init(&dns_lock, 0, 1);
    Sem_init(&log_lock, 0, 1);

    /* Ignore SIGPIPE */
    Signal(SIGPIPE, SIG_IGN);

    /* Check arguments */
    if (argc != 2)
    {
	   fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	   exit(0);
    }

    port = atoi(argv[1]);

    /* Open log file */
    logfp = Fopen(logfile, "w");

    /* Listen for a client connection */
    listenfd = Open_listenfd(argv[1]);
    while(1)
    {
        client_len = sizeof(client_addr);
        connfd = Accept(listenfd, (SA *)&client_addr, &client_len);
        /* Create the thread */
        struct serve_arg *arg = (struct serve_arg *)malloc(sizeof(struct serve_arg));
        arg->connfd = connfd;
        arg->client_addr = client_addr;
        Pthread_create(&tid, NULL, thread, (void *)arg);
        //serve(connfd, &client_addr);
    }

    /* Close log file */
    Fclose(logfp);

    exit(0);
}

void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);       //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    //line:netp:servestatic:beginserve
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));       //line:netp:servestatic:endserve
    printf("Response headers:\n");
    printf("%s", buf);

    /* Send response body to client */
		/*
    srcfd = Open(filename, O_RDONLY, 0);    //line:netp:servestatic:open
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);//line:netp:servestatic:mmap
    Close(srcfd);                           //line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);         //line:netp:servestatic:write
    Munmap(srcp, filesize);                 //line:netp:servestatic:munmap
		*/
		srcfd = open(filename, O_RDONLY, 0);
		srcp = (char*)malloc(sizeof(char)*filesize);
		rio_readn(srcfd, srcp, filesize);
		close(srcfd);
		rio_writen(fd, srcp, filesize);
		free(srcp);
}

/*
 * serve - The server's way to tackle the request
 *
 * Read the HTTP request,
 * and parse it to determine the name of the end server.
 * Then open a connection to the end server,
 * send it the request, receive the reply,
 * and forward the reply to the browser
 * if the request is not blocked
 */
void serve(int connfd, struct sockaddr_in *client_addr)
{
    char buf[MAXLINE]={0}, uri[MAXLINE], hostname[MAXLINE],
        pathname[MAXLINE], request[MAXLINE], response[MAXLINE],
        method[MAXLINE], version[MAXLINE], logstring[MAXLINE];
    int port, clientfd, size = 0;
    rio_t rio_request, rio_response;

    //printf("start serve\n");

    /* Read the HTTP request */
    Rio_readinitb(&rio_request, connfd);
    Rio_readlineb_w(&rio_request, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    fprintf(stderr,"serve:%s\n",buf);

    /* Parse the uri to determine the end server */
    if(parse_uri(uri, hostname, pathname, &port) == -1)
    {
        unix_error("parse_uri error");
        return;
    }

    /* Open a connection to the end server as a client */
    clientfd = Open_clientfd_ts(hostname, port);

    fprintf(stderr,"start send request\n");
    /* Generate the request and send it to the end server */
    generate_request(method, pathname, version, hostname, request);
    fprintf(stderr,"request:%s",request);
    Rio_writen_w(clientfd, request, strlen(request));
    while(Rio_readlineb_w(&rio_request, buf, MAXLINE) > 2)//"\r\n".length==2
    {
        Rio_writen_w(clientfd, buf, strlen(buf));
        fprintf(stderr,"request:%s",buf);

    }
    sprintf(buf,"\r\n");
    Rio_writen_w(clientfd, buf, strlen(buf));
    fprintf(stderr,"request:%s",buf);


    int has_body_len=0;
    int body_len=0;
    /* Receive the response and forward to the browser */
    int temp;
    Rio_readinitb(&rio_response, clientfd);
    fprintf(stderr,"start forward response\n");
    /* Read header */
    while((temp = Rio_readlineb_w(&rio_response, response, 1024)) != 2)//"\r\n".length==2
    {
      if (!strcmp(buf, "\r\n")) {
        break;
      }
      
      if (!strncasecmp(buf, "Content-Length", 14)) {
        has_body_len = 1;
        sscanf(buf, "%*s %d", &body_len);
        fprintf(stderr, "Body Length = %d\n", body_len);
      }

        Rio_writen_w(connfd, response, temp);
        fprintf(stderr,"reponse:%s",response);
        size += temp;
    }
    Rio_writen_w(connfd,"\r\n",2);
    size+=2;


    //serve_static(int fd, char *filename, int filesize)



    format_log_entry(logstring, client_addr, uri, size);
    fprintf(stderr,"log:%s\n", logstring);

    Close(clientfd);

    /* write log */
    //printf("start write log\n");

    P(&log_lock);
    Fwrite(logstring, sizeof(char), strlen(logstring), logfp);
    fflush(logfp);
    V(&log_lock);



    //printf("end\n");
}

/* Thread function */
void *thread(void *vargp)
{
    printf("thread %d take this job\n", (int)Pthread_self());
    struct serve_arg *arg = (struct serve_arg *) vargp;
    int connfd = arg->connfd;
    struct sockaddr_in client_addr = arg->client_addr;
    free(vargp);

    /* Detach itself to collect resourses */
    Pthread_detach(Pthread_self());

    serve(connfd, &client_addr);

    Close(connfd);
}

/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0)
    {
	   hostname[0] = '\0';
	   return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");

    /* When there's no path and port
     * hostend will be NULL(0)
     * Then, len cannot be calculated by (hostend - hostbegin)
     */
    if (hostend == NULL)
    {
        strcpy(hostname, hostbegin);
        *port = 80;
        pathname[0] = '\0';
        return 0;
    }

    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')
    {
	   *port = atoi(hostend + 1);
    }

    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL)
    {
	   pathname[0] = '\0';
    }

    else
    {
	   pathbegin++;
	   strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
		      char *uri, int size)
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
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", time_str, a, b, c, d, uri, size);
}


/*
 * generate_request - generate a http request
 *
 * <method> /<pathname> <version>\r\n
 * Host: <hostname>\r\n
 * \r\n
 */
void generate_request(char* method, char* pathname, char* version,
    char *hostname, char *request)
{
    sprintf(request, "%s /%s %s\r\n", method, pathname, version);
}

/* My rio_read and write */
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n)
{
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0)
    {
        fprintf(stderr, "Rio_readnb_w error: %s\n", strerror(errno));
        return 0;
    }

    return rc;
}

void Rio_writen_w(int fd, void *usrbuf, size_t n)
{
    if (rio_writen(fd, usrbuf, n) != n)
    {
        fprintf(stderr, "Rio_writen_w error: %s\n", strerror(errno));
    }
}

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen)
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
    {
        fprintf(stderr, "Rio_readlineb_w error: %s\n", strerror(errno));
        return 0;
    }

    return rc;
}

int Open_clientfd_ts(char *hostname, int port)
{
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;
    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        return -1;
    }

    P(&dns_lock);//lock before build ad server socket
    if ((hp = gethostbyname(hostname)) == NULL)
    {
        V(&dns_lock);
        return -2;
    }

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    bcopy((char *)hp->h_addr_list[0],(char *) &serveraddr.sin_addr.s_addr, hp->h_length);
    V(&dns_lock);

    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) <0)
    {
        return -1;
    }
    return clientfd;
}

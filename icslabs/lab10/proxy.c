// gcc -o tiny tiny.c csapp.c -lpthread
// gcc -o adder adder.c
// ./tiny 1024

/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"

#define BUF_SIZE 102400

int parse_uri(char *uri, char *target_addr, char *path, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);
void doit(int fd, struct sockaddr_in *clientaddr);
int read_n_serve_requesthdrs(rio_t *rio_request, int cliendfd, char* method, char* uri, char* version);
int receive_n_forward_responsehdrs(rio_t *rio_response, int connfdh, char* method, char* pathname, char* version);
int receive_n_forward_responsebody(rio_t *rio_response, int connfd, int contentLengt);
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg);
int Open_clientfd_ts(char *hostname, int port);

int logfd;

		 /*
		  * parse_uri - URI parser
		  *
		  * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
		  * the host name, path name, and port.  The memory for hostname and
		  * pathname must already be allocated and should be at least MAXLINE
		  * bytes. Return -1 if there are any problems.
		  */
		 int parse_uri(char *uri, char *hostname, char *pathname, char *port)
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
		         //pathbegin++;
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
		     sprintf(logstring, "%s: %d.%d.%d.%d %s %zu\n", time_str, a, b, c, d, uri, size);
		 }



void sigchld_handler(int sig){//***************************************************11.7

	printf("Caught sigchild\n");

	waitpid(-1, NULL, 0);

	return;// return to the caller
}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
		logfd=Open("proxy.log",O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);

		signal(SIGCHLD,sigchld_handler);//*******************************************11.7

    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE,
                    port, MAXLINE, 0);
        fprintf(stderr,"Accepted connection from (%s, %s)\n", hostname, port);
	doit(connfd, &clientaddr);                                             //line:netp:tiny:doit
	Close(connfd);                                            //line:netp:tiny:close
    }

		Close(logfd);
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
#define M_GET 0
#define M_POST 1
#define M_HEAD 2
#define M_NONE (-1)
void doit(int connfd, struct sockaddr_in *clientaddr)
{
    int rmtd = 0;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE],hostname[MAXLINE],pathname[MAXLINE],port[MAXLINE];
    rio_t rio_request, rio_response;
		int clientfd;

    /*for post*/
    int contentLen;
    char post_content[MAXLINE];

    /* Read request line and headers */
    Rio_readinitb(&rio_request, connfd);
    if (!Rio_readlineb(&rio_request, buf, MAXLINE))  //line:netp:doit:readrequest
        return;

    sscanf(buf, "%s %s %s", method, uri, version);
		//fprintf(stderr,"m:%s u:%s v:%s\n", method, uri, version);

			parse_uri(uri, hostname, pathname, port);



			/* Open a connection to the end server as a client */
			clientfd = Open_clientfd(hostname, port);
			/*Send request header to End-server */
    	read_n_serve_requesthdrs(&rio_request, clientfd, method, uri, version);

			/* Recieve response line and headers from End-server */
		  Rio_readinitb(&rio_response, clientfd);
			/* Receive the response from End-erver and forward to the browser */
			contentLen=receive_n_forward_responsehdrs(&rio_response, connfd,method,pathname,version);

			/* Recieve response body from End-server if Content-Length > 0 */
			if(contentLen>0)
				receive_n_forward_responsebody(&rio_response, connfd,contentLen);

				char *logstring[MAXLINE];
				format_log_entry(logstring, &clientaddr, uri, contentLen);
				Rio_writen(logfd,logstring,strlen(logstring));
				fprintf(stderr,"log:\n%s",logstring);
				fprintf(stderr,"Completed.\n");
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
int read_n_serve_requesthdrs(rio_t *rio_request, int clientfd, char* method, char* uri, char* version)
{
    char buf[MAXLINE],buf_write[MAXLINE];

		fprintf(stderr,"request start:%s %s %s\n", method, uri, version);
		sprintf(buf_write, "%s %s %s\r\n", method, uri, version);

		while(Rio_readlineb(rio_request, buf, MAXLINE)>2){

			sprintf(buf_write, "%s%s", buf_write, buf);//append buf to buf_write
		}
		sprintf(buf_write, "%s%s", buf_write, buf);//append \r\n
		fprintf(stderr,"<<request header>>\n%s",buf_write);
		Rio_writen(clientfd, buf_write, strlen(buf_write));

		return 0;
}

int receive_n_forward_responsehdrs(rio_t *rio_response, int connfd, char *method, char *pathname, char *version){
		int temp;
 		int size;
		int contentLength = 0;
		int has_body_len=0;
 		char response[MAXLINE],buf[MAXLINE];
 		 fprintf(stderr,"start forward response...\n");

		 /* Read header */
		Rio_readlineb(rio_response, buf, MAXLINE);
		sprintf(response,"%s",buf);
 		while((temp = Rio_readlineb(rio_response, buf, MAXLINE)) != 2)//"\r\n".length==2
 		{
			sprintf(response, "%s%s", response, buf);

			if (!strncasecmp(buf, "Content-Length", 14)) {
				has_body_len = 1;
				sscanf(buf, "%*s %d", &contentLength);
				fprintf(stderr, "response Body-Length = %d\n", contentLength);
			}

 		}
		sprintf(response, "%s%s", response, buf);// append \r\n
		size=strlen(response);
		fprintf(stderr,"<<reponsed header>>\n%s",response);
		Rio_writen(connfd, response, size);
		return (contentLength>0)?contentLength:(-1);
	}
/* $end read_requesthdrs */
int receive_n_forward_responsebody(rio_t *rio_response, int connfd, int contentLength){
		int temp;
 		int remainingLength=contentLength;
 		char buf[BUF_SIZE];
 		 fprintf(stderr,"start forward response body...\n");

		 //Rio_readlineb(rio_response, buf, MAXLINE);//
		 //fprintf(stderr,"<body>:%s",buf);

		 while(remainingLength>0){
			 temp=Rio_readnb(rio_response, buf, contentLength);
			 remainingLength-=temp;
			 fprintf(stderr,"temp:%d",temp);
			  Rio_writen(connfd,buf,strlen(buf));
				fprintf(stderr,"body:%s",buf);
		 }
		 Rio_writen(connfd,"\r\n",2);//***************************************************\0 added

		return remainingLength;
	}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

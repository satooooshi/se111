#include "csapp.h"

#define BUF_SIZE 102400
#define ADDR_SIZE sizeof(struct sockaddr_in)

/*
 * Function prototypes
 */
int parse_uria(char *uri, char *target_addr, char *path, int *port);
//int parse_uri(char *uri, char *hostname, char *pathname, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri,
                      int size);

int Open_clientfd_ts(char *hostname, int port) {
  static pthread_mutex_t *mutex = NULL;

  if (mutex == NULL) {
    mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mutex, 0);
  }
  pthread_mutex_lock(mutex);
  fprintf(stderr, "locked\n");
  fprintf(stderr, "hostname %s port %d\n", hostname, port);

  char *chport[128]={0};//safe cast from (int)port to (char*)chport
  sprintf(chport,"%d",port);

  //
  //fprintf(stderr,"chport:%s\n",chport);
  int result = Open_clientfd(hostname, chport);
  fprintf(stderr, "unlocked\n");
  pthread_mutex_unlock(mutex);
  return result;
}

void print_log_ts(const char *buf) {
  static pthread_mutex_t *mutex = NULL;
  if (mutex == NULL) {
    mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mutex, 0);
  }
  pthread_mutex_lock(mutex);
  printf("%s\n", buf);
  fflush(stdout);
  pthread_mutex_unlock(mutex);
}

void doit(struct sockaddr_in *clientaddr, int src_fd) {
  char buf[BUF_SIZE], buf_write[BUF_SIZE];

  memset(buf_write, 0, BUF_SIZE);
  char hostname[1024], pathname[1024], uri[1024];
  int port;
  rio_t src, dest;
  int len, total_len;
  Rio_readinitb(&src, src_fd);
  len = Rio_readlineb(&src, buf, MAXLINE);

  char method[1024],version[1024];
  //sscanf(buf, "%*s %s", uri);
  sscanf(buf,"%s %s %s",method,uri,version);
  fprintf(stderr,"doit:uri:%s\n",uri);

  parse_uria(uri, hostname, pathname, &port);


  sprintf(buf,"%s %s %s\n",method, pathname, version);
  fprintf(stderr,"doit:pathname:%s\r\n",buf);

  total_len=0;
  // Request
   do{
    // ignore these headers
    if (!strncasecmp(buf, "Connection", 10) ||
        !strncasecmp(buf, "Proxy-Connection", 16)) {
      continue;
    }
    if (!strcmp(buf, "\r\n")) {
      //memcpy(buf_write + total_len, "\r\n", 2);
      //total_len += 2;
      break;
    }

    fprintf(stderr, "Write: %s\n", buf);
    memcpy(buf_write + total_len, buf, len);
    total_len += len;
  }while ((len = Rio_readlineb(&src, buf, BUF_SIZE)) > 0);




  const char *end = "Connection: close\r\nProxy-Connection: close\r\n\r\n";
  len = strlen(end);
  memcpy(buf_write + total_len, end, len);
  total_len += len;

  //
  //fprintf(stderr,"doit:port:%d",port);
  int dest_fd = Open_clientfd_ts(hostname, port);
  Rio_writen(dest_fd, buf_write, total_len);
  fprintf(stderr, "---Echo Request--\n");
  fprintf(stderr, "%s", buf_write);
  fprintf(stderr, "---\n");
  //-------

  // Response Header
  Rio_readinitb(&dest, dest_fd);
  int response_size = 0;
  total_len = 0;
  int body_len = -1;
  int has_body_len = 0;
  memset(buf_write, 0, BUF_SIZE);
  while ((len = Rio_readlineb(&dest, buf, BUF_SIZE))) {
    fprintf(stderr, "Read: %s", buf);
    memcpy(buf_write + total_len, buf, len);
    total_len += len;

    if (!strncasecmp(buf, "Content-Length", 14)) {
      has_body_len = 1;
      sscanf(buf, "%*s %d", &body_len);
      fprintf(stderr, "Body Length = %d\n", body_len);
    }

    if (!strcmp(buf, "\r\n")) {
      break;
    }
  }
  Rio_writen(src_fd, buf_write, total_len);
  fprintf(stderr, "---Echo Response header---\n");
  fprintf(stderr, buf_write, total_len);
  fprintf(stderr, "---\n");
  response_size = total_len;
  total_len = 0;
  fprintf(stderr, "--\n");

  if (body_len > 0) {
    while (body_len > 0) {
      int len_to_read = BUF_SIZE - 1;
      if (body_len < len_to_read)
        len_to_read = body_len;
      len = Rio_readnb(&dest, buf, len_to_read);
      Rio_writen(src_fd, buf, len);
      total_len += len;
    }
  } else {
    while ((len = Rio_readlineb(&dest, buf, BUF_SIZE)) > 0) {
      fprintf(stderr, "Read: %s", buf);
      total_len += len;
      Rio_writen(src_fd, buf, len);
    }
  }
  response_size += total_len;
  fprintf(stderr, "\n--\nCompleted\n");
  format_log_entry(buf, clientaddr, uri, response_size);
  Close(dest_fd);
  Close(src_fd);
  print_log_ts(buf);
}

void *thread_main(void *args) {
  pthread_detach(pthread_self());
  int src_fd = *((char *)args + ADDR_SIZE);
  doit((struct sockaddr_in *)args, src_fd);
  free(args);
  return NULL;
}

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv) {
  freopen("proxy.log", "w", stdout);
  /* Check arguments */
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
    exit(0);
  }
  int listenfd, connfd;
  int port = atoi(argv[1]);
  struct sockaddr_in clientaddr;

  listenfd = Open_listenfd(argv[1]);

  while (1) {
    socklen_t clientlen = ADDR_SIZE;
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    void *args = malloc(ADDR_SIZE + sizeof(int));
    memcpy(args, &clientaddr, ADDR_SIZE);
    memcpy((char *)args + ADDR_SIZE, &connfd, sizeof(int));

    pthread_t tid;
    pthread_create(&tid, NULL, thread_main, args);
    // thread_main(args);
  }
  exit(0);
}

/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uria(char *uri, char *hostname, char *pathname, int *port) {
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
  len = hostend - hostbegin;
  strncpy(hostname, hostbegin, len);
  hostname[len] = '\0';

  /* Extract the port number */
  *port = 80; /* default */
  if (*hostend == ':')
    *port = atoi(hostend + 1);

  /* Extract the path */
  pathbegin = strchr(hostbegin, '/');
  if (pathbegin == NULL) {
    pathname[0] = '\0';
  } else {
    //pathbegin++;
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
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri,
                      int size) {
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
  sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}


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
        pathbegin++;
        strcpy(pathname, pathbegin);
    }

    return 0;
}

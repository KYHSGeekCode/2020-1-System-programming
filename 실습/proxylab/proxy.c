#include <stdio.h>
#include <string.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";
static const char *connection_close_hdr = "close";
static const char *proxy_connection_close_hdr = "close";

static const char *connection = "Connection";
static const char *proxy_connection = "Proxy-Connection";
static const char *user_agent = "User-Agent";
static const char *host = "Host";
struct httpParam {
    char * field;
    char * value;
    struct httpParam * next;
};

struct parsedRequest {
    char * uri;
    char * httpVersion;
    char * host;
    char has_user_agent;
    char has_connection;
    char has_proxy_connection;
    char has_host;
    struct httpParam * params;
};

struct response {
    char * uri;
    char * data;
    int length;
};

int doProxy(int fd);
char *trimwhitespace(char *str);
struct parsedRequest parseRequest(int fd);
struct response * execRequest(struct parsedRequest * parsed);
void freeParsedRequest(struct parsedRequest * request);
void replyRequest(int fd, struct response * result);
char * getHostFromURI(char * uri);
int guessPortFromURI(char * uri);


int main(int argc, char** argv)
{
    int port = atoi(argv[1]);
    printf("Runnion on %d port\n", port);
    int listenSocket = Socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    struct sockaddr_in serveraddr;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0) {
        printf("SetSockOpt failed\n");
        return -1;
    }
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if(bind(listenSocket, (SA *) &serveraddr, sizeof(serveraddr)) < 0) {
        printf("Bind failed\n");
        perror("Bind failed:");
        return -1;
    }
    if(listen(listenSocket, LISTENQ) < 0) {
        printf("Listen failed\n");
        return -1;
    }
    // now, listenSocket contains valid socket to call accept
    struct sockaddr_in clientAddr;
    struct hostent *hp;
    char * haddrp;
    unsigned short client_port;
    printf("Hello world\n");
    while(1) {
        const int clientlen = sizeof(clientAddr);
        int connfd = Accept(listenSocket, (SA *) &clientAddr, &clientlen);
        hp = gethostbyaddr((const char *)&clientAddr.sin_addr.s_addr,sizeof(clientAddr.sin_addr.s_addr), AF_INET);
        haddrp = inet_ntoa(clientAddr.sin_addr);
        client_port = ntohs(clientAddr.sin_port);
        if(hp)
            printf("server connected to %s (%s), port %u\n", hp->h_name, haddrp, client_port);
        else
            printf("server connected to (%s), port %u\n", haddrp, client_port);
        doProxy(connfd);
        printf("DoProxy done\n");
        Close(connfd);
    }
    return 0;
}


int doProxy(int fd) {
    struct parsedRequest parsed = parseRequest(fd);
    printf("Parse request done\n");
    struct response *result = execRequest(&parsed);
    printf("execRequest done\n");
    replyRequest(fd, result);
    printf("replyRequest done\n");
    freeParsedRequest(&parsed);
    free(result->data);
    free(result);
    printf("doProxy about to return\n");

    return 0;
}

struct parsedRequest parseRequest(int fd) {
    size_t n;
    char buf[MAXLINE];
    char originalBuf[MAXLINE];
    rio_t rio;
    Rio_readinitb(&rio, fd);
    struct parsedRequest result;
    result.params = NULL;
    struct httpParam * iterator = NULL;
    size_t content_length = 0;
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        //
        printf("line: %s", buf);
        strncpy(originalBuf, buf, MAXLINE);
        char * saveptr;
        char * first = strtok_r(buf, " ", &saveptr);
        if(!first)
            continue;
        if(strncmp(first, "\r\n", 2)==0) {
            printf("Last\n");
            break;
        }
        if(strcasecmp(first, "GET") == 0) {
            //GET
            char * uri = strtok_r(NULL, " ", &saveptr);
            char * httpVersion = strtok_r(NULL, " ", &saveptr);
            printf("uri: %s, version: %s\n", uri, httpVersion);
            result.uri = strdup(uri);
            result.httpVersion = strdup(httpVersion);
            continue;
        } else {
            char *field = strtok_r(originalBuf, ":", &saveptr);
            char *value = strtok_r(NULL, ":", &saveptr);
            printf("Field: %s, value: %s\n", field, value);
            struct httpParam *param = malloc(sizeof(struct httpParam));
            param->next = NULL;
            param->field = strdup(trimwhitespace(field));
            param->value = strdup(trimwhitespace(value));
            if(strcmp(param->field, connection)==0) {
                free(param->value);
                param->value = strdup(connection_close_hdr);
                result.has_connection = 1;
            } else if(strcmp(param->field, proxy_connection) == 0) {
                free(param->value);
                param->value = strdup(proxy_connection_close_hdr);
                result.has_proxy_connection = 1;
            } else if(strcmp(param->field, user_agent)==0) {
                free(param->value);
                param->value = strdup(user_agent_hdr);
                result.has_user_agent = 1;
            } else if(strcmp(param->field, host) == 0) {
                result.has_host = 1;
                result.host = strdup(param->value);
            }
            if(iterator == NULL) {
                iterator = param;
                result.params = iterator;
            } else {
                iterator -> next = param;
                iterator = param;
            }
            continue;
        }

    }
    return result;
}

struct response * execRequest(struct parsedRequest * parsed) {
    // do something
    rio_t rio;
    char * hostname = parsed->has_host ? parsed->host : getHostFromURI(parsed->uri);
    printf("Hostname: %s has_host: %d host: %s\n", hostname, parsed->has_host, parsed->host);
    char buf[MAXLINE];
    int port = guessPortFromURI(parsed->uri);
    printf("Port: %d\n", port);
    int clientfd;
    struct hostent *  hp;
    struct sockaddr_in serveraddr;
    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return NULL;
    if ((hp = gethostbyname(hostname)) == NULL)
        return NULL;
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0], (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);
    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
        return NULL;
    printf("Replying...\n");
    sprintf(buf, "GET %s HTTP/1.0\r\n", parsed->uri);
    Rio_writen(clientfd, buf, strlen(buf));
    printf("Wrote %s", buf);
    if(!parsed->has_host) {
        sprintf(buf, "Host: %s\r\n", hostname);
        Rio_writen(clientfd, buf, strlen(buf));
        printf("Wrote %s", buf);
    }
    if(!parsed->has_user_agent) {
        sprintf(buf, "%s: %s\r\n", user_agent, user_agent_hdr);
        Rio_writen(clientfd, buf, strlen(buf));
        printf("Wrote %s", buf);
    }
    if(!parsed->has_connection) {
        sprintf(buf, "%s: %s\r\n", connection, connection_close_hdr);
        Rio_writen(clientfd, buf, strlen(buf));
        printf("Wrote %s", buf);
    }
    if(!parsed->has_proxy_connection) {
        sprintf(buf, "%s: %s\r\n", proxy_connection, proxy_connection_close_hdr);
        Rio_writen(clientfd, buf, strlen(buf));
        printf("Wrote %s manually", buf);
    }
    struct httpParam * iterator = parsed->params;
    while(iterator) {
        sprintf(buf, "%s: %s\r\n", iterator->field, iterator->value);
        Rio_writen(clientfd, buf, strlen(buf));
        printf("Wrote %s", buf);
        iterator = iterator -> next;
    }
    Rio_writen(clientfd, "\r\n", strlen("\r\n"));
    printf("Wrote %s", buf);
    ssize_t n = 0;
    int totalN = 0;
    struct response * responseData = malloc(sizeof(struct response));
    responseData->data = NULL;
    printf("Reading\n");

    Rio_readinitb(&rio, clientfd);
    while((n=Rio_readnb(&rio, buf, MAXLINE)>0)) {
        printf("n:%d, buf:%s\n", n, buf);
        totalN += n;
        responseData->data = realloc(responseData->data, totalN);
        memcpy(responseData->data + totalN - n, buf, n);
    }
    responseData -> length = totalN;
    printf("total N: %d\n", totalN);
    Close(clientfd);
    return responseData;
}


void freeParsedRequest(struct parsedRequest * request) {
    free(request->uri);
    free(request->httpVersion);
    struct httpParam * iterator = request->params;
    while(iterator) {
        free(iterator->field);
        free(iterator->value);
        struct httpParam * current = iterator;
        iterator = iterator->next;
        free(current);
    }
    request->params = NULL;
    if(request->host)
        free(request->host);
}

void replyRequest(int fd, struct response * result) { 
    Rio_writen(fd, result->data, result->length);
}

char * getHostFromURI(char * uri) {
    
    return "";
}

int guessPortFromURI(char * uri) {
    printf("GuessPortFromURI uri:%s\n", uri);
    int state = 0;
    char *  portStart = NULL;
    int port = 0;
    while(*uri) {
        char ch = *uri;
        if(*uri == ':') {
            state = 1;
        }
        else if(isdigit(ch)) {
            if(state == 1) {
                portStart = uri;
                state = 2;
            } else if(state == 2){

            } else {
            }
        }
        else if(ch == '/') {
            if(state == 2) {
                // portEnd =  uri-1;
                state = 3;
            }else {
                state = 0;
            }
        } else {
            state = 0;
        }
        if(state == 3)
            break;
        uri++;
    }
    if(portStart)
        port = atoi(portStart);
    if(port == 0)
        port = 80;
    return port;
}

// https://stackoverflow.com/a/122721/8614565
// Note: This function returns a pointer to a substring of the original string.
// // If the given string was allocated dynamically, the caller must not overwrite
// // that pointer with the returned value, since the original pointer must be
// // deallocated using the same allocator with which it was allocated.  The return
// // value must NOT be deallocated using free() etc.
char *trimwhitespace(char *str)
{
    char *end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0) // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

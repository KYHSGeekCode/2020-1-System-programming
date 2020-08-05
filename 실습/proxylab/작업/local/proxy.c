#include <stdio.h>
#include <string.h>
#include "csapp.h"
#include "lrucache.h"

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";
static const char *connection_close_hdr = "close";
static const char *proxy_connection_close_hdr = "close";

static const char *connection = "Connection";
static const char *proxy_connection = "Proxy-Connection";
static const char *user_agent = "User-Agent";
static const char *host = "Host";
static const char *contentLengthField = "Content-Length";

static struct LRUCache cache;

struct httpParam {
    char * field;
    char * value;
    struct httpParam * next;
};

struct parsedRequest {
    char * uri;
    char * httpVersion;
    char * method;
    char * host;
    char has_user_agent;
    char has_connection;
    char has_proxy_connection;
    char has_host;
    ssize_t content_length;
    char * content;
    struct httpParam * params;
};

struct response {
    char * uri;
    char * data;
    int length;
};

extern sem_t semaphore;

int doProxy(int fd);
char *trimwhitespace(char *str);
struct parsedRequest parseRequest(int fd);
struct response execRequest(struct parsedRequest * parsed);
void freeParsedRequest(struct parsedRequest * request);
void replyRequest(int fd, struct response * result);
char * getHostFromURI(char * uri);
int guessPortFromURI(char * uri);
void * proxy_thread(void *vargp);
struct response execRequest_Cached(struct parsedRequest * parsed);
void initSignal();
char * makeRelativeURI(char * uri);

int main(int argc, char** argv)
{
    char hostname[MAXLINE];
    char clientPort[MAXLINE];
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int portNumber = atoi(argv[1]);
    printf("Runnion on %d port\n", portNumber);
    int listenSocket; // = Socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in clientAddr;
    listenSocket = Open_listenfd(argv[1]);
    initCache(&cache);
    initSignal();
    pthread_t tid;
    while(1) {
        const int clientlen = sizeof(clientAddr);
        int connfd = Accept(listenSocket, (SA *) &clientAddr, &clientlen);
        Getnameinfo((SA *) &clientAddr, clientlen, hostname, MAXLINE, clientPort, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, clientPort);
        // hp = gethostbyaddr((const char *)&clientAddr.sin_addr.s_addr,sizeof(clientAddr.sin_addr.s_addr), AF_INET);
        int *connfdp = malloc(sizeof(int));
        *connfdp = connfd;
        Pthread_create(&tid, NULL, proxy_thread, connfdp);
        // Close(connfd);
    }
    freeCache(&cache);
    return 0;
}

void * proxy_thread(void *vargp) {
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    free(vargp);
    doProxy(connfd);
    printf("DoProxy done\n");
    Close(connfd);
    return NULL;
}

struct sigaction old_action;

void ctrlC_handler(int sig_no) {
    freeCache(&cache);
    exit(0);
}

void initSignal() {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &ctrlC_handler;
    sigaction(SIGINT, &action, &old_action);
}


struct LRUNode * createData(const char * key, void * metadata) {
    printf("CreateData called\n");
    struct parsedRequest * request = (struct parsedRequest * ) metadata;
    struct response response =  execRequest(request);
    struct LRUNode * node = malloc(sizeof(struct LRUNode));
    if(response.length == -1) {
        printf("Warning: response is NULL");
        node -> data = NULL;
        node -> size = 0;
    } else {
        node -> data = response.data;
        node -> size = response.length;
    }
    node -> next = NULL;
    node -> prev = NULL;
    return node;
}

int doProxy(int fd) {
    struct parsedRequest parsed = parseRequest(fd);
    printf("Parse request done\n");
    if(!parsed.method || (strncmp(parsed.method, "GET", 3)!=0 && strncmp(parsed.method, "POST", 4)!=0)) {
        printf("[PROXY] ============ Ignoring method %s ============\n", parsed.method);
        freeParsedRequest(&parsed);
        return -1;
    }
    struct response result = execRequest_Cached(&parsed);
    printf("execRequest done\n");
    replyRequest(fd, &result);
    printf("replyRequest done\n");
    freeParsedRequest(&parsed);
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
    result.content_length = 0;
    result.uri = NULL;
    result.method =  NULL;
    result.httpVersion = NULL;
    result.host = NULL;
    result.content = NULL;
    result.has_host = 0;
    result.has_proxy_connection = 0;
    result.has_connection = 0;
    result.has_user_agent = 0;
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
            printf("Last buf: %s\n", originalBuf);
            break;
        }
        if((strcasecmp(first, "GET") == 0) || (strcasecmp(first, "POST") == 0)) {
            // GET or POST
            char * uri = strtok_r(NULL, " ", &saveptr);
            char * httpVersion = strtok_r(NULL, " ", &saveptr);
            char * method = first;
            printf("uri: %s, version: %s\n", uri, httpVersion);
            result.uri = strdup(uri);
            result.httpVersion = strdup(httpVersion);
            result.method = strdup(method);
            continue;
        } else {
            char *field = strtok_r(originalBuf, ":", &saveptr);
            char *value = strtok_r(NULL, ":", &saveptr);
            // printf("Field: %s, value: %s\n", field, value);
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
            } else if(strcmp(param->field, contentLengthField) == 0) {
                result.content_length = atoi(param->value);
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
    if(result.content_length > 0) {
        printf("Content length: %ld\n", result.content_length);
        int n = 0;
        int totalN = 0;
        result.content = NULL;
        while((n=Rio_readnb(&rio, buf, MAXLINE > result.content_length ? result.content_length: MAXLINE))>0) {
            // printf("something");
            // printf("n:%d\n", n);
            totalN += n;
            result.content = realloc(result.content, totalN);
            memcpy(result.content + totalN - n, buf, n);
            if(totalN == result.content_length)
                break;
        }
        // result.content_length = totalN;
        printf("Wrote content of size %d\n", totalN);

    }
    if(!result.has_host) {
        result.host = getHostFromURI(result.uri);
    }
    return result;
}

struct response execRequest_Cached(struct parsedRequest * parsed) {
    printf("execRequest_Cached called");
    struct LRUNode * node = LRUCache_get(&cache, parsed->uri, (void * ) parsed);
    struct response response; // = malloc(sizeof(struct response));
    response.data = node -> data;
    response.length = node -> size;
    response.uri = parsed -> uri;
    return response;
}

struct response execRequest(struct parsedRequest * parsed) {
    struct response responseData; // = malloc(sizeof(struct response));
    responseData.data = NULL;
    responseData.length = -1;
    rio_t rio;
    char * hostname = parsed->host;
    printf("Hostname: %s has_host: %d host: %s\n", hostname, parsed->has_host, parsed->host);
    char buf[MAXLINE];
    int port = guessPortFromURI(parsed->uri);
    printf("Port: %d\n", port);
    int clientfd;
    struct hostent *  hp;
    struct sockaddr_in serveraddr;
    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return responseData;
    if ((hp = gethostbyname(hostname)) == NULL)
        return responseData;
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0], (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);
    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
        return responseData;
    printf("Requesting...\n");
    char * relativeURI = makeRelativeURI(parsed -> uri);
    sprintf(buf, "%s %s HTTP/1.0\r\n", parsed -> method,  relativeURI);
    free(relativeURI);
    Rio_writen(clientfd, buf, strlen(buf));
    // printf("Wrote %s", buf);
    if(!parsed->has_host) {
        sprintf(buf, "Host: %s\r\n", hostname);
        Rio_writen(clientfd, buf, strlen(buf));
        // printf("Wrote %s", buf);
    }
    if(!parsed->has_user_agent) {
        sprintf(buf, "%s: %s\r\n", user_agent, user_agent_hdr);
        Rio_writen(clientfd, buf, strlen(buf));
        // printf("Wrote %s", buf);
    }
    if(!parsed->has_connection) {
        sprintf(buf, "%s: %s\r\n", connection, connection_close_hdr);
        Rio_writen(clientfd, buf, strlen(buf));
        // printf("Wrote %s", buf);
    }
    if(!parsed->has_proxy_connection) {
        sprintf(buf, "%s: %s\r\n", proxy_connection, proxy_connection_close_hdr);
        Rio_writen(clientfd, buf, strlen(buf));
        // printf("Wrote %s manually", buf);
    }
    struct httpParam * iterator = parsed->params;
    while(iterator) {
        sprintf(buf, "%s: %s\r\n", iterator->field, iterator->value);
        Rio_writen(clientfd, buf, strlen(buf));
        // printf("Wrote %s", buf);
        iterator = iterator -> next;
    }
    Rio_writen(clientfd, "\r\n", strlen("\r\n"));
    if(parsed->content_length > 0) {
        Rio_writen(clientfd, parsed->content, parsed->content_length);
    }
    // printf("Wrote %s", buf);
    ssize_t n = 0;
    int totalN = 0;
    printf("Reading\n");

    Rio_readinitb(&rio, clientfd);
    while((n=Rio_readnb(&rio, buf, MAXLINE))>0) {
        // printf("something");
        printf("n:%ld\n", n);
        totalN += n;
        responseData.data = realloc(responseData.data, totalN);
        memcpy(responseData.data + totalN - n, buf, n);
    }
    responseData.length = totalN;
    printf("total N: %d\n", totalN);
    Close(clientfd);
    return responseData;
}


void freeParsedRequest(struct parsedRequest * request) {
    free(request->uri);
    request -> uri = NULL;
    free(request->httpVersion);
    request -> httpVersion = NULL;
    free(request->method);
    request -> method = NULL;
    struct httpParam * iterator = request->params;
    while(iterator) {
        free(iterator->field);
        free(iterator->value);
        struct httpParam * current = iterator;
        iterator = iterator->next;
        free(current);
    }
    request->params = NULL;
    free(request->host);
    request -> host = NULL;
    free(request -> content);
    request -> content = NULL;
}

void replyRequest(int fd, struct response * result) { 
    if(result)
        Rio_writen(fd, result->data, result->length);
    else
        printf("Warning: result is null\n");
}

char * getHostFromURI(char * uri) {
    // ://부터 :이나 / 전까지 뽑아낸다
    int state = 0;
    char * start;
    int len = 0;
    while(*uri) {
        switch(state) {
            case 0:
                if(*uri == ':') {
                    state++;
                } else {
                    state = 0;
                }
                break;
            case 1:
                if(*uri == '/') {
                    state++;
                } else {
                    state = 0;
                }
                break;
            case 2:
                if(*uri == '/') {
                    state++;
                    start = uri + 1;
                } else {
                    state = 0;
                }
                break;
            case 3:
                if(*uri == '/') {
                    state++;
                } else {
                    len++;
                    state = 3;
                }
                break;
            case 4:
                break;
        }
        uri++;
    }
    char * result = malloc(sizeof(char) * (len + 1));
    result[len] = '\0';
    memcpy(result, start, len);
    return result;
}

char *makeRelativeURI(char * uri) {
    if(*uri == '/') {
        return strdup(uri);
    }
    if(strstr(uri, "://") == NULL) {
        int len = strlen(uri);
        char * result = malloc(sizeof(char) * (len + 2));
        result[0] = '/';
        strcpy(result+1, uri);
        result[len+1] = '\0';
        return strdup(uri);
    }
    int state = 0;
    char * pathStart = NULL;
    while(*uri) {
        char ch = *uri;
        switch(state) {
            case 0:
                if(ch == ':') {
                    state++;
                }
                break;
            case 1:
                if(ch == '/') {
                    state++;
                } else {
                    state = 0;
                }
                break;
            case 2:
                if(ch == '/') {
                    state++;
                } else {
                    state = 0;
                }
                break;
            case 3:
                if(ch == '/') {
                    state++;
                    pathStart = uri;
                }
                break;
        }
        uri++;
    }
    if(state == 4) {
        return strdup(pathStart);
    }
    return strdup("/");
}

int guessPortFromURI(char * uri) {
    if(uri == NULL)
        return 80;
    printf("GuessPortFromURI uri:%s\n", uri);
    int state = 0;
    char *  portStart = NULL;
    int port = 0;
    while(*uri) {
        char ch = *uri;
        if(*uri == ':') {
            state = 1;
            portStart = NULL;
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
                portStart = NULL;
                state = 0;
            }
        } else {
            portStart = NULL;
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

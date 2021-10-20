#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

typedef struct _request {
	char *method;
	char *uri;
	char *version;
	char *headers;
	char *body;
	char buf[4096];
} request;

enum codes { C400, C403, C404, C501 };

void sigchldhandler(int sig);
int server_init(short port);
int server_request(int listenfd, request* req);
void server_response(int connfd, request* req);
void error_page(int connfd, int code);
long content_length(const char* filename);
char* content_type(const char* filename);

int main(int argc, char* argv[])
{
	short port=8000, quite=0;

	int listenfd, connfd;

	request req;

	/* parse arguments */
	for (++argv; --argc; ++argv) {
		if (!strcmp(*argv, "--help")) {
			printf("Usage: server [-OPTIONS] [PORT]\n"
			       "If no port specified, then equals 8000\n"
			       "-q, --quite Server doesn't output incoming requests\n"
			       "    --help  Output this message and exit\n");
			exit(EXIT_SUCCESS);
		} else if (!strcmp(*argv, "-q") || !strcmp(*argv, "--quite")) {
			quite = 1;
		} else {
			port = atoi(*argv);
		}
	}

	/* initialize server */
	listenfd = server_init(port);

	while (1) {
		/* receive request from client */
		if ((connfd = server_request(listenfd, &req)) == -1)
			{ continue; }

		/* print request line and headers */
		if (!quite) {
			printf("%s %s %s\n%s\n", req.method, req.uri, req.version, req.headers);
			printf("-----------------------------------------------------------\n");
		}

		/* send response to client */
		server_response(connfd, &req);
	}

	return 0;
}

int server_init(short port)
{
	int listenfd;
	struct sockaddr_in addr;

	/* set handlers for signals */
	signal(SIGCHLD, sigchldhandler);
	signal(SIGPIPE, SIG_IGN);

	/* create socket */
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		{ perror("socket"); exit(EXIT_FAILURE); }

	/* specify address for socket */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	/* bind socket to specified address */
	if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		{ perror("bind"); exit(EXIT_FAILURE); }

	/* listen to connections */
	if (listen(listenfd, 5) == -1)
		{ perror("listen"); exit(EXIT_FAILURE); }

	/* return server file descriptor */
	return listenfd;
}

int server_request(int listenfd, request* req)
{
	int connfd;

	/* accept connection */
	if ((connfd = accept(listenfd, NULL, NULL)) == -1)
		{ perror("accept"); return -1; }

	/* clear buffer and receive data from client */
	memset(req->buf, 0, sizeof(req->buf));
	if (read(connfd, req->buf, sizeof(req->buf)-1) == -1)
		{ perror("read"); close(connfd); return -1; }

	/* parse request */
	req->body = strstr(req->buf, "\r\n\r\n"); 
	req->method = strtok(req->buf, " ");
	req->uri = strtok(NULL, " ");
	req->version = strtok(NULL, "\r");
	req->headers = strtok(NULL, "");

	/* check for invalid syntax */
	if (!req->method || !req->uri || !req->version || !req->headers || !req->body)
		{ error_page(connfd, C400); close(connfd); return -1; }

	/* final pointers preparations */
	req->headers += 1;
	*(req->body+2) = '\0';
	req->body += 4;
	req->buf[4095] = '\0';

	/* return client file descriptor */
	return connfd;
}

void server_response(int connfd, request* req)
{
	char content_name[1024];
	char *args;
	char content_args[1024];
	FILE *content_file;
	int send_body;

	char res_buf[1024];
	int res_size;

	/* parse uri */
	if (!strcmp(req->uri, "/")) { /* root */
		strcpy(content_name, "./index.html");
		strcpy(content_args, "");
	} else { /* standard */
		strcpy(content_name, ".");
		strcat(content_name, strtok(req->uri, "?\0"));
		strcpy(content_args, "QUERY_STRING=");
		if (args = strtok(NULL, "\0"))
			strcat(content_args, args);
	}

	/* check if file exists*/
	if (access(content_name, F_OK) == -1)
		{ error_page(connfd, C404); return; }

	/* send data to client */
	if ((send_body = !strcmp(req->method, "GET")) || !strcmp(req->method, "HEAD")) {

		/* serving dynamic content */
		if (!strncmp(content_name, "./cgi-bin/", 10)) {
			/* check for execution permisiion */
			if (access(content_name, X_OK) == -1)
				{ error_page(connfd, C403); return; }

			/* execute script */
			if (!fork()) {
				putenv(content_args);
				sprintf(res_buf, "REQUEST_METHOD=%s", req->method);
				putenv(res_buf);
				dup2(connfd, 1);
				write(connfd, "HTTP/1.0 200 OK\r\n", 17);
				execl(content_name, content_name, NULL);
			}

		/* serving static content */
		} else {
			/* check for execution permisiion */
			if (access(content_name, R_OK) == -1)
				{ error_page(connfd, C403); return; }

			/* generate response line and headers */
			sprintf(res_buf, "HTTP/1.0 200 OK\r\n"
					 "Content-length: %ld\r\n"
					 "Content-type: %s\r\n\r\n",
					 content_length(content_name), content_type(content_name));

			/* send response line and headers */
			write(connfd, res_buf, strlen(res_buf));

			/* send response body */
			if (send_body) {
				content_file = fopen(content_name, "r");
				while ((res_size = fread(res_buf, 1, sizeof(res_buf), content_file)) != 0)
					write(connfd, res_buf, res_size);
				fclose(content_file);
			}
		}

		/* close connection */
		close(connfd);

	/* receive data from client */
	} else if (!strcmp(req->method, "POST")) {
		long length;
		char *temp;

		/* check if content-length header is present */
		temp = strstr(req->headers, "Content-Length: ");
		if (!temp || (length = atol(temp+16)) < 1)
			{ error_page(connfd, C400); return; }

		/* check if content is dynamic and can be executed*/
		if (strncmp(content_name, "./cgi-bin/", 10) || access(content_name, X_OK) == -1)
			{ error_page(connfd, C403); return; }

		int fdpipe[2];
		pipe(fdpipe);

		/* execute script */
		if (!fork()) {
			dup2(fdpipe[0], 0);
			close(fdpipe[0]);
			sprintf(res_buf, "REQUEST_METHOD=POST");
			putenv(res_buf);
			sprintf(res_buf+20, "CONTENT_LENGTH=%ld", length);
			putenv(res_buf+20);
			dup2(connfd, 1);
			write(connfd, "HTTP/1.0 200 OK\r\n", 17);
			execl(content_name, content_name, NULL);
		}

		write(fdpipe[1], req->body, strlen(req->body));

		/* close connection */
		close(connfd);
		close(fdpipe[1]);

	/* method not implemented */
	} else {
		error_page(connfd, C501);
	}
}

void error_page(int connfd, int code)
{
	static const char *errhead[] = { "HTTP/1.0 400 Bad Request\r\n\r\n",
					 "HTTP/1.0 403 Forbidden\r\n",
					 "HTTP/1.0 404 Not Found\r\n\r\n",
					 "HTTP/1.0 501 Not Implemented\r\n\r\n" };
	char errbody[1024];

	/* generate error page */
	sprintf(errbody, 
		"<html><head><title>%s</title></head>"
		"<body><center><h1>%s</h1></center><hr><center>web_server 1.0</center></body></html>\r\n\r\n",
		errhead[code]+9, errhead[code]+9);

	/* send to client */
	write(connfd, errhead[code], strlen(errhead[code]));
	write(connfd, errbody, strlen(errbody));

	/* close connection */
	close(connfd);
}

long content_length(const char* filename)
{
	long length;
	FILE *file;

	file = fopen(filename, "r");
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fclose(file);

	return length;
}

char* content_type(const char* filename)
{
	if (strstr(filename, ".html"))
		return "text/html";
	else if (strstr(filename, ".gif"))
		return "image/gif";
	else if (strstr(filename, ".png"))
		return "image/png";
	else if (strstr(filename, ".jpeg") || strstr(filename, ".jpg"))
		return "image/jpeg";
	else if (strstr(filename, ".mp4"))
		return "video/mp4";
	else
		return "text/plain";
}

void sigchldhandler(int sig)
{
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <netinet/in.h>

char *get_type(const char *filename);

int main(int argc, char *argv[])
{
	short port;

	int listenfd, connfd;

	struct sockaddr_in addr;

	char req_header[1024];
	char *req_method, *req_uri, *req_version;

	char content_name[1024];
	char *args;
	char content_args[1024];
	FILE *content_file;
	struct stat content_stat;

	char res_200[] = "HTTP/1.0 200 OK\r\n";
	char res_404[] = "HTTP/1.0 404 Not Found\r\n\r\n";
	char res_501[] = "HTTP/1.0 501 Not Implemented\r\n\r\n";

	char res_buf[1024];
	int res_size;

	if (argc == 1) {
		port = 8000;
	} else if (argc == 2) {
		port = atoi(argv[1]);
	} else {
		printf("usage: %s [port]\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	/* create socket */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	/* specify address for socket */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	/* bind socket to specified address */
	bind(listenfd, (struct sockaddr *)&addr, sizeof(addr));

	/* listen to connections */
	listen(listenfd, 5);

	while (1) {
		/* accept connection */
		connfd = accept(listenfd, NULL, NULL);

		/* receive request from client */
		read(connfd, req_header, sizeof(req_header));

		/* parse request header */
		req_method = strtok(req_header, " ");
		req_uri = strtok(NULL, " ");
		req_version = strtok(NULL, "\n");

		if (!strcmp(req_method, "GET")) { /* send data to client */
			/* parse uri */
			if (!strcmp(req_uri, "/")) { /* root */
				strcpy(content_name, "./index.html");
				strcpy(content_args, "");
			} else { /* standard */
				strcpy(content_name, ".");
				strcat(content_name, strtok(req_uri, "?\0"));
				strcpy(content_args, "QUERY_STRING=");
				if (args = strtok(NULL, "\0"))
					strcat(content_args, args);
			}

			if (!stat(content_name, &content_stat)) {
				/* correct response */
				write(connfd, res_200, sizeof(res_200)-1);
				if (!strncmp(content_name, "./cgi-bin/", 10)) { /* serving dynamic content */
					if (!fork()) {
						putenv(content_args);
						dup2(connfd, 1);
						execl(content_name, content_name, NULL);
						exit(EXIT_SUCCESS);
					} else {
						wait(NULL);
					}
				} else { /* serving static content */
					sprintf(res_buf, "Connection: close\r\n"
							 "Content-length: %ld\r\n"
							 "Content-type: %s\r\n\r\n",
							 content_stat.st_size, get_type(content_name));
					write(connfd, res_buf, strlen(res_buf));
					content_file = fopen(content_name, "r");
					while ((res_size = fread(res_buf, 1, sizeof(res_buf), content_file)) != 0)
						write(connfd, res_buf, res_size);
					fclose(content_file);
				}
			} else {
				/* file doesn't exist */
				write(connfd, res_404, sizeof(res_404));
			}
		} else {
			/* method not implemented */
			write(connfd, res_501, sizeof(res_501));
		}

		/* close connection */
		close(connfd);
	}

	return 0;
}

char *get_type(const char *filename)
{
	if (strstr(filename, ".html"))
		return "text/html";
	else if (strstr(filename, ".gif"))
		return "image/gif";
	else if (strstr(filename, ".png"))
		return "image/png";
	else if (strstr(filename, ".jpeg"))
		return "image/jpeg";
	else
		return "text/plain";
}

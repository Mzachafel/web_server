#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

int main(int argc, char *argv[])
{
	short port;

	int listenfd, connfd;

	struct sockaddr_in addr;

	char req_header[1024];
	char *req_method, *req_uri, *req_version;

	char res_200[] = "HTTP/1.1 200 OK\r\n\r\n";
	char res_404[] = "HTTP/1.1 404 Not Found\r\n\r\n";
	char res_501[] = "HTTP/1.1 501 Not Implemented\r\n\r\n";

	char htmlname[1024];
	FILE *htmlfile;

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
			/* get html file name */
			if (!strcmp(req_uri, "/")) {
				sprintf(htmlname, "./index.html");
			} else {
				sprintf(htmlname, ".%s", req_uri);
			}

			/* read html file if exists, then send */
			if ((htmlfile = fopen(htmlname, "r")) != NULL) {
				write(connfd, res_200, sizeof(res_200)-1);
				while ((res_size = fread(res_buf, 1, sizeof(res_buf), htmlfile)) != 0)
					write(connfd, res_buf, res_size);
				fclose(htmlfile);
			} else {
				/* file doesn't exist */
				write(connfd, res_404, sizeof(res_404));
			}
		} else if (!strcmp(req_method, "HEAD")) {
			/* send header to client */
			write(connfd, res_200, sizeof(res_200));
		} else {
			/* method not implemented */
			write(connfd, res_501, sizeof(res_501));
		}

		/* close connection */
		close(connfd);
	}

	return 0;
}

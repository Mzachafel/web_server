#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#define errchck(x,y) if ((x) == -1) { \
		   perror(y); \
		   exit(EXIT_FAILURE); }

int main()
{
	int listenfd, connfd;

	struct sockaddr_in addr;

	char req_header[1024];
	char *req_method, *req_uri, *req_version;

	char res_200[] = "HTTP/1.1 200 OK\r\n\r\n";
	char res_404[] = "HTTP/1.1 404 Not Found\r\n\r\n";
	char res_501[] = "HTTP/1.1 501 Not Implemented\r\n\r\n";
	int headsize;

	char htmlname[1024];
	FILE *htmlfile, *tempfile;
	long htmlsize;

	char *res;

	/* create socket */
	errchck(listenfd = socket(AF_INET, SOCK_STREAM, 0), "socket");

	/* specify address for socket */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8000);
	addr.sin_addr.s_addr = INADDR_ANY;

	/* bind socket to specified address */
	errchck(bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)), "bind");

	/* listen to connections */
	errchck(listen(listenfd, 5), "listen");

	while (1) {
		/* accept connection */
		errchck(connfd = accept(listenfd, NULL, NULL), "accept");

		/* receive request from client */
		errchck(read(connfd, req_header, sizeof(req_header)), "read");

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
				/* get html file size */
				tempfile = fopen(htmlname, "rb");
				fseek(tempfile, 0, SEEK_END);
				htmlsize = ftell(tempfile);
				fclose(tempfile);

				/* get header size */
				headsize = sizeof(res_200);

				/* create response */
				res = malloc(headsize + htmlsize);
				strcat(res, res_200);
				fread(res+headsize-1, 1, htmlsize, htmlfile);

				/* close html file */
				fclose(htmlfile);

				errchck(write(connfd, res, headsize+htmlsize), "write");
			} else {
				errchck(write(connfd, res_404, sizeof(res_404)), "write");
			}
		} else if (!strcmp(req_method, "HEAD")) { /* send header to client */
			errchck(write(connfd, res_200, sizeof(res_200)), "write");
		} else { /* method not implemented */
			errchck(write(connfd, res_501, sizeof(res_501)), "write");
		}

		/* close connection */
		errchck(close(connfd), "close");
	}

	return 0;
}

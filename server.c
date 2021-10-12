#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#define errchck(x) if ((x) == -1) { \
		   perror(NULL); \
		   exit(EXIT_FAILURE); }

int main()
{
	char server_message[256] = "You have reached server!\n";

	/* create socket */
	int server_socket;
	errchck(server_socket = socket(AF_INET, SOCK_STREAM, 0));
	/* socket(domain, type, protocol) - returns file descriptor for socket
	 * domain - internet or filesystem socket - AF_INET - internet socket
	 * type - TCP or UDP socket - SOCK_STREAM - TCP
	 * protocol - specify protocol if socket is raw - 0 - use protocol specified in type */

	/* specify address for socket */
	struct sockaddr_in server_address;
	/* struct sockaddr_in
	 * sin_family- need to be same as domain in socket
	 * sin_port
	 * sin_addr */
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(9002);
	/* htons(hostshort) - translate unsigned short into network byte order */
	server_address.sin_addr.s_addr = INADDR_ANY;

	/* bind socket to specified address */
	errchck(bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)));

	/* listen to connections */
	errchck(listen(server_socket, 5));

	/* accept the connection to send data */
	int client_socket;
	errchck(client_socket = accept(server_socket, NULL, NULL));

	/* send data to client */
	errchck(send(client_socket, server_message, sizeof(server_message), 0));

	/* close sockets */
	errchck(close(client_socket));
	errchck(close(server_socket));

	return 0;
}

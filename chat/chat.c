#include <inc/lib.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>

#define PORT 1050
#define VERSION "0.1"
#define HTTP_VERSION "1.0"

#define E_BAD_REQ	1000

#define BUFFSIZE 512
#define MAXPENDING 5	// Max connection requests

static void
die(char *m)
{
	cprintf("%s\n", m);
	exit();
}

static void
handle_client(int sock)
{
	struct http_request con_d;
	int r;
	char buffer[BUFFSIZE];
	int received = -1;
	struct http_request *req = &con_d;

	while (1)
	{
		// Receive message
		if ((received = read(sock, buffer, BUFFSIZE)) < 0)
			panic("failed to read");

		memset(req, 0, sizeof(req));

		req->sock = sock;

		r = http_request_parse(req, buffer);
		if (r == -E_BAD_REQ)
			send_error(req, 400);
		else if (r < 0)
			panic("parse failed");
		else
			send_file(req);

		req_free(req);

		// no keep alive
		break;
	}

	close(sock);
}

void
umain(int argc, char **argv)
{
	// int serversock, clientsock;
	// struct sockaddr_in server, client;

	// binaryname = "chatserver";

	// // Create the TCP socket
	// if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	// 	die("Failed to create socket");

	// // Construct the server sockaddr_in structure
	// memset(&server, 0, sizeof(server));		// Clear struct
	// server.sin_family = AF_INET;			// Internet/IP
	// server.sin_addr.s_addr = htonl(INADDR_ANY);	// IP address
	// server.sin_port = htons(PORT);			// server port

	// // Bind the server socket
	// if (bind(serversock, (struct sockaddr *) &server,
	// 	 sizeof(server)) < 0)
	// {
	// 	die("Failed to bind the server socket");
	// }

	// // Listen on the server socket
	// if (listen(serversock, MAXPENDING) < 0)
	// 	die("Failed to listen on server socket");

	// cprintf("Waiting for chat server connections...\n");

	// while (1) {
	// 	unsigned int clientlen = sizeof(client);
	// 	// Wait for client connection
	// 	if ((clientsock = accept(serversock,
	// 				 (struct sockaddr *) &client,
	// 				 &clientlen)) < 0)
	// 	{
	// 		die("Failed to accept client connection");
	// 	}
	// 	handle_client(clientsock);
	// }

	// close(serversock);
    int r = fork();
    if(r == 0) {
        while(1) {
            int i, counter = 0;
            struct Fd *tmp;
            for(i = 0; i < MAXFD; i++) {
                if(fd_lookup(i, &tmp) == 0) {
                    counter++;
                }
            }
            cprintf("number of open FDs is %d\n", counter);
        }
    }
    else {
        int fd = open("tmp_file.txt", O_CREAT | O_RDWR);
        cprintf("it got fd id %d\n", fd);
        sys_env_copy_fd(r, fd);
        while(1) { }
    }

}

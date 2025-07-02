#include <inc/lib.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <inc/chat.h>

#define PORT 30000
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

void broadcast_message(int sender_fd, char* buffer, int length) {
    struct Fd *tmp_store;
    struct chat_packet packet;
    memset(packet.data, 0, sizeof(struct chat_packet));
    packet.sender_id = sender_fd;
    memcpy(packet.data, buffer, length);
    int i;
    for(i = 1; i < 32; i++) {
        if(fd_lookup(i, &tmp_store) == 0 && sender_fd != i) {
            int r = write(i, (char*)&packet, length + sizeof(envid_t));
            if(r == -1) {
                close(i);
                cprintf("closed fd id %d\n", i);
            }
        }
    }
}

void
umain(int argc, char **argv)
{
	int serversock, clientsock;
	struct sockaddr_in server, client;

	binaryname = "chatserver";

	// Create the TCP socket
	if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		die("Failed to create socket");

	// Construct the server sockaddr_in structure
	memset(&server, 0, sizeof(server));		// Clear struct
	server.sin_family = AF_INET;			// Internet/IP
	server.sin_addr.s_addr = htonl(INADDR_ANY);	// IP address
	server.sin_port = htons(PORT);			// server port

	// Bind the server socket
	if (bind(serversock, (struct sockaddr *) &server,
		 sizeof(server)) < 0)
	{
		die("Failed to bind the server socket");
	}

	// Listen on the server socket
	if (listen(serversock, MAXPENDING) < 0)
		die("Failed to listen on server socket");

	cprintf("Waiting for chat server connections...\n");

    int child = fork();
    if(child == 0) { //this is the child
        //poll through the sockets and check if someone sent something
        while(1) {
            int fd, received = 0;
            struct Fd *fd_store;
            char buffer[BUFFSIZE];
            memset(buffer, 0, BUFFSIZE);
            for(fd = 1; fd < 32; fd++) {
                if(fd_lookup(fd, &fd_store) == 0 && (received = read_nowait(fd, buffer, BUFFSIZE)) > 0) {
                    broadcast_message(fd, buffer, received);
                }
            }
        }
    }
    else {
        while (1) {
            unsigned int clientlen = sizeof(client);
            // Wait for client connection
            if ((clientsock = accept(serversock,
                        (struct sockaddr *) &client,
                        &clientlen)) < 0)
            {
                die("Failed to accept client connection");
            }
            sys_env_copy_fd(child, clientsock);
            cprintf("A new user(%d) has connected!\n", clientsock);
        }
        close(serversock);
    }
}

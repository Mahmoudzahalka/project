#include <inc/lib.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <inc/chat.h>

#define PORT 30000
// ##define IPADDR "10.0.2.15"
#define IPADDR "127.0.0.1"
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

void
umain(int argc, char **argv)
{
    binaryname = "chatclient";

	int sock;
	struct sockaddr_in echoserver;
	char buffer[BUFFSIZE];
	unsigned int echolen;
	int received = 0;

    opencons();
    dup(0, 1);


	cprintf("Connecting to:\n");
	cprintf("\tip address %s = %x\n", IPADDR, inet_addr(IPADDR));

	// Create the TCP socket
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		die("Failed to create socket");

	cprintf("opened socket\n");

	// Construct the server sockaddr_in structure
	memset(&echoserver, 0, sizeof(echoserver));       // Clear struct
	echoserver.sin_family = AF_INET;                  // Internet/IP
	echoserver.sin_addr.s_addr = inet_addr(IPADDR);   // IP address
	echoserver.sin_port = htons(PORT);		  // server port

	cprintf("trying to connect to server\n");

	// Establish connection
	if (connect(sock, (struct sockaddr *) &echoserver, sizeof(echoserver)) < 0)
		die("Failed to connect with server");
    
    cprintf("connected to chat server!!!\n");

    int child = fork();
    if(child == 0) { //this is the child
        //poll through the sockets and check if someone sent something
        while(1) {
            char *buf;
            buf = readline(NULL);
            if(!buf) {
                exit();
            }
            write(sock, buf, strlen(buf));
        }
    }
    else {
        while (1) {
            char buffer[1024];
            memset(buffer, 0, 1024);
            int read_bytes = read(sock, buffer, 1024);
            if(read_bytes <= 0) {
                cprintf("Exiting!\n");
                exit();
            }
            cprintf("\nEnvironment ID %d: %s\n", ((struct chat_packet*)buffer)->sender_id, ((struct chat_packet*)buffer)->data);
        }
        close(sock);
    }
}

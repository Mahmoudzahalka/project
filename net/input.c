#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;

void sleep(uint32_t ms) {
	uint32_t time = sys_time_msec();
	uint32_t end = time + ms;
	while(sys_time_msec() < end) {
		sys_yield();
	}
}

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";
	while(1) {
		int length = 0;
		char buffer[1518];
		while((length = sys_net_try_receive(buffer, 1518)) < 0) { }
		memmove(nsipcbuf.pkt.jp_data, buffer, 1518);
		nsipcbuf.pkt.jp_len = length;
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P|PTE_W|PTE_U);
		sleep(100);
	 }

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
}

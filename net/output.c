#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;
#define TX_BUFFER_SIZE 1518


void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// if(sys_add_descriptor_buffers() < 0)
	// 	panic("Failed to allocate network output buffers\n");

	while(true) {
		envid_t env_store;
		int tail_idx = 0;
		//lab6:zero copy search through all the pages for the descriptors
retry:
		if((tail_idx = sys_get_tail_idx()) == -E_NO_DESC)
			goto retry;
		int recv_value = ipc_recv(&env_store, &nsipcbuf, NULL);

		union Nsipc *buffer = &nsipcbuf;
		//union Nsipc *buffer = (union Nsipc*)((tail_idx) * PGSIZE);
		//int recv_value = ipc_recv(&env_store, buffer, NULL);
		// int i = 0;
		// cprintf("DEBUG!!!!!!!\n");
		// if(counter == 0)
		// 	for(i = 0; i < 9; i++) {
		// 		cprintf("%d", ((char*)buffer)[i]);
		// 	}
		// 	cprintf("\nDONE\n");

		if(env_store != ns_envid || recv_value != NSREQ_OUTPUT) {
			continue;
		}
		// for(; i < buffer->pkt.jp_len; i++) {
		// 	cprintf("%x", buffer->pkt.jp_data[i]);
		// }
		// cprintf("\nDONE\n");
		while(sys_net_try_send(((void*)buffer) + 4, *((uint32_t*)buffer)) != 0) { cprintf("stuck here\n"); }
	}
	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
}

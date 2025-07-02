#include <kern/e1000.h>
#include <kern/env.h>

extern uint16_t irq_mask_8259A;

volatile uint32_t *pci_addr;
//-------------------transmit pointers-----------------
volatile uint32_t *e1000_tdt;
volatile uint32_t *e1000_tdlen;
//-------------------receive pointers------------------
volatile uint32_t *e1000_rdt;

static uint32_t tx_desc_index = 0;

volatile uint32_t in_page_pointers[TX_DESC_AMOUNT] = {0};

struct e1000_tx_desc tx_desc[TX_DESC_AMOUNT] = { {0} };
char tx_buffers[TX_DESC_AMOUNT][TX_BUFFER_SIZE];//  __attribute__((aligned(PGSIZE)));
//struct e1000_tx_buffer tx_buffers[TX_DESC_AMOUNT+1]  __attribute__((aligned(PGSIZE)));

struct e1000_rx_desc rx_desc[RX_DESC_AMOUNT] = { {0} };
char rx_buffers[RX_DESC_AMOUNT][TX_BUFFER_SIZE];//  __attribute__((aligned(PGSIZE)));

// LAB 6: Your driver code here
void initialize_e1000() {
    //initializing the transmit descriptors.
    int i;
    for(i = 0; i < TX_DESC_AMOUNT; i++) {
        tx_desc[i].buffer_addr_low = PADDR((void*)&tx_buffers[i]);//PADDR(((void*)(&tx_buffers[i+1])) + 4);
        tx_desc[i].buffer_addr_high = 0;
        tx_desc[i].lower.flags.length = TX_BUFFER_SIZE;
        //tx_desc[i].lower.flags.cmd = 1 << 7 | 1 << 3;
    }
    pci_addr[E1000_TDBAL/4] = PADDR((void*)tx_desc);
    pci_addr[E1000_TDBAH/4] = 0;
    pci_addr[E1000_TDLEN/4] = (uint32_t)(sizeof(struct e1000_tx_desc) * TX_DESC_AMOUNT);
    //cprintf("the length is %d\n", (uint32_t)(sizeof(struct e1000_tx_desc) * TX_DESC_AMOUNT));
    pci_addr[E1000_TDH/4] = 0;
    pci_addr[E1000_TDT/4] =0x0;
    pci_addr[E1000_TCTL/4] = E1000_TCTL_EN | E1000_TCTL_PSP | (E1000_TCTL_COLD & (0x40 << 12)) | (E1000_TCTL_CT & (0x10 << 4));
    pci_addr[E1000_TIPG/4] = (10) | (4 << 10) | (6 << 20);

    //initializing the recieve descriptors.
    for(i = 0; i < RX_DESC_AMOUNT; i++) {
        rx_desc[i].buffer_addr_low = PADDR((void*)(rx_buffers[i]));
        rx_desc[i].buffer_addr_high = 0;
        rx_desc[i].length = RX_BUFFER_SIZE;
    }
    uint32_t mac_address[2];
    e1000_get_mac_address(mac_address);
    pci_addr[E1000_RAL/4] = mac_address[0];
    pci_addr[E1000_RAH/4] = mac_address[1] | E1000_RAH_AV;
    //cprintf("THE MAC ADDRESS IS %x %x\n", mac_address[0], mac_address[1]);

    pci_addr[E1000_RDBAL/4] = PADDR((void*)rx_desc);
    pci_addr[E1000_RDBAH/4] = 0;
    pci_addr[E1000_RDLEN/4] = (uint32_t)(sizeof(struct e1000_rx_desc) * RX_DESC_AMOUNT);
    pci_addr[E1000_RDH/4] = 0;
    pci_addr[E1000_RDT/4] = RX_DESC_AMOUNT-1;
    pci_addr[E1000_RCTL/4] = E1000_RCTL_EN | E1000_RCTL_SZ_2048 | E1000_RCTL_SECRC;
    pci_addr[E1000_RDTR1/4] = 0;
    pci_addr[E1000_ICS/4] = E1000_ICR_RXT0 | E1000_ICR_GPI_EN0 | E1000_ICR_TXDW;
    pci_addr[E1000_RADV/4] = 0;
    pci_addr[E1000_IMS/4] = E1000_IMS_RXT0 | E1000_IMS_TXDW;
}

int e1000_attach_function(struct pci_func *pcif) {
	pci_func_enable(pcif);
    irq_setmask_8259A(irq_mask_8259A & ~(1 << 11));
    pci_addr = (uint32_t*)mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
    e1000_tdt = (uint32_t*)&pci_addr[E1000_TDT/4];
    e1000_tdlen = (uint32_t*)&pci_addr[E1000_TDLEN/4];
    e1000_rdt = (uint32_t*)&pci_addr[E1000_RDT/4];
    initialize_e1000();
    return 0;
}

int e1000_transmit_packet(void* packet_buffer, uint32_t size) {
    if(size > MAX_PACKET_SIZE)
        return -E_INVAL;
    int next_desc_idx = *e1000_tdt;
    if(RS_VALUE(&tx_desc[next_desc_idx]) == 1 && TX_DD_VALUE(&tx_desc[next_desc_idx]) == 0) {
        curenv->env_status = ENV_WAITING_FOR_TRANSMIT;
        return -E_NO_DESC; //this descriptor is not ready to use.
    }
    
    tx_desc[next_desc_idx].lower.flags.length = size;
    memmove(tx_buffers[next_desc_idx], packet_buffer, size);// !!!!!!!!!!

    tx_desc[next_desc_idx].lower.flags.cmd |= (1 << 3) | 0x1 | 1 << 7;
    pci_addr[E1000_IMS/4] |= E1000_IMS_TXDW;
    *e1000_tdt = (*e1000_tdt + 1) % TX_DESC_AMOUNT;
    return 0;
}

int e1000_receive_packet(void* packet_buffer, uint32_t size) {
    static uint32_t current_rdesc_index = 0;

    if(size > MAX_PACKET_SIZE) 
        return -E_INVAL;
    if(RX_DD_VALUE(&rx_desc[current_rdesc_index]) == 0) {
        curenv->env_status = ENV_WAITING_FOR_RECV;
        return -E_NO_RECV;
    }
    uint32_t packet_length = rx_desc[current_rdesc_index].length;
    if(packet_length > size)
        return -E_BAD_SIZE;
    memmove(packet_buffer, rx_buffers[current_rdesc_index], packet_length);
    memset(packet_buffer + packet_length, 0, MAX_PACKET_SIZE - packet_length);//set the other bytes to 0
    *e1000_rdt = current_rdesc_index;
    current_rdesc_index = (current_rdesc_index + 1) % RX_DESC_AMOUNT;
    return packet_length;
}

void e1000_get_mac_address(uint32_t address[2]) {
    uint32_t counter = 0;
    address[0] = address[1] = 0;
    while(counter < 3) {
        pci_addr[E1000_EERD/4] = E1000_EERD_START_MASK | counter << 8;
        while(!(pci_addr[E1000_EERD/4] & E1000_EERD_DONE_MASK)) { }
        address[counter/2] |= ((pci_addr[E1000_EERD/4] & E1000_EERD_DATA_MASK) >> 16) << ((counter % 2) * 16);
        counter++;
    }
}

void e1000_handle_transmit_int() {
    int env_id = 0;
    for(env_id = 0; env_id < NENV; env_id++) {
        if(envs[env_id].env_status == ENV_WAITING_FOR_TRANSMIT) {
            envs[env_id].env_status = ENV_RUNNABLE;
            break;
        }
    }
}

void e1000_handle_receive_int() {
    int env_id = 0;
    for(env_id = 0; env_id < NENV; env_id++) {
        if(envs[env_id].env_status == ENV_WAITING_FOR_RECV) {
            envs[env_id].env_status = ENV_RUNNABLE;
            break;
        }
    }
}

int e1000_get_tail_idx() {
    int tail_idx = *e1000_tdt;
    if(RS_VALUE(&tx_desc[tail_idx]) == 1 && TX_DD_VALUE(&tx_desc[tail_idx]) == 0) {
        return -E_NO_DESC; //this descriptor is not ready to use.
    }
    return tail_idx;
}
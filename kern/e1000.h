#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#include <kern/pci.h>
#include <kern/pmap.h>
#include <inc/string.h>
#include <inc/error.h>
#include <kern/picirq.h>
#include <kern/syscall.h>
#include <kern/env.h>
#include <inc/ns.h>//REMOVE ME

#define DESCRIPTORSVA   (0x0ffff000 - TX_DESC_AMOUNT * PGSIZE)

//--------------------defines-------------------------------
#define TX_DESC_AMOUNT          64
#define RX_DESC_AMOUNT          128
#define MAX_PACKET_SIZE         1518
#define TX_BUFFER_SIZE          1518
#define RX_BUFFER_SIZE          2048

//-------------------------tx regs--------------------------
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
#define E1000_RA       0x05400  /* Receive Address - RW Array */
#define E1000_RAL      0x05400  /* Receive Address - RW Array */
#define E1000_RAH      0x05404  /* Receive Address - RW Array */
#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */
#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */
#define E1000_ICR      0x000C0  /* Interrupt Cause Read - R/clr */
#define E1000_ICS      0x000C8  /* Interrupt Cause Set - WO */

#define E1000_RDTR1    0x02820  /* RX Delay Timer (1) - RW */
#define E1000_RADV     0x0282C  /* RX Interrupt Absolute Delay Timer - RW */
#define E1000_IMS      0x000D0  /* Interrupt Mask Set - RW */
#define E1000_IMC      0x000D8  /* Interrupt Mask Clear - WO */





//-------------------------RCTL bits-------------------------
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */

#define E1000_ICR_RXT0          0x00000080 /* rx timer intr (ring 0) */
#define E1000_IMS_RXT0          E1000_ICR_RXT0      /* rx timer intr */
#define E1000_ICR_GPI_EN0       0x00000800 /* GP Int 0 */
#define E1000_ICS_GPI_EN0       E1000_ICR_GPI_EN0   /* GP Int 0 */
#define E1000_IMC_RXT0      E1000_ICR_RXT0      /* rx timer intr */
#define E1000_ICR_TXDW          0x00000001 /* Transmit desc written back */
#define E1000_ICS_TXDW      E1000_ICR_TXDW      /* Transmit desc written back */
#define E1000_IMS_TXDW      E1000_ICR_TXDW      /* Transmit desc written back */
#define E1000_IMC_TXDW      E1000_ICR_TXDW      /* Transmit desc written back */
#define E1000_TIDV     0x03820  /* TX Interrupt Delay Value - RW */
#define E1000_TXD_CMD_IDE    0x80000000 /* Enable Tidv register */
#define E1000_TXDCTL   0x03828  /* TX Descriptor Control - RW */



//-------------------------EEPROM addresses------------------
#define E1000_EERD     0x00014  /* EEPROM Read - RW */

#define E1000_EERD_ADDR_MASK    0xFFFC
#define E1000_EERD_START_MASK   0x1
#define E1000_EERD_DONE_MASK    0x10
#define E1000_EERD_DATA_MASK    0xFFFF0000



//-----------------------------------------------------------

#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */


/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

#define TX_DD_VALUE(desc_addr)         (((struct e1000_tx_desc*)(desc_addr))->upper.fields.status & 0x1)
#define RS_VALUE(desc_addr)         (((struct e1000_tx_desc*)(desc_addr))->lower.flags.cmd & (1 << 3))

#define RX_DD_VALUE(desc_addr)         (((struct e1000_rx_desc*)(desc_addr))->status & 0x1)


struct e1000_tx_desc {
    uint32_t buffer_addr_low;       /* Address of the descriptor's data buffer */
    uint32_t buffer_addr_high;
    union {
        uint32_t data;
        struct {
            uint16_t length;    /* Data buffer length */
            uint8_t cso;        /* Checksum offset */
            uint8_t cmd;        /* Descriptor control */
        } flags;
    } lower;
    union {
        uint32_t data;
        struct {
            uint8_t status;     /* Descriptor status */
            uint8_t css;        /* Checksum start */
            uint16_t special;
        } fields;
    } upper;
};

/* Receive Descriptor */
struct e1000_rx_desc {
    uint32_t buffer_addr_low; /* Address of the descriptor's data buffer */
    uint32_t buffer_addr_high;
    uint16_t length;     /* Length of data DMAed into data buffer */
    uint16_t csum;       /* Packet checksum */
    uint8_t status;      /* Descriptor status */
    uint8_t errors;      /* Descriptor Errors */
    uint16_t special;
};

struct e1000_tx_buffer {
    uint32_t size;
    char data[4096-4];
};



int e1000_attach_function(struct pci_func *pcif);
int e1000_transmit_packet(void* packet_buffer, uint32_t size);
int e1000_receive_packet(void* packet_buffer, uint32_t size);
void e1000_get_mac_address(uint32_t address[2]);
void e1000_handle_transmit_int();
void e1000_handle_receive_int();
int e1000_get_tail_idx();


#endif	// JOS_KERN_E1000_H

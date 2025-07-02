
#include "fs.h"

static int bc_page_count = 0;

// Return the virtual address of this disk block.
void*
diskaddr(uint32_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (uvpd[PDX(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
	return (uvpt[PGNUM(va)] & PTE_D) != 0;
}

int get_block_cache_page_num() {
	uint32_t page_addr, counter = 0;
	for(page_addr = DISKMAP; page_addr < DISKMAP + DISKSIZE; page_addr += BLKSIZE) {
		//cprintf("looping id %x and final address is %x\n", page_addr, DISKMAP + DISKSIZE);
		if((uvpd[PDX(page_addr)] & PTE_P) && (uvpt[page_addr/PGSIZE] & PTE_P))
			cprintf("block id %d is valid\n", (page_addr - DISKMAP)/BLKSIZE);
			counter++;
	}
	return counter;
}

int check_evict_page(uint32_t pte) {
	if((pte & PTE_P) && (!(pte & PTE_A)))
		return 1;
	return 0;
}

void evict_page(void* addr, uint32_t pte) {
	if(va_is_dirty(addr)) {
		flush_block(addr);
	}
	sys_page_unmap(0, addr);
	bc_page_count--;
}

void clear_pages_access_flag() {
	uint32_t page_addr;
	for(page_addr = DISKMAP; page_addr < DISKMAP + DISKSIZE; page_addr += BLKSIZE) {
		if(va_is_mapped((void*)page_addr))
			sys_fs_clear_access_bit(page_addr);
	}
}

void bc_evict_pages() {
	void *page_to_evict = NULL;
	uint32_t page_addr, counter = 0,in_between_counter = 0;
	bool initialized_flag = false;
	for(page_addr = DISKMAP; page_addr < DISKMAP + DISKSIZE; page_addr += BLKSIZE) {
		if(uvpd[PDX(page_addr)] & PTE_P) {
			uint32_t pte = uvpt[page_addr/PGSIZE];
			if(check_evict_page(pte)) {
				evict_page((void*)page_addr, pte);
				counter++;
			}
			else {
				if(pte & PTE_P) {
					if(!initialized_flag) {
						page_to_evict = (void*)page_addr;
						initialized_flag = true;
					}
					if(in_between_counter >= IN_BETWEEN_LIMIT) {
						page_to_evict = (void*)page_addr;
					}
					in_between_counter = 0;
					sys_fs_clear_access_bit(page_addr);
				}
				else	
					in_between_counter++;
			}
		}
	}
	if(counter == 0) {
		evict_page(page_to_evict, uvpt[(uint32_t)page_to_evict/PGSIZE]);
	}
}

// Fault any disk block that is read in to memory by
// loading it from disk.
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;

	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_eip, addr, utf->utf_err);

	// Sanity check the block number.
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page.
	// Hint: first round addr to page boundary. fs/ide.c has code to read
	// the disk.
	//
	// LAB 5: you code here:

	addr = ROUNDDOWN(addr, PGSIZE);
	sys_page_alloc(0, addr, PTE_P | PTE_W | PTE_U);
	ide_read(BLKSECTS * blockno, addr, BLKSECTS);

	// Clear the dirty bit for the disk block page since we just read the
	// block from disk
	if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);

	bc_page_count++;
	if(bc_page_count == BC_PRE_MAX_PAGE_COUNT) {
		clear_pages_access_flag();
	}
	else if(bc_page_count >= BC_MAX_PAGE_COUNT) {
		bc_evict_pages();
	}

	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_SYSCALL constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	// LAB 5: Your code here.
	if(!va_is_mapped(addr) || !va_is_dirty(addr)) {
		return;
	}
	ide_write(blockno * BLKSECTS, ROUNDDOWN(addr,PGSIZE), BLKSECTS);
	sys_page_map(0, ROUNDDOWN(addr, PGSIZE), 0, ROUNDDOWN(addr, PGSIZE), PTE_SYSCALL);
	//panic("flush_block not implemented");
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{
	struct Super super;
	set_pgfault_handler(bc_pgfault);
	check_bc();

	// cache the super block by reading it once
	memmove(&super, diskaddr(1), sizeof super);
}


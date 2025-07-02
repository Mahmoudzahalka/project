// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if((err & FEC_WR) == 0 || (uvpt[PGNUM(addr)] & PTE_COW) == 0) {
		panic("pgfault: faulting access was not a write and to a copy-on-write page!\n");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	int ret = sys_page_alloc(0, PFTEMP, PTE_P | PTE_W | PTE_U);
	if(ret != 0) {
		panic("pgfault: sys_page_alloc failed!\n");
	}
	memcpy(PFTEMP, ROUNDDOWN(addr,PGSIZE), PGSIZE);
	ret = sys_page_map(0, PFTEMP, 0, ROUNDDOWN(addr,PGSIZE), PTE_P | PTE_U | PTE_W);
	if(ret != 0)
		panic("pgfault: sys_page_map failed!\n");
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void *address = (void*)(pn * PGSIZE);
	if(uvpt[pn] & (PTE_W | PTE_COW) && !(uvpt[pn] & PTE_SHARE)) {
		int ret = sys_page_map(0, address, envid, address, PTE_U | PTE_P | PTE_COW);
		if(ret != 0)
			panic("duppage: sys_page_map failed!\n");
		ret = sys_page_map(0, address, 0, address, PTE_U | PTE_P | PTE_COW);
		if(ret != 0)
			panic("duppage: sys_page_map failed!\n");
	}
	else {
		int ret = sys_page_map(0, address, envid, address, (uvpt[pn] & PTE_SYSCALL));
		if(ret != 0)
			panic("duppage: sys_page_map failed!\n");
	}
	//panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault);
	envid_t ret_env = sys_exofork();
	if(ret_env != 0) {
		if(ret_env < 0) {
			panic("fork: failed to create child process!\n");
		}
		uint32_t i = 0;
		//lab 6:zero copy
		//for(i = 64 * PGSIZE; i < UTOP-PGSIZE; i+= PGSIZE) {
		for(i = 64 * PGSIZE; i < UTOP-PGSIZE; i+= PGSIZE) {
			if((uvpd[PDX(i)] & PTE_P) && (uvpt[i/PGSIZE] & PTE_P)) {
				int dub_ret = duppage(ret_env, i/PGSIZE);
				if(dub_ret != 0)
					panic("fork: duppage failed!\n");
			}
		}
	}
	else {//child
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	int ret = sys_page_alloc(ret_env, (void*)(UXSTACKTOP - PGSIZE), PTE_P | PTE_W | PTE_U);
	if(ret != 0)
		panic("fork: sys_page_alloc failed!\n");
	ret = sys_env_set_pgfault_upcall(ret_env, thisenv->env_pgfault_upcall);
	if(ret != 0)
		panic("fork: sys_env_set_pgfault_upcall failed!\n");
	ret = sys_env_set_status(ret_env, ENV_RUNNABLE);
	if(ret != 0)
		panic("fork: sys_env_set_status failed!\n");
	return ret_env;
	//panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}

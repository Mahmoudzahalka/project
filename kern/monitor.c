// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line
#define TRAP_FLAG 256 //2^8


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display stack frames", mon_backtrace },
	{ "showmappings", "Display physical page mappings", mon_showmappings },
	{ "setpermission", "Change the permissions of a mapping", mon_setpermissions },
	{ "dump", "Dump the content of a range of virtual/physical memory", mon_dumpmemory },
	{ "continue", "Continue execution to the next breakpoint", mon_continue},
	{ "singlestep", "Execute a single instruction", mon_singlestep},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t *ebp = (uint32_t*)read_ebp();
	while(ebp != 0x0) {
		struct Eipdebuginfo info;
		debuginfo_eip((uintptr_t) *(ebp + 1), &info);
		cprintf("ebp %08.x eip %08.x args %08.x %08x %08.x %08.x %08.x\n", ebp, *(ebp + 1), *(ebp + 2), *(ebp + 3), *(ebp + 4), *(ebp + 5), *(ebp + 6));
		cprintf("	%s:%d: ", info.eip_file, info.eip_line);
		int cnt = 0;
		while(cnt < info.eip_fn_namelen) {
			cprintf("%c", info.eip_fn_name[cnt]);
			cnt++;
		}
		cprintf("+%d\n", *(ebp+1) - info.eip_fn_addr);
		ebp =(uint32_t*)  *(ebp);
	}
	return 0;
}

uint32_t jos_hex2dec(char c) {
	if(c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	else if(c >= 'A' && c <= 'F')
		return 10 + c - 'A';
	else
		return c - '0';
}

uint32_t jos_stoi(char* str) {
	str += 2;//skip the '0x'
	uint32_t result = 0;
 	while(*str != '\0') {
		result *= 16;
		result += jos_hex2dec(*str);
		str++;
	}
	return result;
}

bool valid_hex(char *num) {
	uint32_t len = 0;
	while(num[len] != '\0') {
		if(len == 0 && num[len] != '0')
			return false;
		else if(len == 1 && num[len] != 'x')
			return false;
		else if(len >= 2 && !((num[len] >= '0' && num[len] <= '9') || (num[len] >= 'a' && num[len] <= 'f') || (num[len] >= 'A' && num[len] <= 'F')))
			return false;
		len++;
	}
	return true;
}

bool valid_showmapping_arguments(int argc, char **argv) {
	if(argc != 3)
		return false;
	else if(!valid_hex(argv[1]) || !valid_hex(argv[2]))
		return false;
	return true;
}

int mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	if(!valid_showmapping_arguments(argc, argv)) {
		cprintf("Invalid arguments!\n");
		cprintf("Usage: showmappings [address 1] [address 2]\n");
		return 0;
	}
	uintptr_t addrs[2];
	addrs[0] = PTE_ADDR(jos_stoi(argv[1]));
	addrs[1] = PTE_ADDR(jos_stoi(argv[2]));
	extern pde_t *kern_pgdir;
	uint32_t iterator;
	cprintf("Virtual page address:	Physical page address(permissions):\n");
	for(iterator = addrs[0]; iterator <= addrs[1]; iterator += PGSIZE) {
		pte_t *pte = pgdir_walk(kern_pgdir, (void*)iterator, 0);
		if(!pte) {
			cprintf("0x%.8x			 0x00000000\n", iterator);
			continue;
		}
		cprintf("0x%.8x:			0x%.8x", iterator, PTE_ADDR(*pte));
		if(*pte & PTE_P)
			cprintf(" | PTE_P");
                if(*pte & PTE_W)
                        cprintf(" | PTE_W");
                if(*pte & PTE_U)
                        cprintf(" | PTE_U");
		cprintf("\n");
	}
	return 0;
}

static bool valid_permissions_input(argc, argv) {
	if(argc < 3)
		return false;
	return true;
}

int mon_setpermissions(int argc, char **argv, struct Trapframe *tf)
{
	if(!valid_permissions_input(argc, argv)) {
		cprintf("Invalid arguments!\n");
		cprintf("Usage: setpermissions [address] [P/W/U/PWT/PCD/A/D/PS/G]\n");
		return 0;
	}
	extern pde_t *kern_pgdir;
	uint32_t addr = PTE_ADDR(jos_stoi(argv[1])), i = 2;
        pte_t *pte = pgdir_walk(kern_pgdir, (void*)addr, 0);
        if(!pte) {
                cprintf("Invalid mapping\n");
                return 0;
        }
	uint32_t perm = *pte & 0xFFF;
	while(i < argc) {
		if(*argv[i] == 'P')
			perm |= PTE_P;
                else if(*argv[i] == 'W')
                        perm |= PTE_W;
                else if(*argv[i] == 'U')
                        perm |= PTE_U;
                else if(strcmp(argv[i], "PWT") == 0)
                        perm |= PTE_PWT;
                else if(strcmp(argv[i], "PCD") == 0)
                        perm |= PTE_PCD;
                else if(*argv[i] == 'A')
                        perm |= PTE_A;
                else if(*argv[i] == 'D')
                        perm |= PTE_D;
                else if(strcmp(argv[i], "PS") == 0)
                        perm |= PTE_PS;
               	else if(*argv[i] == 'G')
                        perm |= PTE_G;
		else if(strcmp(argv[i], "clear") == 0)
			perm = 0;
		else {
			cprintf("Invalid permissions!\n");
			cprintf("Usage: setpermissions [address] [P/W/U/PWT/PCD/A/D/PS/G/clear]\n");
		}
		i++;
	}
	*pte = PTE_ADDR(*pte) | perm;
	return 0;
}

bool valid_dump_arguments(int argc, char **argv)
{
	if(argc != 4)
		return false;
	else if(*argv[1] != 'P' && *argv[1] != 'V')
		return false;
        else if(!valid_hex(argv[2]) || !valid_hex(argv[3]))
                return false;
	return true;
}

int mon_dumpmemory(int argc, char **argv, struct Trapframe *tf)
{
	 if(!valid_dump_arguments(argc, argv)) {
                cprintf("Invalid arguments!\n");
                cprintf("Usage: dump [P(hysical)/V(irtual)] [address 1] [address 2]\n");
                return 0;
        }
	uintptr_t addrs[2];
        addrs[0] = jos_stoi(argv[2]);
        addrs[1] = jos_stoi(argv[3]);
	if(*argv[1] == 'P') {//physical address
		addrs[0] = (uint32_t)KADDR(addrs[0]);
		addrs[1] = (uint32_t)KADDR(addrs[1]);
	}
	unsigned char *value, buffer[4];
	uint32_t count, first_line = 1, buffer_count = 0;
	for(value = (unsigned char *)addrs[0], count = 0; value <= (unsigned char *)addrs[1]; value++) {
		if(count == 0) {
			if(!first_line) {
				cprintf("\n");
			}
			first_line = 0;
			cprintf("0x%08x:", value);
		}
		buffer[buffer_count] = *value;
		buffer_count ++;
		if(buffer_count == 4) {
			cprintf("    0x");
			while(buffer_count) {
				cprintf("%02x", buffer[--buffer_count]);
			}
		}
		count = (count + 1) % 16;
	}
	//clean the buffer in case there are 1-3 bytes left
	while(buffer_count) {
		cprintf("    0x%02x", buffer[--buffer_count]);
	}
	cprintf("\n");
	return 0;
}

int mon_continue(int argc, char **argv, struct Trapframe *tf) {
	if(!tf)
		return 0;
	//enable using this function only if it was called after breakpoint or debug trap.
	if(tf->tf_trapno != T_BRKPT && tf->tf_trapno != T_DEBUG) {
		return 0;
	}
	//set it to zero if it is ON, and keep it zero if it is OFF
	tf->tf_eflags ^= (tf->tf_eflags & TRAP_FLAG);
	return -1;
}

int mon_singlestep(int argc, char **argv, struct Trapframe *tf) {
	if(!tf)
		return 0;
	//enable using this function only if it was called after breakpoint or debug trap.
	if(tf->tf_trapno != T_BRKPT && tf->tf_trapno != T_DEBUG) {
		return 0;
	}
	//set the trap flag to 1, so it causes a trap right after executing the next instruction.
	tf->tf_eflags |= TRAP_FLAG;
	return -1;
}
	


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

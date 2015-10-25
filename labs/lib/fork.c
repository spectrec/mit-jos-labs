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
	int r, pn;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	//cprintf("[%08x] pgfault on %08x\n", env->env_id, addr);

	// LAB 4: Your code here.
	if ((err & FEC_WR) == 0)
		panic("pgfault: non write error, addr = %08x", addr);

	pn = VPN((uint32_t)addr);
	if ((vpt[pn] & PTE_COW) == 0)
		panic("pgfault: write to readonly page (pn = %d), addr = %08x", pn, addr);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.
	if ((r = sys_page_alloc(0, PFTEMP, PTE_U|PTE_P|PTE_W)) != 0)
		panic("sys_page_alloc failed: %e", r);

	memmove(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
	if ((r = sys_page_map(0, PFTEMP, 0, ROUNDDOWN(addr, PGSIZE), PTE_U|PTE_P|PTE_W)) != 0)
		panic("sys_page_map failed: %e", r);

	if ((r = sys_page_unmap(0, PFTEMP)) != 0)
		panic("sys_page_unmap failed: %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why might we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
// 
static int
duppage(envid_t envid, unsigned pn)
{
	void *va = (void *)(pn * PGSIZE);
	pte_t pte = vpt[pn];
	int r;

	if ((pte & PTE_W) != 0 || (pte & PTE_COW) != 0) {
		if ((r = sys_page_map(0, va, envid, va, PTE_P|PTE_U|PTE_COW)) != 0)
			panic("sys_page_map failed: %e", r);

		if ((r = sys_page_map(0, va, 0, va, PTE_P|PTE_U|PTE_COW)) != 0)
			panic("sys_page_map failed: %e", r);
	} else {
		if ((r = sys_page_map(0, va, envid, va, PTE_U|PTE_P)) != 0)
			panic("sys_page_map failed: %e", r);
	}

	return 0;
}

void
duppage2(envid_t dstenv, void *addr)
{
	int r;

	// This is NOT what you should do in your fork.
	if ((r = sys_page_alloc(dstenv, addr, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);
	if ((r = sys_page_map(dstenv, addr, 0, UTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_map: %e", r);
	memmove(UTEMP, addr, PGSIZE);
	if ((r = sys_page_unmap(0, UTEMP)) < 0)
		panic("sys_page_unmap: %e", r);
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
//   Use vpd, vpt, and duppage.
//   Remember to fix "env" and the user exception stack in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	extern unsigned char end[];

	envid_t child_id;
	int r, pdi, pti;

	set_pgfault_handler(pgfault);

	if ((child_id = sys_exofork()) < 0)
		panic("sys_exofork failed: %e", child_id);

	if (child_id == 0) {
		// We're the child.
		// The copied value of the global variable 'env'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		env = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	for (pdi = VPD(0); pdi <= VPD(end); pdi++) {
		if ((vpd[pdi] & PTE_P) == 0)
			continue;

		for (pti = pdi * NPDENTRIES; pti < pdi * NPDENTRIES + NPTENTRIES; pti++) {
			if ((vpt[pti] & PTE_P) == 0)
				continue;

			if ((r = duppage(child_id, pti)) != 0)
				panic("duppage failed: %e", r);
		}
	}

	duppage2(child_id, ROUNDDOWN(&child_id, PGSIZE));

	if ((r = sys_page_alloc(child_id, (void *)(UXSTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W)) != 0)
		panic("sys_page_alloc failed: %e\n", r);
	if ((r = sys_env_set_pgfault_upcall(child_id, env->env_pgfault_upcall)) != 0)
		panic("sys_env_set_pgfault_upcall failed: %e", r);

	// Start the child environment running
	if ((r = sys_env_set_status(child_id, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status failed: %e", r);

	return child_id;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}

#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>

static struct Taskstate ts;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}


void
idt_init(void)
{
	extern struct Segdesc gdt[];

	extern void trap_entry_divide_error();
	extern void trap_entry_debug_exception();
	extern void trap_entry_non_maskable_interrupt();
	extern void trap_entry_breakpoint();
	extern void trap_entry_overflow();
	extern void trap_entry_bounds_check();
	extern void trap_entry_illegal_opcode();
	extern void trap_entry_device_not_available();
	extern void trap_entry_double_fault();
	extern void trap_entry_invalid_task_switch_segment();
	extern void trap_entry_segment_not_present();
	extern void trap_entry_stack_exception();
	extern void trap_entry_general_protection_fault();
	extern void trap_entry_page_fault();
	extern void trap_entry_floating_point_error();
	extern void trap_entry_aligment_check();
	extern void trap_entry_machine_check();
	extern void trap_entry_simd_floating_point_error();
	extern void trap_entry_system_call();

	extern void irq_entry_timer();

	// LAB 3: Your code here.
	SETGATE(idt[T_DIVIDE], 0, GD_KT, trap_entry_divide_error, 0);
	SETGATE(idt[T_DEBUG], 0, GD_KT, trap_entry_debug_exception, 0);
	SETGATE(idt[T_NMI], 0, GD_KT, trap_entry_non_maskable_interrupt, 0);
	SETGATE(idt[T_BRKPT], 0, GD_KT, trap_entry_breakpoint, 3);
	SETGATE(idt[T_OFLOW], 0, GD_KT, trap_entry_overflow, 0);
	SETGATE(idt[T_BOUND], 0, GD_KT, trap_entry_bounds_check, 0);
	SETGATE(idt[T_ILLOP], 0, GD_KT, trap_entry_illegal_opcode, 0);
	SETGATE(idt[T_DEVICE], 0, GD_KT, trap_entry_device_not_available, 0);
	SETGATE(idt[T_DBLFLT], 0, GD_KT, trap_entry_double_fault, 0);
	SETGATE(idt[T_TSS], 0, GD_KT, trap_entry_invalid_task_switch_segment, 0);
	SETGATE(idt[T_SEGNP], 0, GD_KT, trap_entry_segment_not_present, 0);
	SETGATE(idt[T_STACK], 0, GD_KT, trap_entry_stack_exception, 0);
	SETGATE(idt[T_GPFLT], 0, GD_KT, trap_entry_general_protection_fault, 0);
	SETGATE(idt[T_PGFLT], 0, GD_KT, trap_entry_page_fault, 0);
	SETGATE(idt[T_FPERR], 0, GD_KT, trap_entry_floating_point_error, 0);
	SETGATE(idt[T_ALIGN], 0, GD_KT, trap_entry_aligment_check, 0);
	SETGATE(idt[T_MCHK], 0, GD_KT, trap_entry_machine_check, 0);
	SETGATE(idt[T_SIMDERR], 0, GD_KT, trap_entry_simd_floating_point_error, 0);
	SETGATE(idt[T_SYSCALL], 0, GD_KT, trap_entry_system_call, 3);

	// irq's
	SETGATE(idt[IRQ_OFFSET], 0, GD_KT, irq_entry_timer, 0);

	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;

	// Initialize the TSS field of the gdt.
	gdt[GD_TSS >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate), 0);
	gdt[GD_TSS >> 3].sd_s = 0;

	// Load the TSS
	ltr(GD_TSS);

	// Load the IDT
	asm volatile("lidt idt_pd");
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	cprintf("  err  0x%08x\n", tf->tf_err);
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	cprintf("  esp  0x%08x\n", tf->tf_esp);
	cprintf("  ss   0x----%04x\n", tf->tf_ss);
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	if (tf->tf_trapno == T_PGFLT)
		return page_fault_handler(tf);

	if (tf->tf_trapno == T_SYSCALL) {
		int32_t ret, num = tf->tf_regs.reg_eax;
		uint32_t args[] = {
			tf->tf_regs.reg_edx,
			tf->tf_regs.reg_ecx,
			tf->tf_regs.reg_ebx,
			tf->tf_regs.reg_edi,
			tf->tf_regs.reg_esi,
		};

		ret = syscall(num, args[0], args[1], args[2], args[3], args[4]);
		tf->tf_regs.reg_eax = ret;

		return;
	}

	// Handle clock interrupts.
	// LAB 4: Your code here.
	if (tf->tf_trapno == IRQ_OFFSET)
		return sched_yield();

	// Handle spurious interupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}

	// Unexpected trap: The user process or the kernel has a bug.
	cprintf("unexpeted trap: envid == [%08x]\n", curenv->env_id);

	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		assert(curenv);
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNABLE)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.
	if ((tf->tf_cs & 3) != 3) {
		assert((tf->tf_cs & 3) == 0);
		panic("page fault inside kernel");
	}

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return.  In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free.  In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	// LAB 4: Your code here.
	if (curenv->env_pgfault_upcall == NULL) {
		cprintf("[%08x] user fault va %08x ip %08x\n",
			curenv->env_id, fault_va, tf->tf_eip);
		print_trapframe(tf);
		env_destroy(curenv);
	}

	user_mem_assert(curenv, (void *)(UXSTACKTOP-PGSIZE), PGSIZE, PTE_U | PTE_P | PTE_W);
	user_mem_assert(curenv, curenv->env_pgfault_upcall, 4, PTE_P | PTE_U);

	struct UTrapframe utf;
	utf.utf_eflags = tf->tf_eflags;
	utf.utf_fault_va = fault_va;
	utf.utf_regs = tf->tf_regs;
	utf.utf_err = tf->tf_err;
	utf.utf_eip = tf->tf_eip;
	utf.utf_esp = tf->tf_esp;

	if (tf->tf_esp < UXSTACKTOP && tf->tf_esp >= UXSTACKTOP - PGSIZE) {
		tf->tf_esp -= 4; // reserve word (recursive page fault)
	} else {
		tf->tf_esp = UXSTACKTOP;
	}
	tf->tf_eip = (uintptr_t)curenv->env_pgfault_upcall;

	tf->tf_esp -= sizeof(utf);
	if (tf->tf_esp < UXSTACKTOP - PGSIZE) {
		cprintf("[%08x] user fault va %08x, uxstack overflow\n",
			curenv->env_id, fault_va);
		print_trapframe(tf);
		env_destroy(curenv);
	}
	*(struct UTrapframe *)tf->tf_esp = utf;

	env_run(curenv);
}


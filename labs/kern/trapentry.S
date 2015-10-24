/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* The TRAPHANDLER macro defines a globally-visible function for handling
 * a trap.  It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 */ 
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
// http://css.csail.mit.edu/6.858/2011/readings/i386.pdf (page 173)
// http://download.intel.com/design/processor/manuals/253668.pdf (page 259)
TRAPHANDLER_NOEC(trap_entry_divide_error, T_DIVIDE);
TRAPHANDLER_NOEC(trap_entry_debug_exception, T_DEBUG);
TRAPHANDLER_NOEC(trap_entry_non_maskable_interrupt, T_NMI);
TRAPHANDLER_NOEC(trap_entry_breakpoint, T_BRKPT);
TRAPHANDLER_NOEC(trap_entry_overflow, T_OFLOW);
TRAPHANDLER_NOEC(trap_entry_bounds_check, T_BOUND);
TRAPHANDLER_NOEC(trap_entry_illegal_opcode, T_ILLOP);
TRAPHANDLER_NOEC(trap_entry_device_not_available, T_DEVICE);
TRAPHANDLER(trap_entry_double_fault, T_DBLFLT);
TRAPHANDLER(trap_entry_invalid_task_switch_segment, T_TSS);
TRAPHANDLER(trap_entry_segment_not_present, T_SEGNP);
TRAPHANDLER(trap_entry_stack_exception, T_STACK);
TRAPHANDLER(trap_entry_general_protection_fault, T_GPFLT);
TRAPHANDLER(trap_entry_page_fault, T_PGFLT);
TRAPHANDLER_NOEC(trap_entry_floating_point_error, T_FPERR);
TRAPHANDLER(trap_entry_aligment_check, T_ALIGN);
TRAPHANDLER_NOEC(trap_entry_machine_check, T_MCHK);
TRAPHANDLER_NOEC(trap_entry_simd_floating_point_error, T_SIMDERR);

TRAPHANDLER_NOEC(trap_entry_system_call, T_SYSCALL);

// interrupts (no error code, no dpl check)
TRAPHANDLER_NOEC(irq_entry_timer, IRQ_OFFSET);

/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
	// fill in struct `Trapfname'
	pushl %ds
	pushl %es
	pushal

	// prepare for call `trap'
	movl $GD_KD, %eax
	movw %ax, %ds
	movw %ax, %es
	pushl %esp

	call trap // should not return

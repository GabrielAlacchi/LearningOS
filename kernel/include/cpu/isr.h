#ifndef __CPU_ISR
#define __CPU_ISR

#include <types.h>

#define IRQ_OFFSET1 32
#define IRQ_OFFSET2 (IRQ_OFFSET1 + 8)
#define PAGE_FAULT 14
#define DOUBLE_FAULT 8


#define ISR_REGISTER_OK 0x0
#define ISR_ALREADY_REGISTERED 0x1
#define ISR_OOB 0x2

// Define the ISR's for CPU exceptions
extern void _isr_0();
extern void _isr_1();
extern void _isr_2();
extern void _isr_3();
extern void _isr_4();
extern void _isr_5();
extern void _isr_6();
extern void _isr_7();
extern void _isr_8();
extern void _isr_9();
extern void _isr_10();
extern void _isr_11();
extern void _isr_12();
extern void _isr_13();
extern void _isr_14();
extern void _isr_15();
extern void _isr_16();
extern void _isr_17();
extern void _isr_18();
extern void _isr_19();
extern void _isr_20();
extern void _isr_21();
extern void _isr_22();
extern void _isr_23();
extern void _isr_24();
extern void _isr_25();
extern void _isr_26();
extern void _isr_27();
extern void _isr_28();
extern void _isr_29();
extern void _isr_30();
extern void _isr_31();

// IRQ 0-15
extern void _isr_32();
extern void _isr_33();
extern void _isr_34();
extern void _isr_35();
extern void _isr_36();
extern void _isr_37();
extern void _isr_38();
extern void _isr_39();
extern void _isr_40();
extern void _isr_41();
extern void _isr_42();
extern void _isr_43();
extern void _isr_44();
extern void _isr_45();
extern void _isr_46();
extern void _isr_47();


// Function to install the ISR's and IRQs to the IDT and
// load the IDT to the CPU
void isr_install();

// Structure to push registers when saving for ISR
typedef struct {
    u64_t ds;
    u64_t rdi, rsi, rbp, rsp, rdx, rcx, rbx, rax;
    u64_t int_no, err_code;
    u64_t rip, cs, eflags, useresp, ss;
} isr_stack_frame;

typedef void *(*interrupt_handler_t)(const isr_stack_frame *);

int register_isr_handler(u8_t isr_number, interrupt_handler_t handler);
int register_irq_handler(u8_t irq_number, interrupt_handler_t handler);

// One handler for all ISR and IRQs
// It will branch to other functions to handle specific cases as needed.
void root_isr_handler(isr_stack_frame regs);

#endif


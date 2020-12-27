#include <cpu/isr.h>
#include <cpu/idt.h>
#include <driver/pic.h>
#include <driver/vga.h>
#include <types.h>

#include <utility/strings.h>

// Give string values for each exception
char *exception_messages[] = {
    "Division by Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",

    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",

    "Coprocessor Fault",
    "Alignment Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",

    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
};

static volatile interrupt_handler_t registered_handlers[IDT_ENTRIES];

// Install the ISR's to the IDT
void isr_install() {
    println("Clearing out the IDT and handler registrations");
    clear_idt();
    memset(registered_handlers, 0, sizeof(interrupt_handler_t) * IDT_ENTRIES);

    set_idt_gate(0, (u64_t) _isr_0);
    set_idt_gate(1, (u64_t) _isr_1);
    set_idt_gate(2, (u64_t) _isr_2);
    set_idt_gate(3, (u64_t) _isr_3);
    set_idt_gate(4, (u64_t) _isr_4);
    set_idt_gate(5, (u64_t) _isr_5);
    set_idt_gate(6, (u64_t) _isr_6);
    set_idt_gate(7, (u64_t) _isr_7);
    set_idt_gate(8, (u64_t) _isr_8);
    set_idt_gate(9, (u64_t) _isr_9);
    set_idt_gate(10, (u64_t) _isr_10);
    set_idt_gate(11, (u64_t) _isr_11);
    set_idt_gate(12, (u64_t) _isr_12);
    set_idt_gate(13, (u64_t) _isr_13);
    set_idt_gate(14, (u64_t) _isr_14);
    set_idt_gate(15, (u64_t) _isr_15);
    set_idt_gate(16, (u64_t) _isr_16);
    set_idt_gate(17, (u64_t) _isr_17);
    set_idt_gate(18, (u64_t) _isr_18);
    set_idt_gate(19, (u64_t) _isr_19);
    set_idt_gate(20, (u64_t) _isr_20);
    set_idt_gate(21, (u64_t) _isr_21);
    set_idt_gate(22, (u64_t) _isr_22);
    set_idt_gate(23, (u64_t) _isr_23);
    set_idt_gate(24, (u64_t) _isr_24);
    set_idt_gate(25, (u64_t) _isr_25);
    set_idt_gate(26, (u64_t) _isr_26);
    set_idt_gate(27, (u64_t) _isr_27);
    set_idt_gate(28, (u64_t) _isr_28);
    set_idt_gate(29, (u64_t) _isr_29);
    set_idt_gate(30, (u64_t) _isr_30);
    set_idt_gate(31, (u64_t) _isr_31);

    set_idt_gate(32, (u64_t) _isr_32);
    set_idt_gate(33, (u64_t) _isr_33);
    set_idt_gate(34, (u64_t) _isr_34);
    set_idt_gate(35, (u64_t) _isr_35);
    set_idt_gate(36, (u64_t) _isr_36);
    set_idt_gate(37, (u64_t) _isr_37);
    set_idt_gate(38, (u64_t) _isr_38);
    set_idt_gate(39, (u64_t) _isr_39);
    set_idt_gate(40, (u64_t) _isr_40);
    set_idt_gate(41, (u64_t) _isr_41);
    set_idt_gate(42, (u64_t) _isr_42);
    set_idt_gate(43, (u64_t) _isr_43);
    set_idt_gate(44, (u64_t) _isr_44);
    set_idt_gate(45, (u64_t) _isr_45);
    set_idt_gate(46, (u64_t) _isr_46);
    set_idt_gate(47, (u64_t) _isr_47);

    // Remap the PIC controllers
    println("Remapping the PIC Controllers and enabling all interrupts");
    pic_remap(IRQ_OFFSET1, IRQ_OFFSET2);

    // Disable programmable timer interrupts for now. We don't need them until we
    // start scheduling.
    println("Masking all IRQ interrupts");
    for (u8_t irq = 0; irq < 16; irq++) {
        pic_set_mask(irq);
    }

    println("Loading the IDT");
    // Load the IDT to the CPU
    set_idt();

    println("Enabling interrupts");
    // Enable Interrupts
    __asm__ volatile("sti");
}


int register_isr_handler(u8_t isr_number, interrupt_handler_t handler) {
    if (isr_number >= IDT_ENTRIES) {
        return ISR_OOB;
    }

    if (registered_handlers[isr_number]) {
        return ISR_ALREADY_REGISTERED;
    }

    registered_handlers[isr_number] = handler;
    return ISR_REGISTER_OK;
}


int register_irq_handler(u8_t irq_number, interrupt_handler_t handler) {
    int err_code = register_isr_handler(irq_number + IRQ_OFFSET1, handler);

    if (err_code == ISR_REGISTER_OK) {
        // Enable the interrupt in the PIC so the handler can start firing
        pic_clear_mask(irq_number);
    }

    return err_code;
}


void handle_irq(const isr_stack_frame *regs, u8_t irq_number, interrupt_handler_t handler) {
    if (handler == 0) {
        char buf[4];
        itoa(irq_number, buf, 10);
        
        putstr("\rError: Received IRQ", COLOR_WHT, COLOR_RED);
        putstr(buf, COLOR_WHT, COLOR_RED);
        putstr("which has no registered handler", COLOR_WHT, COLOR_RED);
        puts("\n");
    } else {
        // Call the handler before sending EOI
        handler(regs);
    }

    pic_send_eoi(irq_number);
}


void root_isr_handler(isr_stack_frame regs) {
    interrupt_handler_t handler = registered_handlers[regs.int_no];

    if (regs.int_no >= IRQ_OFFSET1 && regs.int_no < IRQ_OFFSET1 + 8) {
        // This is an IRQ for IRQ 0-7
        handle_irq(&regs, regs.int_no - IRQ_OFFSET1, handler);
    } else if (regs.int_no >= IRQ_OFFSET2 && regs.int_no < IRQ_OFFSET2 + 8) {
        // This is an IRQ for IRQ 8-15
        handle_irq(&regs, regs.int_no - IRQ_OFFSET2, handler);
    } else if (handler == 0) {
        putstr("\rError: Received interrupt with no handler: ", COLOR_WHT, COLOR_RED);

        const char *message = exception_messages[regs.int_no];
        putstr(message, COLOR_WHT, COLOR_RED);
        putstr("\n", COLOR_WHT, COLOR_BLK);
    }
}

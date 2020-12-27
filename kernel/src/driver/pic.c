#include <driver/pic.h>
#include <cpu/ports.h>

static inline void io_wait() {
    /* Port 0x80 is used for 'checkpoints' during POST. */
    /* The Linux kernel seems to think it is free for use :-/ */
    asm volatile ( "outb %%al, $0x80" : : "a"(0) );
    /* %%al instead of %0 makes no difference.  TODO: does the register need to be zeroed? */
}

void pic_remap(u8_t offset1, u8_t offset2) {
    u8_t mask1, mask2;

	mask1 = byte_in(PIC1_DATA_PORT);                        // save masks
	mask2 = byte_in(PIC2_DATA_PORT);

    // Restart the PICs
    byte_out(PIC1_CMD_PORT, ICW1_INIT | ICW1_ICW4);
    byte_out(PIC2_CMD_PORT, ICW1_INIT | ICW1_ICW4);

    // Remap the PICs to 32 and 40 (decimal) respectively
    byte_out(PIC1_DATA_PORT, offset1); // 0x20 = 32
    io_wait();
    byte_out(PIC2_DATA_PORT, offset2); // 0x28 = 40
    io_wait();

	byte_out(PIC1_DATA_PORT, 4); // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	io_wait();
	byte_out(PIC2_DATA_PORT, 2); // ICW3: tell Slave PIC its cascade identity (0000 0010)
	io_wait();

    byte_out(PIC1_DATA_PORT, ICW4_8086);
    io_wait();
    byte_out(PIC2_DATA_PORT, ICW4_8086);
    io_wait();

    byte_out(PIC1_DATA_PORT, mask1);
    io_wait();
    byte_out(PIC2_DATA_PORT, mask2);
    io_wait();
}

void pic_send_eoi(u8_t irq_number) {
    if (irq_number >= 8) {
        // Send EOI to the slave PIC
        byte_out(PIC2_CMD_PORT, PIC_EOI);
    }

    // Send EOI to the master PIC.
    byte_out(PIC1_CMD_PORT, PIC_EOI);
}

void pic_set_mask(u8_t irq_number) {
    u16_t pic_port;
    
    if (irq_number >= 8) {
        irq_number -= 8;
        pic_port = PIC2_DATA_PORT;
    } else {
        pic_port = PIC1_DATA_PORT;
    }

    u8_t mask = byte_in(pic_port);
    mask |= (1 << irq_number);
    byte_out(pic_port, mask);
}

void pic_clear_mask(u8_t irq_number) {
    u16_t pic_port;

        
    if (irq_number >= 8) {
        irq_number -= 8;
        pic_port = PIC2_DATA_PORT;
    } else {
        pic_port = PIC1_DATA_PORT;
    }

    u8_t mask = byte_in(pic_port);
    mask &= ~(1 << irq_number);
    byte_out(pic_port, mask);
}

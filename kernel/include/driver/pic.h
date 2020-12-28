#ifndef __DRIVER_PIC
#define __DRIVER_PIC

#include <types.h>

/* IO Ports for PIC1 and PIC2 */
#define PIC1_CMD_PORT 0x20
#define PIC1_DATA_PORT 0x21
#define PIC2_CMD_PORT 0xA0
#define PIC2_DATA_PORT 0xA1

/* Refer to https://wiki.osdev.org/8259_PIC for Details */

#define ICW1_ICW4	0x01		/* ICW4 (not) needed */
#define ICW1_SINGLE	0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	0x10		/* Initialization - required! */
 
#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
#define ICW4_SFNM	0x10		/* Special fully nested (not) */

#define PIC_EOI		0x20		/* End-of-interrupt command code */

/* Remaps the PIC so IRQ(n=0-7) goes to offset1 + n and IRQ(n=8-15) goes to offset2 + (n-8) */
void pic_remap(u8_t offset1, u8_t offset2);

/* Notify the PIC about End of Interrupt */
void pic_send_eoi(u8_t irq_number);

/* Unmask IRQ n */
void pic_clear_mask(u8_t irq_number);

/* Mask IRQ n */
void pic_set_mask(u8_t irq_number);

#endif
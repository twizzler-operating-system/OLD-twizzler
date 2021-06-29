#include <arch/x86_64-acpi.h>
#include <arch/x86_64-io.h>
#include <arch/x86_64-madt.h>
#include <memory.h>
#include <pmap.h>
#include <processor.h>
#include <system.h>
#define MAX_IOAPICS 8

struct ioapic {
	int id;
	void *vaddr;
	int gsib;
};

static struct ioapic ioapics[MAX_IOAPICS];

static void write_ioapic(struct ioapic *chip, const uint8_t offset, const uint32_t val)
{
	/* tell IOREGSEL where we want to write to */
	*(volatile uint32_t *)(chip->vaddr) = offset;
	/* write the value to IOWIN */
	*(volatile uint32_t *)(chip->vaddr + 0x10) = val;
}

#if 0
static uint32_t read_ioapic(struct ioapic *chip, const uint8_t offset)
{
 	/* tell IOREGSEL where we want to read from */
 	*(volatile uint32_t*)(chip->vaddr) = offset;
 	/* return the data from IOWIN */
 	return *(volatile uint32_t*)(chip->vaddr + 0x10);
}
#endif

void ioapic_init(struct ioapic_entry *entry)
{
	for(int i = 0; i < MAX_IOAPICS; i++) {
		if(ioapics[i].id == -1) {
			ioapics[i].id = entry->apicid;
			ioapics[i].vaddr = pmap_allocate(entry->ioapicaddr, 0x1000, PMAP_UC);
			ioapics[i].gsib = entry->gsib;
			return;
		}
	}
	panic("not enough entries to hold all these IOAPICs!");
}

static void write_ioapic_vector(struct ioapic *l,
  int irq,
  char masked,
  char trigger,
  char polarity,
  char mode,
  int vector)
{
	uint32_t lower = 0, higher = 0;
	lower = vector & 0xFF;
	/* 8-10: delivery mode */
	lower |= (mode << 8) & 0x700;
	/* 13: polarity */
	if(polarity)
		lower |= (1 << 13);
	/* 15: trigger */
	if(trigger)
		lower |= (1 << 15);
	/* 16: mask */
	if(masked)
		lower |= (1 << 16);
	/* 56-63: destination. Currently, we just send this to the bootstrap cpu */
	int bootstrap = arch_processor_current_id(); /* TODO (perf): irq routing */
	higher |= (bootstrap << 24) & 0xF;
	write_ioapic(l, irq * 2 + 0x10, 0x10000);
	write_ioapic(l, irq * 2 + 0x10 + 1, higher);
	write_ioapic(l, irq * 2 + 0x10, lower);
}

void arch_interrupt_mask(int v)
{
	int vector = v - 32;
	for(int i = 0; i < MAX_IOAPICS; i++) {
		struct ioapic *chip = &ioapics[i];
		if(chip->id == -1)
			continue;
		if(vector >= chip->gsib && vector < chip->gsib + 24) {
			write_ioapic_vector(
			  chip, vector, 1, vector + chip->gsib > 4 ? 1 : 0, 0, 0, 32 + vector + chip->gsib);
		}
	}
}

void arch_interrupt_unmask(int v)
{
	assert(v >= 32);
	int vector = v - 32;
	for(int i = 0; i < MAX_IOAPICS; i++) {
		struct ioapic *chip = &ioapics[i];
		if(chip->id == -1)
			continue;
		if(vector >= chip->gsib && vector < chip->gsib + 24) {
			write_ioapic_vector(
			  chip, vector, 0, vector + chip->gsib > 4 ? 1 : 0, 0, 0, 32 + vector + chip->gsib);
		}
	}
}

__orderedinitializer(__orderedbefore(MADT_INITIALIZER_ORDER)) static void __ioapic_preinit(void)
{
	for(int i = 0; i < MAX_IOAPICS; i++)
		ioapics[i].id = -1;
}

static void do_init_ioapic(struct ioapic *chip)
{
	for(int i = 0; i < 24; i++) {
		write_ioapic_vector(chip, i, 1, 0, 0, 0, 32 + i + chip->gsib);
	}
}

static inline void io_wait(void)
{
	/* Port 0x80 is used for 'checkpoints' during POST. */
	/* The Linux kernel seems to think it is free for use :-/ */
	asm volatile("outb %%al, $0x80" : : "a"(0));
}
__orderedinitializer(
  __orderedafter(ARCH_INTERRUPT_INITIALIZATION_ORDER)) static void __ioapic_postinit(void)
{
	int found = 0;
	/* get PIC to known state (init), and then disable PIC */

#define PIC1 0x20 /* IO base address for primary PIC */
#define PIC2 0xA0 /* IO base address for secondary PIC */
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2 + 1)
#define ICW1_ICW4 0x01          /* ICW4 (not) needed */
#define ICW1_SINGLE 0x02        /* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04     /* Call address interval 4 (8) */
#define ICW1_LEVEL 0x08         /* Level triggered (edge) mode */
#define ICW1_INIT 0x10          /* Initialization - required! */
#define ICW4_8086 0x01          /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO 0x02          /* Auto (normal) EOI */
#define ICW4_BUF_SECONDARY 0x08 /* Buffered mode/secondary */
#define ICW4_BUF_PRIMARY 0x0C   /* Buffered mode/primary */
#define ICW4_SFNM 0x10          /* Special fully nested (not) */
	unsigned char a1, a2;
	a1 = x86_64_inb(PIC1_DATA); // save masks
	a2 = x86_64_inb(PIC2_DATA);

	x86_64_outb(PIC1_COMMAND,
	  ICW1_INIT | ICW1_ICW4); // starts the initialization sequence (in cascade mode)
	io_wait();
	x86_64_outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();
	x86_64_outb(PIC1_DATA, 32); // ICW2: Primary PIC vector offset
	io_wait();
	x86_64_outb(PIC2_DATA, 40); // ICW2: Secondary PIC vector offset
	io_wait();
	x86_64_outb(
	  PIC1_DATA, 4); // ICW3: tell Primary PIC that there is a slave PIC at IRQ2 (0000 0100)
	io_wait();
	x86_64_outb(PIC2_DATA, 2); // ICW3: tell Secondary PIC its cascade identity (0000 0010)
	io_wait();
	x86_64_outb(PIC1_DATA, ICW4_8086);
	io_wait();
	x86_64_outb(PIC2_DATA, ICW4_8086);
	io_wait();
	x86_64_outb(PIC1_DATA, a1); // restore saved masks.
	x86_64_outb(PIC2_DATA, a2);

	x86_64_outb(0xA1, 0xFF);
	x86_64_outb(0x21, 0xFF);

	for(int i = 0; i < MAX_IOAPICS; i++) {
		if(ioapics[i].id != -1) {
			do_init_ioapic(&ioapics[i]);
			found = 1;
		}
	}
	if(!found)
		panic("no IOAPIC found, don't know how to map interrupts!");
}

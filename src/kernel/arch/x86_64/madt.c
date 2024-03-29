/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch/x86_64-acpi.h>
#include <arch/x86_64-madt.h>
#include <debug.h>
#include <processor.h>
#include <stdint.h>
#include <system.h>

struct __attribute__((packed)) madt_desc {
	struct sdt_header header;
	uint32_t lca;
	uint32_t flags;
};

#define LAPIC_ENTRY 0
#define IOAPIC_ENTRY 1
#define INTSRC_ENTRY 2

struct __attribute__((packed)) lapic_entry {
	struct madt_record rec;
	uint8_t acpiprocid;
	uint8_t apicid;
	uint32_t flags;
};

struct __packed intsrc_entry {
	struct madt_record rec;
	uint8_t bus_src;
	uint8_t irq_src;
	uint32_t gsi;
	uint16_t flags;
};

static struct madt_desc *madt;

__orderedinitializer(
  MADT_INITIALIZER_ORDER + __orderedafter(PROCESSOR_INITIALIZER_ORDER)) static void madt_init(void)
{
	madt = acpi_find_table("APIC");
	assert(madt != NULL);

	uintptr_t table = (uintptr_t)(madt + 1);
	size_t len = madt->header.length - sizeof(*madt);
	for(size_t off = 0; off < len;) {
		struct madt_record *rec = (void *)(table + off);
		if(rec->reclen == 0)
			break;
		// printk("[madt] record type: %d\n", rec->type);
		switch(rec->type) {
			struct lapic_entry *lapic;
			struct intsrc_entry *intsrc;
			case LAPIC_ENTRY:
				lapic = (void *)rec;
				if(lapic->flags & 1) {
					processor_register(lapic->apicid == 0, lapic->apicid);
				}
				break;
			case IOAPIC_ENTRY:
				ioapic_init((struct ioapic_entry *)rec);
				break;
			case INTSRC_ENTRY:
				intsrc = (void *)rec;
				(void)intsrc;
				//			printk(":: INTSRC: %d %d %d %x\n",
				//			  intsrc->bus_src,
				//			  intsrc->irq_src,
				//			  intsrc->gsi,
				//			  intsrc->flags);
				break;
			default:
				break;
		}
		off += rec->reclen;
	}
}

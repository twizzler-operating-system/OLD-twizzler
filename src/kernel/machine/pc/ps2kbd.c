/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <device.h>
#include <init.h>
#include <interrupt.h>
#include <kso.h>
#include <machine/isa.h>
#include <object.h>
#include <processor.h>
#include <syscall.h>

#include <limits.h>

#include <twz/sys/dev/device.h>

#include <arch/x86_64-io.h>

static struct device *kbd_dev;

static void __kbd_interrupt(int v, struct interrupt_handler *ih)
{
	(void)v;
	(void)ih;
	long tmp = x86_64_inb(0x60);
	static bool _f = false;
	if(tmp == 0xe1 && !_f) {
		processor_print_all_stats();
		thread_print_all_threads();
		mm_print_stats();
	}
	_f = !_f;
	device_signal_sync(kbd_dev->root, 0, tmp);
}

static struct interrupt_handler _kbd_ih = {
	.fn = __kbd_interrupt,
};

static void __late_init_kbd(void *a __unused)
{
	/* krc: move */
	kbd_dev = device_create(pc_get_isa_bus(), DEVICE_BT_ISA, 0, DEVICE_ID_KEYBOARD, 0);
	kso_setname(kbd_dev->root, "PS/2 Keyboard");

	interrupt_register_handler(33, &_kbd_ih);
}
POST_INIT(__late_init_kbd, NULL);

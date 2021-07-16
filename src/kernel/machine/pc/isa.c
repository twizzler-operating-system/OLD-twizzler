/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <device.h>
#include <kso.h>
#include <machine/isa.h>
#include <spinlock.h>

static struct device *pc_isa_bus;
static struct spinlock lock = SPINLOCK_INIT;
static _Atomic bool init = false;

struct device *pc_get_isa_bus(void)
{
	if(!init) {
		spinlock_acquire_save(&lock);
		if(!init) {
			/* krc: move */
			pc_isa_bus = device_create(NULL, DEVICE_BT_ISA, DEVICE_TYPE_BUSROOT, 0, 0);
			kso_setname(pc_isa_bus->root, "ISA Bus");
			device_attach_busroot(pc_isa_bus, DEVICE_BT_ISA);
			init = true;
		}
		spinlock_release_restore(&lock);
	}
	return pc_isa_bus;
}

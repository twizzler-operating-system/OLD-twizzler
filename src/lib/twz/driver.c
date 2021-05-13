/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <twz/obj.h>
#include <twz/ptr.h>
#include <twz/sys/dev/pcie.h>

bool pcief_capability_get(struct pcie_function_header *pf, int id, union pcie_capability_ptr *cap)
{
	twzobj pf_obj;
	twz_object_init_ptr(&pf_obj, pf);
	struct pcie_config_space *space = twz_object_lea(&pf_obj, pf->space);
	if(space->device.cap_ptr) {
		size_t offset = space->device.cap_ptr;
		do {
			cap->header = (struct pcie_capability_header *)((char *)space + offset);

			if(cap->header->capid == id)
				return true;
			offset = cap->header->next;
		} while(offset != 0);
	}
	/* TODO: pcie extended caps? */
	return false;
}

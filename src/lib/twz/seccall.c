/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <twz/obj.h>
#include <twz/sys/obj.h>
#include <twz/sys/view.h>

void twz_secure_api_setup_tmp_stack(void)
{
	uint32_t fl;
	twz_view_get(NULL, TWZSLOT_TMPSTACK, NULL, &fl);
	if(!(fl & VE_VALID)) {
		objid_t id;
		/* TODO: get rid of this (or tie this to thread?) */
		if(twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_VIEW, 0, 0, &id)
		   < 0) {
			abort();
		}
		twz_view_fixedset(NULL, TWZSLOT_TMPSTACK, id, VE_VALID | VE_WRITE | VE_READ);
	}
}

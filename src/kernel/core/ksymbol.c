/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <ksymbol.h>

/* TODO (perf): insert these into a hash table */
__noinstrument
const struct ksymbol *ksymbol_find_by_value(uintptr_t val, bool range)
{
	for(size_t i=0;i<kernel_symbol_table_length;i++) {
		if(kernel_symbol_table[i].value == val ||
				(range && kernel_symbol_table[i].value <= val && kernel_symbol_table[i].value + kernel_symbol_table[i].size > val)) {
			return &kernel_symbol_table[i];
		}
	}
	return NULL;
}


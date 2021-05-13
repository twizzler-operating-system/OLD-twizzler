/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <ksymbol.h>

#pragma weak kernel_symbol_table
#pragma weak kernel_symbol_table_length
__attribute__((section(".ksyms"))) __attribute__((weak,used)) const size_t kernel_symbol_table_length = 0;
__attribute__((section(".ksyms"))) const struct ksymbol kernel_symbol_table[] __attribute__ ((weak,used)) = {{0,0,0}};


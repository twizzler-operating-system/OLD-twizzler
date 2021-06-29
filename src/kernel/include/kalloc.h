#pragma once

#define KALLOC_ZERO 1

void *krealloc(void *p, size_t sz, int);
void *krecalloc(void *p, size_t num, size_t sz, int);
void *kalloc(size_t sz, int);
void *kcalloc(size_t num, size_t sz, int);
void kfree(void *p);
void kalloc_system_init(void);

#pragma once

#include <twz/sys/dev/pcie.h>

#include <lib/list.h>
#include <spinlock.h>

struct device;
struct pcie_function {
	struct pcie_config_space *config;
	uint16_t segment;
	uint8_t bus, device, function;
	uint8_t flags;
	struct spinlock lock;
	struct list entry;
	struct device *dev;
};
void pcie_iommu_fault(uint16_t seg, uint16_t sid, uint64_t addr, bool handled);

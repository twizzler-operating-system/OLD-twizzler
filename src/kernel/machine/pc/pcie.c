/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch/x86_64-acpi.h>
#include <debug.h>
#include <device.h>
#include <init.h>
#include <kalloc.h>
#include <kso.h>
#include <machine/pc-pcie.h>
#include <memory.h>
#include <page.h>
#include <slab.h>
#include <syscall.h>
#include <system.h>
#include <twz/sys/dev/bus.h>
#include <twz/sys/dev/device.h>

/* The PCI subsystem. We make some simplifying assumptions:
 *   - Our system bus is PCIe. Thus everything is memory-mapped. We do not support IO-based access.
 *   - Devices that we want to work on support MSI or MSI-X. Thus we don't need to worry about
 *     old-style pin-based interrupts and all that complexity.
 *
 * Each PCIe function has a configuration space (a region of memory that controls the PCI-side of
 * the device) and a set of memory-mapped registers that are device-specific (which control the
 * device itself). The configuration space for each segment of the PCIe system is mapped into a
 * single object that can be controlled from userspace. However, there are things we need to do
 * here:
 *   - Some initialization per-device
 *
 * The goal is to have as little PCI configuration in here as possible.
 */

/* ACPI MCFG table contains an array of entries that describe information for a particular segment
 * of PCIe devices */
struct mcfg_desc_entry {
	uint64_t ba;
	uint16_t pci_seg_group_nr;
	uint8_t start_bus_nr;
	uint8_t end_bus_nr;
	uint32_t _resv;
} __packed;

/* ACPI table for configuring PCIe memory-mapped config space */
struct __packed mcfg_desc {
	struct sdt_header header;
	uint64_t _resv;
	struct mcfg_desc_entry spaces[];
};

static struct mcfg_desc *mcfg;
static size_t mcfg_entries;

struct slabcache sc_pcief;
static DECLARE_LIST(pcief_list);
struct spinlock pcielock = SPINLOCK_INIT;

static void _pf_ctor(void *_u, void *ptr)
{
	(void)_u;
	struct pcie_function *pf = ptr;
	pf->flags = 0;
	pf->lock = SPINLOCK_INIT;
}

static void _pf_dtor(void *_u, void *ptr)
{
	(void)_u;
	(void)ptr;
}

__initializer static void _init_objs(void)
{
	slabcache_init(
	  &sc_pcief, "sc_pcief", sizeof(struct pcie_function), NULL, _pf_ctor, NULL, _pf_dtor, NULL);
}

#include <lib/iter.h>
static struct pcie_function *pcief_lookup(uint16_t space,
  uint8_t bus,
  uint8_t device,
  uint8_t function)
{
	foreach(_pf, list, &pcief_list) {
		struct pcie_function *pf = list_entry(_pf, struct pcie_function, entry);
		if(pf->segment == space && pf->bus == bus && pf->device == device
		   && pf->function == function) {
			return pf;
		}
	}
	return NULL;
}

static void pcief_register(uint16_t space,
  uint8_t bus,
  uint8_t device,
  uint8_t function,
  struct device *dev)
{
	struct pcie_function *pf = slabcache_alloc(&sc_pcief, NULL);
	pf->segment = space;
	pf->bus = bus;
	pf->device = device;
	pf->function = function;
	pf->dev = dev;
	spinlock_acquire_save(&pcielock);
	list_insert(&pcief_list, &pf->entry);
	spinlock_release_restore(&pcielock);
}

static void __alloc_bar(struct object *obj,
  size_t start,
  size_t sz,
  int pref,
  int wc,
  uintptr_t addr)
{
	while(sz > 0) {
		/*
		struct page *pg = page_alloc_nophys();
		pg->addr = addr;
		pg->type = PAGE_TYPE_MMIO;
		pg->flags |= (pref ? (wc ? PAGE_CACHE_WC : PAGE_CACHE_WT) : PAGE_CACHE_UC);
		size_t amount = 0;
		// printk("caching page %lx (sz=%lx, start=%lx)\n", addr, sz, start);
		if(sz >= mm_page_size(1) && !(addr & (mm_page_size(1) - 1))) {
		    pg->level = 1;
		    amount = mm_page_size(1);
		} else {
		    amount = mm_page_size(0);
		}
		*/
		panic("TODO: hook this back up");
		// obj_cache_page(obj, start, pg);
		/*
		if(sz < amount)
		    sz = 0;
		else
		    sz -= amount;
		addr += amount;
		start += amount;
		*/
	}
}

void pcie_iommu_fault(uint16_t seg, uint16_t sid, uint64_t addr, bool handled)
{
	addr |= handled ? 1 : 0;
	uint8_t bus = sid >> 8;
	uint8_t dfn = sid & 0xff;
	struct pcie_function *pf = pcief_lookup(seg, bus, dfn >> 3, dfn & 7);
	if(!pf) {
		printk("[pcie] unhandled pcie iommu fault to %x:%x:%x.%x\n", seg, bus, dfn >> 3, dfn & 7);
		return;
	}
	/* TODO: what to do if we overflow? */
	device_signal_sync(pf->dev->root, DEVICE_SYNC_IOV_FAULT, addr);
}

#include <pmap.h>
static long pcie_function_init(struct device *busdev,
  uint16_t segment,
  int bus,
  int device,
  int function,
  int wc)
{
	/* locate the base address for the config space */
	uintptr_t ba = 0;
	for(size_t i = 0; i < mcfg_entries; i++) {
		if(mcfg->spaces[i].pci_seg_group_nr == segment) {
			ba = mcfg->spaces[i].ba
			     + ((bus - mcfg->spaces[i].start_bus_nr) << 20 | device << 15 | function << 12);
			break;
		}
	}
	if(!ba) {
		return -ENOENT;
	}

	struct pcie_config_space *space = pmap_allocate(ba, sizeof(*space), PMAP_UC);

	/* register a new device */
	uint32_t sid = (uint32_t)segment << 16 | bus << 8 | device << 3 | function;
	unsigned int fnid = function | device << 3 | bus << 8;
	struct device *dev = device_create(busdev, DEVICE_BT_PCIE, 0, sid, fnid);
	if(!dev)
		return -ENOMEM;

	pcief_register(segment, bus, device, function, dev);

	/* init the headers */
	struct pcie_function_header hdr = {
		.bus = bus,
		.device = device,
		.function = function,
		.segment = segment,
		.classid = space->header.class_code,
		.subclassid = space->header.subclass,
		.vendorid = space->header.vendor_id,
		.deviceid = space->header.device_id,
		.progif = space->header.progif,
		.header_type = space->header.header_type,
	};

	/* map the BARs */
	// size_t start = mm_page_size(1);
	int is_normal = (space->header.header_type & 0x7f) == 0;
	for(int i = 0; i < (is_normal ? 6 : 2); i++) {
		uint32_t bar = space->device.bar[i];
		if(!bar)
			continue;
		/* learn size */
		*(volatile uint32_t *)&space->device.bar[i] = 0xffffffff;
		asm volatile("clflush %0" ::"m"(space->device.bar[i]) : "memory");
		uint32_t encsz = (*(volatile uint32_t *)&space->device.bar[i]) & 0xfffffff0;
		*(volatile uint32_t *)&space->device.bar[i] = bar;
		asm volatile("clflush %0" ::"m"(space->device.bar[i]) : "memory");
		size_t sz = ~encsz + 1;
		if(sz > 0x10000000) {
			printk("[pcie] warning - unimplemented: support for large BARs (%ld bytes)\n", sz);
			return -ENOTSUP;
		}
		if(bar & 1) {
			/* TODO: I/O spaces? */
			continue;
		}
		int type = (bar >> 1) & 3;
		int pref = (bar >> 3) & 1;
		uint64_t addr = bar & 0xfffffff0;
		if(type == 2) {
			addr |= ((uint64_t)space->device.bar[i + 1]) << 32;
		}

		uint64_t cflags = (pref ? (wc ? PAGE_CACHE_WC : PAGE_CACHE_WT) : PAGE_CACHE_UC);
		struct object *obj = device_add_mmio(dev, addr, sz, cflags, i);
		kso_setnamef(obj, "MMIO BAR %d", i);

#if 0
		hdr.bars[i] = (volatile void *)start;
		hdr.prefetch[i] = pref;
		hdr.barsz[i] = sz;
		printk("init " IDFMT " bar %d for addr %lx at %lx len=%ld, type=%d (p=%d,wc=%d)\n",
		  IDPR(fobj->id),
		  i,
		  addr,
		  start,
		  sz,
		  type,
		  pref,
		  (wc >> i) & 1);
#endif

		//	start += sz;
		//	start = ((start - 1) & ~(mm_page_size(1) - 1)) + mm_page_size(1);

		if(type == 2)
			i++; /* skip the next bar because we used it in this one to make a 64-bit register */
	}
	struct object *obj = device_add_mmio(dev, ba, 0x1000, PAGE_CACHE_UC, 6);
	kso_setnamef(obj, "MMIO CFGSPC %x", sid);
	// start += 0x1000;
	//__alloc_bar(fobj, start, 0x1000, 0, 0, ba);
	// hdr.space = (void *)start;

	// unsigned int fnid = function | device << 3 | bus << 8;
	// kso_tree_attach_child(pbobj, fobj, fnid);

	obj = device_add_info(dev, &hdr, sizeof(hdr), 0);
	kso_setnamef(obj, "info");
	return 0;
}

static long __pcie_kaction(struct object *obj, long cmd, long arg)
{
	switch(cmd) {
		int bus, device, function;
		uint16_t segment;
		uint16_t wc;
		case KACTION_CMD_PCIE_INIT_DEVICE:
			/* arg specifies which bus, device, and function */
			wc = arg >> 32;
			segment = (arg >> 16) & 0xffff;
			bus = (arg >> 8) & 0xff;
			device = (arg >> 3) & 0x1f;
			function = arg & 7;

			struct device *dev = object_get_kso_data_checked(obj, KSO_DEVICE);
			assert(dev);
			return pcie_function_init(dev, segment, bus, device, function, wc);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

__attribute__((no_sanitize("undefined"))) static void pcie_init_space(struct mcfg_desc_entry *space)
{
	printk("[acpi] found MCFG descriptor table: %p\n", space);
	printk("[pcie] initializing PCIe configuration space at %lx covering %.4d:%.2x-%.2x\n",
	  space->ba,
	  space->pci_seg_group_nr,
	  space->start_bus_nr,
	  space->end_bus_nr);

	uintptr_t start_addr = space->ba;
	uintptr_t end_addr =
	  space->ba + ((space->end_bus_nr - space->start_bus_nr) << 20 | 32 << 15 | 8 << 12);

	struct device *pcie_bus = device_create(NULL, DEVICE_BT_PCIE, DEVICE_TYPE_BUSROOT, 0, 0);
	assert(pcie_bus);
	device_attach_busroot(pcie_bus, DEVICE_BT_PCIE);

	struct object *obj =
	  device_add_mmio(pcie_bus, start_addr, end_addr - start_addr, PAGE_CACHE_UC, 0);
	kso_setnamef(obj, "pcie mmio cfgspc %x", space->pci_seg_group_nr);
	struct pcie_bus_header hdr = {
		.magic = PCIE_BUS_HEADER_MAGIC,
		.start_bus = space->start_bus_nr,
		.end_bus = space->end_bus_nr,
		.segnr = space->pci_seg_group_nr,
		.spaces = (void *)(mm_page_size(1)),
	};
	obj = device_add_info(pcie_bus, &hdr, sizeof(hdr), 0);
	kso_setnamef(obj, "pcie info %x", space->pci_seg_group_nr);
	kso_setnamef(pcie_bus->root, "PCIe bus %.2x::%.2x-%.2x", hdr.segnr, hdr.start_bus, hdr.end_bus);
	pcie_bus->root->kaction = __pcie_kaction;
}

static void __pcie_init(void *arg __unused)
{
	for(size_t i = 0; i < mcfg_entries; i++) {
		pcie_init_space(&mcfg->spaces[i]);
	}
}

__orderedinitializer(__orderedafter(ACPI_INITIALIZER_ORDER)) static void mcfg_init(void)
{
	if(!(mcfg = acpi_find_table("MCFG"))) {
		printk("[pcie] WARNING - no PCI Extended Configuration Space found!\n");
		return;
	}

	mcfg_entries =
	  (mcfg->header.length - sizeof(struct mcfg_desc)) / sizeof(struct mcfg_desc_entry);

	printk("[pcie] found MCFG table (%p) with %ld entries\n", mcfg, mcfg_entries);

	static struct init_call call;
	post_init_call_register(&call, false, __pcie_init, NULL, __FILE__, __LINE__);
}

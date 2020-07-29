#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/objctl.h>
#include <twz/persist.h>
#include <twz/queue.h>
#include <twz/sys.h>
#include <twz/thread.h>

#include <twz/debug.h>
#include <twz/driver/device.h>
#include <twz/driver/nic.h>
#include <twz/driver/pcie.h>

#include "e1000.h"

#include <unistd.h>

#include <thread>

#define LOG2(X) ((unsigned)(8 * sizeof(unsigned long long) - __builtin_clzll((X)) - 1))
#define MIN(a, b) ({ (a) < (b) ? (a) : (b); })

#define BAR_MEMORY 0
#define BAR_FLASH 1
#define BAR_MSIX 3

void *e1000_co_get_regs(twzobj *co, int bar)
{
	struct pcie_function_header *hdr = (struct pcie_function_header *)twz_device_getds(co);
	return twz_object_lea(co, (void *)hdr->bars[bar]);
}

uint8_t e1000_reg_read8(e1000_controller *nc, int r, int bar)
{
	void *regs = e1000_co_get_regs(&nc->ctrl_obj, bar);
	return *(volatile uint8_t *)((char *)regs + r);
}

uint32_t e1000_reg_read32(e1000_controller *nc, int r, int bar)
{
	void *regs = e1000_co_get_regs(&nc->ctrl_obj, bar);
	return *(volatile uint32_t *)((char *)regs + r);
}

uint64_t e1000_reg_read64(e1000_controller *nc, int r, int bar)
{
	void *regs = e1000_co_get_regs(&nc->ctrl_obj, bar);
	return *(volatile uint64_t *)((char *)regs + r);
}

void e1000_reg_write32(e1000_controller *nc, int r, int bar, uint32_t v)
{
	void *regs = e1000_co_get_regs(&nc->ctrl_obj, bar);
	*(volatile uint32_t *)((char *)regs + r) = v;
	asm volatile("sfence;" ::: "memory");
}

void e1000_reg_write64(e1000_controller *nc, int r, int bar, uint64_t v)
{
	void *regs = e1000_co_get_regs(&nc->ctrl_obj, bar);
	*(volatile uint64_t *)((char *)regs + r) = v;
	asm volatile("sfence;" ::: "memory");
}

int e1000c_reset(e1000_controller *nc)
{
	e1000_reg_write32(nc, REG_RCTRL, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_TCTRL, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_CTRL, BAR_MEMORY, CTRL_RST);
	usleep(1);
	while(e1000_reg_read32(nc, REG_CTRL, BAR_MEMORY) & CTRL_RST)
		asm volatile("pause");
	return 0;
}

#include <twz/driver/msi.h>
int e1000c_pcie_init(e1000_controller *nc)
{
	struct pcie_function_header *hdr =
	  (struct pcie_function_header *)twz_device_getds(&nc->ctrl_obj);
	struct pcie_config_space *space = twz_object_lea(&nc->ctrl_obj, hdr->space);
	/* bus-master enable, memory space enable. We can do interrupt disable too, since we'll be using
	 * MSI */
	space->header.command =
	  COMMAND_MEMORYSPACE | COMMAND_BUSMASTER | COMMAND_INTDISABLE | COMMAND_SERRENABLE;
	/* allocate an interrupt vector */
	int r;
	if((r = twz_object_kaction(&nc->ctrl_obj, KACTION_CMD_DEVICE_SETUP_INTERRUPTS, 1)) < 0) {
		fprintf(stderr, "kaction: %d\n", r);
		return -EINVAL;
	}

	/* try to use MSI-X, but fall back to MSI if not available */
	union pcie_capability_ptr cp;
	if(!pcief_capability_get(hdr, PCIE_MSIX_CAPABILITY_ID, &cp)) {
		fprintf(stderr, "[e1000] no interrupt generation method supported\n");
		return -ENOTSUP;
	}

	msix_configure(&nc->ctrl_obj, cp.msix, 1);
	return 0;
}

static void __prep_packet(e1000_controller *nc, packet *p)
{
	if(p->mapped)
		return;
	int r = twz_device_map_object(
	  &nc->ctrl_obj, &nc->packet_obj, (p->pinaddr % OBJ_MAXSIZE) - OBJ_NULLPAGE_SIZE, 0x1000);
	assert(r == 0);
	p->mapped = true;
}

static packet *get_packet(e1000_controller *nc)
{
	if(nc->packet_buffers.size() > 0) {
		packet *p = nc->packet_buffers.back();
		nc->packet_buffers.pop_back();
		__prep_packet(nc, p);
		return p;
	}

	packet *p = new packet();
	p->vaddr = twz_object_lea(&nc->packet_obj, (void *)nc->packet_off);
	p->pinaddr = nc->packet_off + nc->packet_pin - OBJ_NULLPAGE_SIZE;
	p->length = 0x1000;
	fprintf(stderr, "[e1000] alloc new packet: %p (%lx)\n", p->vaddr, p->pinaddr);
	nc->packet_off += 0x1000;
	__prep_packet(nc, p);
	return p;
}

int e1000c_init(e1000_controller *nc)
{
	uint32_t rah = e1000_reg_read32(nc, REG_RAH, BAR_MEMORY);
	uint32_t ral = e1000_reg_read32(nc, REG_RAL, BAR_MEMORY);
	nc->mac[0] = ral & 0xff;
	nc->mac[1] = (ral >> 8) & 0xff;
	nc->mac[2] = (ral >> 16) & 0xff;
	nc->mac[3] = (ral >> 24) & 0xff;
	nc->mac[4] = rah & 0xff;
	nc->mac[5] = (rah >> 8) & 0xff;

	struct nic_header *nh = (struct nic_header *)twz_object_base(&nc->info_obj);
	memcpy(nh->mac, nc->mac, 6);
	nh->flags |= NIC_FL_MAC_VALID;
	twz_thread_sync(THREAD_SYNC_WAKE, &nh->flags, INT_MAX, NULL);

	e1000_reg_write32(nc, REG_CTRL, BAR_MEMORY, CTRL_FD | CTRL_ASDE);

	if(twz_object_new(&nc->buf_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE))
		return -1;

	int r;
	r = twz_object_pin(&nc->buf_obj, &nc->buf_pin, 0);
	if(r)
		return r;
	r = twz_object_ctl(&nc->buf_obj, OCO_CACHE_MODE, 0, 0x7000, OC_CM_UC);
	if(r)
		return r;
	r = twz_device_map_object(&nc->ctrl_obj, &nc->buf_obj, 0, 0x1000000);
	if(r)
		return r;

	r = twz_object_pin(&nc->packet_obj, &nc->packet_pin, 0);
	assert(r == 0);

	nc->nr_tx_desc = 0x1000 / sizeof(e1000_tx_desc);
	nc->nr_rx_desc = 0x1000 / sizeof(e1000_rx_desc);

	e1000_reg_write32(nc, REG_TXDESCLO, BAR_MEMORY, (uint32_t)(nc->buf_pin));
	e1000_reg_write32(nc, REG_TXDESCHI, BAR_MEMORY, (uint32_t)(nc->buf_pin >> 32));
	e1000_reg_write32(nc, REG_RXDESCLO, BAR_MEMORY, (uint32_t)((nc->buf_pin + 0x1000)));
	e1000_reg_write32(nc, REG_RXDESCHI, BAR_MEMORY, (uint32_t)((nc->buf_pin + 0x1000) >> 32));

	nc->tx_ring = (struct e1000_tx_desc *)twz_object_base(&nc->buf_obj);
	nc->rx_ring = (struct e1000_rx_desc *)((char *)nc->tx_ring + 0x1000);

	for(size_t i = 0; i < nc->nr_rx_desc; i++) {
		nc->rx_ring[i].status = 0;
		packet *p = get_packet(nc);
		nc->rx_ring[i].addr = p->pinaddr;
		nc->rx_ring[i].length = p->length;
		nc->packet_desc_map[i] = p;
	}

	for(size_t i = 0; i < nc->nr_tx_desc; i++) {
		nc->tx_ring[i].status = 0;
		nc->tx_ring[i].cmd = 0;
	}

	e1000_reg_write32(nc, REG_TXDESCLEN, BAR_MEMORY, 0x1000);
	e1000_reg_write32(nc, REG_RXDESCLEN, BAR_MEMORY, 0x1000);

	e1000_reg_write32(nc, REG_TXDESCHEAD, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_TXDESCTAIL, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_RXDESCHEAD, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_RXDESCTAIL, BAR_MEMORY, nc->nr_rx_desc - 1);

	e1000_reg_write32(nc,
	  REG_IVAR,
	  BAR_MEMORY,
	  IVAR_EN_RxQ0 | IVAR_EN_RxQ1 | IVAR_EN_TxQ0 | IVAR_EN_TxQ1 | IVAR_EN_OTHER);
	e1000_reg_write32(nc, REG_IAM, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_IMC, BAR_MEMORY, 0xffffffff);
	e1000_reg_write32(
	  nc, REG_IMS, BAR_MEMORY, ICR_LSC | ICR_RXO | ICR_RxQ0 | ICR_RxQ1 | ICR_TxQ0 | ICR_TxQ1);

	e1000_reg_write32(nc,
	  REG_RCTRL,
	  BAR_MEMORY,
	  RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_LBM_NONE | RCTL_BAM | RCTL_SECRC
	    | RCTL_BSIZE_2048);

	e1000_reg_write32(nc,
	  REG_TCTRL,
	  BAR_MEMORY,
	  TCTL_EN | TCTL_PSP | (15 << TCTL_CT_SHIFT) | (64 << TCTL_COLD_SHIFT) | TCTL_RTLC);

	e1000_reg_write32(nc, REG_CTRL, BAR_MEMORY, CTRL_FD | CTRL_ASDE | CTRL_SLU);
	e1000_reg_write32(nc, REG_CTRL_EXT, BAR_MEMORY, ECTRL_DRV_LOAD);
	fprintf(stderr,
	  "[e1000] init controller for MAC %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, %ld,%ld rx,tx descs\n",
	  nc->mac[0],
	  nc->mac[1],
	  nc->mac[2],
	  nc->mac[3],
	  nc->mac[4],
	  nc->mac[5],
	  nc->nr_rx_desc,
	  nc->nr_tx_desc);
	return 0;
}

void e1000_tx_desc_init(struct e1000_tx_desc *desc, uint64_t p, size_t len)
{
	desc->addr = p;
	desc->length = len;
	/* TODO: do we need to report status, etc? */
	desc->cmd = CMD_EOP | CMD_IFCS | CMD_RS | CMD_RPS;
	desc->status = 0;
}

int32_t e1000c_send_packet(e1000_controller *nc, uint64_t pdata, size_t len)
{
	if((nc->cur_tx + 1) % nc->nr_tx_desc == nc->head_tx) {
		return -EAGAIN;
	}

	uint32_t num = nc->cur_tx;
	struct e1000_tx_desc *desc = &nc->tx_ring[nc->cur_tx];
	e1000_tx_desc_init(desc, pdata, len);

	nc->cur_tx = (nc->cur_tx + 1) % nc->nr_tx_desc;
	e1000_reg_write32(nc, REG_TXDESCTAIL, BAR_MEMORY, nc->cur_tx);
	return num;
}

void e1000c_interrupt_recv(e1000_controller *nc, int q)
{
	if(q) {
		fprintf(stderr, "[e1000] got activity on RxQ1\n");
		return;
	}
	uint32_t head = e1000_reg_read32(nc, REG_RXDESCHEAD, BAR_MEMORY);

	struct packet_queue_entry pqe;
	while(nc->head_rx != head) {
		struct e1000_rx_desc *desc = &nc->rx_ring[nc->head_rx];
		packet *packet = nc->packet_desc_map[nc->head_rx];
		assert(packet);
		pqe.qe.info = nc->head_rx;
		pqe.ptr = twz_ptr_swizzle(&nc->rxqueue_obj, packet->vaddr, FE_READ);
		pqe.len = desc->length;
		pqe.flags = 0;
		pqe.cmd = 0;
		assert(nc->packet_info_map[pqe.qe.info] == NULL);
		nc->packet_info_map[pqe.qe.info] = packet;
		nc->packet_desc_map[nc->head_rx] = NULL;
		packet->cached = true;
		fprintf(stderr, "[e1000] recv packet %p (%lx)\n", packet->vaddr, packet->pinaddr);
		queue_submit(&nc->rxqueue_obj, (struct queue_entry *)&pqe, 0);
		// packet.objid = twz_object_guid(&nc->buf_obj);
		// packet.pdata = desc->addr;
		// packet.len = desc->length;
		// packet.stat = 0;
		// packet.qe.info = head;
		nc->head_rx = (nc->head_rx + 1) % nc->nr_rx_desc;
	}

	while(queue_get_finished(&nc->rxqueue_obj, (struct queue_entry *)&pqe, QUEUE_NONBLOCK) == 0) {
		//	fprintf(stderr, "got completion for %d\n", packet.qe.info);

		struct e1000_rx_desc *desc = &nc->rx_ring[nc->tail_rx];
		packet *packet = nc->packet_info_map[pqe.qe.info];
		assert(packet);
		nc->packet_info_map[pqe.qe.info] = NULL;
		nc->packet_desc_map[nc->tail_rx] = packet;
		desc->status = 0;
		desc->addr = packet->pinaddr;
		_clwb(desc);

		if(packet->cached) {
			_clwb_len(packet->vaddr, 0x1000);
			packet->cached = false;
		}

		// desc->status = 0;
		// desc->addr = packet.pdata;
		e1000_reg_write32(nc, REG_RXDESCTAIL, BAR_MEMORY, nc->tail_rx);
		nc->tail_rx = (nc->tail_rx + 1) % nc->nr_rx_desc;
	}

	// e1000_reg_write32(nc, REG_RXDESCTAIL, nc->head_rx);

	// uint32_t tail = e1000_reg_read32(nc, REG_RXDESCTAIL, BAR_MEMORY);
	// fprintf(stderr, "got recv!!: %x %x\n", head, tail);
}

void e1000c_interrupt_send(e1000_controller *nc, int q)
{
	if(q) {
		fprintf(stderr, "[e1000] got activity on TxQ1\n");
		return;
	}
	uint32_t head = e1000_reg_read32(nc, REG_TXDESCHEAD, BAR_MEMORY);
	while(nc->head_tx != head) {
		struct e1000_tx_desc *desc = &nc->tx_ring[nc->head_tx];
		while(!(desc->status & STAT_DD)) {
			asm("pause");
		}

		tx_request *req = nullptr;
		{
			std::unique_lock<std::mutex> lck(nc->mtx);
			req = nc->txs[nc->head_tx];
			nc->txs[nc->head_tx] = nullptr;
		}

		if(req) {
			// fprintf(stderr, "found packet: %d -> %d\n", nc->head_tx, req->packet.qe.info);
			queue_complete(&nc->txqueue_obj, (struct queue_entry *)&req->packet, 0);
			delete req;
		}

		nc->head_tx = (nc->head_tx + 1) % nc->nr_tx_desc;
	}
}

void e1000c_interrupt(e1000_controller *nc)
{
	uint32_t icr = e1000_reg_read32(nc, REG_ICR, BAR_MEMORY);
	// fprintf(stderr, "ICR: %x\n", icr);
	e1000_reg_write32(nc, REG_ICR, BAR_MEMORY, icr);

	if(icr & ICR_LSC) {
		/* TODO */
		e1000_reg_write32(nc, REG_CTRL, BAR_MEMORY, CTRL_FD | CTRL_ASDE | CTRL_SLU);
	}
	if(icr & ICR_RXO) {
		fprintf(stderr, "[e1000] warning - recv queues overrun\n");
	}
	if(icr & ICR_RxQ0) {
		e1000c_interrupt_recv(nc, 0);
	}
	if(icr & ICR_RxQ1) {
		e1000c_interrupt_recv(nc, 1);
	}
	if(icr & ICR_TxQ0) {
		e1000c_interrupt_send(nc, 0);
	}
	if(icr & ICR_TxQ1) {
		e1000c_interrupt_send(nc, 1);
	}
}

void e1000_wait_for_event(e1000_controller *nc)
{
	kso_set_name(NULL, "e1000.event_handler");
	struct device_repr *repr = twz_device_getrepr(&nc->ctrl_obj);
	struct sys_thread_sync_args sa[MAX_DEVICE_INTERRUPTS + 1];
	twz_thread_sync_init(&sa[0], THREAD_SYNC_SLEEP, &repr->syncs[DEVICE_SYNC_IOV_FAULT], 0);
	twz_thread_sync_init(&sa[1], THREAD_SYNC_SLEEP, &repr->interrupts[0].sp, 0);
	for(;;) {
		uint64_t iovf = atomic_exchange(&repr->syncs[DEVICE_SYNC_IOV_FAULT], 0);
		if(iovf & 1) {
			fprintf(stderr, "[nvme] unhandled IOMMU error!\n");
			exit(1);
		}
		bool worked = false;
		uint64_t irq = atomic_exchange(&repr->interrupts[0].sp, 0);
		if(irq) {
			worked = true;
			e1000c_interrupt(nc);
		}
		if(!iovf && !worked) {
			int r = twz_thread_sync_multiple(2, sa, NULL);
			if(r < 0) {
				fprintf(stderr, "[nvme] thread_sync error: %d\n", r);
				return;
			}
		}
	}
}

#include <set>
#include <unordered_map>

void e1000_tx_queue(e1000_controller *nc)
{
	std::set<std::pair<objid_t, size_t>> mapped;
	std::unordered_map<objid_t, uint64_t> pins;
	kso_set_name(NULL, "e1000.queue_handler");
	while(1) {
		tx_request *req = new tx_request;
		queue_receive(&nc->txqueue_obj, (struct queue_entry *)&req->packet, 0);

		void *packet_data = twz_object_lea(&nc->txqueue_obj, req->packet.ptr);

		twzobj data_obj;
		twz_object_from_ptr_cpp(packet_data, &data_obj);
		objid_t data_id = twz_object_guid(&data_obj);

		uint64_t data_pin;
		if(pins.find(data_id) == pins.end()) {
			int r = twz_object_pin(&data_obj, &data_pin, 0);
			assert(!r);
			pins[data_id] = data_pin;
		} else {
			data_pin = pins[data_id];
		}

		size_t offset = (size_t)twz_ptr_local(packet_data);
		offset -= OBJ_NULLPAGE_SIZE;
		if(mapped.find(std::make_pair(data_id, offset)) == mapped.end()) {
			twzobj tmpobj;
			twz_object_init_guid(&tmpobj, twz_object_guid(&data_obj), FE_READ);
			int nr_prep = 128;
			twz_device_map_object(&nc->ctrl_obj, &tmpobj, offset, 0x1000 * nr_prep);
			for(int i = 0; i < nr_prep; i++) {
				mapped.insert(std::make_pair(twz_object_guid(&data_obj), offset + 0x1000 * i));
			}
		}

		{
			std::unique_lock<std::mutex> lck(nc->mtx);
			int32_t pn = e1000c_send_packet(nc, data_pin + offset, req->packet.len);
			if(pn < 0) {
				fprintf(stderr, "TODO: dropped packet\n");
			} else {
				nc->txs.reserve(pn + 1);
				nc->txs[pn] = req;
			}
		}
	}
}

int main(int argc, char **argv)
{
	if(!argv[1] || argc < 5) {
		fprintf(stderr, "usage: e1000 controller-name tx-queue-name rx-queue-name info-name\n");
		return 1;
	}

	e1000_controller nc = {};
	int r = twz_object_init_name(&nc.ctrl_obj, argv[1], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open controller %s: %d\n", argv[1], r);
		return 1;
	}

	r = twz_object_init_name(&nc.txqueue_obj, argv[2], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open txqueue\n");
		return 1;
	}

	r = twz_object_init_name(&nc.rxqueue_obj, argv[3], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open rxqueue\n");
		return 1;
	}

	r = twz_object_init_name(&nc.info_obj, argv[4], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open info obj\n");
		return 1;
	}

	printf("[e1000] starting e1000 controller %s\n", argv[1]);
	r = twz_object_new(&nc.packet_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
	if(r) {
		abort();
	}

	if(e1000c_reset(&nc))
		return -1;

	if(e1000c_pcie_init(&nc))
		return -1;

	if(e1000c_init(&nc))
		return -1;

	nc.init = true;

	e1000c_interrupt(&nc);

	std::thread thr(e1000_tx_queue, &nc);

	struct nic_header *nh = (struct nic_header *)twz_object_base(&nc.info_obj);
	nh->flags |= NIC_FL_UP;
	twz_thread_sync(THREAD_SYNC_WAKE, &nh->flags, INT_MAX, NULL);

	e1000_wait_for_event(&nc);
	return 0;
}

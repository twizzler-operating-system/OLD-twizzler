#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <twz/meta.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/obj/queue.h>
#include <twz/sys/obj.h>
#include <twz/sys/sys.h>
#include <twz/sys/thread.h>

#include <twz/debug.h>
#include <twz/sys/dev/device.h>
#include <twz/sys/dev/pcie.h>

#include "nvme.h"

/* TODO: can we define a large page-size? */

#define NVME_ASQS 1024
#define NVME_ACQS 1024
#define LOG2(X) ((unsigned)(8 * sizeof(unsigned long long) - __builtin_clzll((X)) - 1))

#define MIN(a, b) ({ (a) < (b) ? (a) : (b); })

static size_t nvme_get_ideal_nr_queues(void)
{
	/* TODO */
	return 1;
}

static void nvme_cmd_init_identify(struct nvme_cmd *cmd, uint8_t cns, uint32_t nsid, uint64_t addr)
{
	*cmd = (struct nvme_cmd){};
	cmd->hdr.cdw0 = NVME_CMD_SDW0_OP(NVME_ADMIN_OP_IDENTIFY);
	cmd->hdr.nsid = nsid;
	cmd->hdr.dptr.prpp[0].addr = addr;
	cmd->cmd_dword_10[0] = cns;
}

static void nvme_cmd_init_setfeatures_nq(struct nvme_cmd *cmd, uint16_t nsq, uint16_t ncq)
{
	*cmd = (struct nvme_cmd){};
	cmd->hdr.cdw0 = NVME_CMD_SDW0_OP(NVME_ADMIN_SET_FEATURES);
	cmd->cmd_dword_10[0] = NVME_CMD_SET_FEATURES_NQ;
	cmd->cmd_dword_10[1] = ((uint32_t)(ncq - 1) << 16) | (nsq - 1);
}

static void nvme_cmd_init_read(struct nvme_cmd *cmd,
  uintptr_t addr,
  uint32_t nsid,
  uint64_t lba,
  uint16_t nrblks,
  uint32_t blksz,
  uint32_t pagesz)
{
	*cmd = (struct nvme_cmd){};
	cmd->hdr.cdw0 = NVME_CMD_SDW0_OP(NVME_NVM_CMD_READ);
	cmd->hdr.dptr.prpp[0].addr = addr;
	size_t bytes = blksz * nrblks;
	if(bytes > pagesz) {
		cmd->hdr.dptr.prpp[1].addr = addr + pagesz;
	}
	if(addr % pagesz || nrblks * blksz > 2 * pagesz) {
		fprintf(stderr, "[nvme]: WARNING - NI: large transfer\n");
	}
	cmd->hdr.nsid = nsid;
	cmd->cmd_dword_10[0] = lba & 0xffffffff;
	cmd->cmd_dword_10[1] = lba >> 32;
	cmd->cmd_dword_10[2] = nrblks - 1;
}
#if 0
static void nvme_cmd_init_write(struct nvme_cmd *cmd,
  uintptr_t addr,
  uint32_t nsid,
  uint64_t lba,
  uint16_t nrblks,
  uint32_t blksz,
  uint32_t pagesz)
{
	*cmd = (struct nvme_cmd){};
	cmd->hdr.cdw0 = NVME_CMD_SDW0_OP(NVME_NVM_CMD_WRITE);
	cmd->hdr.dptr.prpp[0].addr = addr;
	size_t bytes = blksz * nrblks;
	if(bytes > pagesz) {
		cmd->hdr.dptr.prpp[1].addr = addr + pagesz;
	}
	if(addr % pagesz || nrblks * blksz > 2 * pagesz) {
		fprintf(stderr, "[nvme]: WARNING - NI: large transfer\n");
	}
	cmd->hdr.nsid = nsid;
	cmd->cmd_dword_10[0] = lba & 0xffffffff;
	cmd->cmd_dword_10[1] = lba >> 32;
	cmd->cmd_dword_10[2] = nrblks - 1;
}
#endif

static void nvme_cmd_init_create_sq(struct nvme_cmd *cmd,
  uintptr_t mem,
  size_t qs,
  uint16_t qid,
  uint16_t cmp_id,
  uint16_t pri)
{
	*cmd = (struct nvme_cmd){};
	cmd->hdr.cdw0 = NVME_CMD_SDW0_OP(NVME_ADMIN_CREATE_SQ);
	cmd->hdr.dptr.prpp[0].addr = mem;
	cmd->cmd_dword_10[0] = ((qs - 1) << 16) | qid;
	cmd->cmd_dword_10[1] =
	  ((uint32_t)cmp_id << 16) | (pri << 1) | NVME_CMD_CREATE_CQ_CDW11_PHYS_CONT;
}

static void nvme_cmd_init_create_cq(struct nvme_cmd *cmd,
  uintptr_t mem,
  size_t qs,
  uint16_t qid,
  uint16_t iv)
{
	*cmd = (struct nvme_cmd){};
	cmd->hdr.cdw0 = NVME_CMD_SDW0_OP(NVME_ADMIN_CREATE_CQ);
	cmd->hdr.dptr.prpp[0].addr = mem;
	cmd->cmd_dword_10[0] = ((qs - 1) << 16) | qid;
	cmd->cmd_dword_10[1] =
	  ((uint32_t)iv << 16) | NVME_CMD_CREATE_CQ_CDW11_INT_EN | NVME_CMD_CREATE_CQ_CDW11_PHYS_CONT;
}

void nvmeq_init(nvme_queue *q,
  struct nvme_cmd *sentries,
  struct nvme_cmp *centries,
  uint32_t count,
  uint32_t *head_db,
  uint32_t *tail_db)
{
	q->subq.entries = sentries;
	q->cmpq.entries = centries;
	q->sps = (atomic_uint_least64_t *)calloc(count, sizeof(uint64_t) * count);
	q->cvs = new std::condition_variable[count];
	q->count = count;
	q->cmpq.head_doorbell = head_db;
	q->subq.tail_doorbell = tail_db;
	q->subq.head = q->subq.tail = 0;
	q->cmpq.head = q->cmpq.tail = 0;
	q->subq.phase = true;
	q->cmpq.phase = true;
}

uint16_t nvmeq_submit_cmd(nvme_queue *q, struct nvme_cmd *cmd)
{
	size_t index = q->subq.tail;
	cmd->hdr.cdw0 |= (uint32_t)(index << 16);
	memcpy((void *)&q->subq.entries[q->subq.tail], cmd, sizeof(*cmd));
	atomic_thread_fence(std::memory_order_acq_rel);

	size_t new_tail = (q->subq.tail + 1) & (q->count - 1);
	bool wr = new_tail < q->subq.tail;
	q->subq.tail = new_tail;
	*q->subq.tail_doorbell = q->subq.tail;
	q->subq.phase ^= wr;
	return (uint16_t)index;
}

void *nvme_co_get_regs(twzobj *co)
{
	struct pcie_function_header *hdr = (struct pcie_function_header *)twz_device_getds(co);
	return twz_object_lea(co, (void *)hdr->bars[0]);
}

uint32_t nvme_reg_read32(nvme_controller *nc, int r)
{
	void *regs = nvme_co_get_regs(&nc->co);
	return *(volatile uint32_t *)((char *)regs + r);
}

uint64_t nvme_reg_read64(nvme_controller *nc, int r)
{
	void *regs = nvme_co_get_regs(&nc->co);
	return *(volatile uint64_t *)((char *)regs + r);
}

void nvme_reg_write32(nvme_controller *nc, int r, uint32_t v)
{
	void *regs = nvme_co_get_regs(&nc->co);
	*(volatile uint32_t *)((char *)regs + r) = v;
	asm volatile("sfence;" ::: "memory");
}

void nvme_reg_write64(nvme_controller *nc, int r, uint64_t v)
{
	void *regs = nvme_co_get_regs(&nc->co);
	*(volatile uint64_t *)((char *)regs + r) = v;
	asm volatile("sfence;" ::: "memory");
}

#include <twz/sys/dev/msi.h>
int nvmec_pcie_init(nvme_controller *nc)
{
	struct pcie_function_header *hdr = (struct pcie_function_header *)twz_device_getds(&nc->co);
	struct pcie_config_space *space = twz_object_lea(&nc->co, hdr->space);
	/* bus-master enable, memory space enable. We can do interrupt disable too, since we'll be using
	 * MSI */
	space->header.command =
	  COMMAND_MEMORYSPACE | COMMAND_BUSMASTER | COMMAND_INTDISABLE | COMMAND_SERRENABLE;
	/* allocate an interrupt vector */
	nc->nrvec = nvme_get_ideal_nr_queues();
	int r;
	if((r = twz_object_kaction(&nc->co, KACTION_CMD_DEVICE_SETUP_INTERRUPTS, nc->nrvec)) < 0) {
		fprintf(stderr, "kaction: %d\n", r);
	}

	/* try to use MSI-X, but fall back to MSI if not available */
	union pcie_capability_ptr cp;
	nc->msix = pcief_capability_get(hdr, PCIE_MSIX_CAPABILITY_ID, &cp);
	if(!nc->msix && !pcief_capability_get(hdr, PCIE_MSI_CAPABILITY_ID, &cp)) {
		fprintf(stderr, "[nvme] no interrupt generation method supported\n");
		return -ENOTSUP;
	}

	if(nc->msix) {
		msix_configure(&nc->co, cp.msix, nc->nrvec);
	} else {
		fprintf(stderr, "[nvme] TODO: not implemented: MSI (not MSI-X support)\n");
		return -ENOTSUP;
	}
	return 0;
}

int nvmec_reset(nvme_controller *nc)
{
	/* cause a controller reset */
	nvme_reg_write32(nc, NVME_REG_CC, 0);
	/* wait for RDY to clear, indicating reset completed */
	while(nvme_reg_read32(nc, NVME_REG_CSTS) & NVME_CSTS_RDY)
		asm("pause");
	return 0;
}

int nvmec_wait_for_ready(nvme_controller *nc)
{
	/* TODO: timeout */
	uint32_t status;
	while(!((status = nvme_reg_read32(nc, NVME_REG_CSTS)) & (NVME_CSTS_RDY | NVME_CSTS_CFS)))
		asm("pause");

	if(status & NVME_CSTS_CFS) {
		fprintf(stderr, "[nvme] fatal controller status\n");
		return -1;
	}

	return 0;
}

uint32_t *nvmec_get_doorbell(nvme_controller *nc, uint32_t dnr, bool tail)
{
	void *regs = nvme_co_get_regs(&nc->co);
	return (uint32_t *)((char *)regs
	                    + (tail ? NVME_REG_SQnTDBL(dnr, nc->dstride)
	                            : NVME_REG_CQnHDBL(dnr, nc->dstride)));
}

#include <twz/sys/dev/device.h>
int nvmec_init(nvme_controller *nc)
{
	uint64_t caps = nvme_reg_read64(nc, NVME_REG_CAP);
	/* controller requires we set queue entry sizes and host page size. */
	uint32_t cmd = NVME_CC_MPS(4096 /*TODO: arch-dep*/)
	               | NVME_CC_IOCQES(LOG2(sizeof(struct nvme_cmp)))
	               | NVME_CC_IOSQES(LOG2(sizeof(struct nvme_cmd)));

	/* try for weighted-round-robin */
	if(caps & (1 << 17))  // wrr w/ urgent
		cmd |= (1 << 11); // enable arb method

	nc->max_queue_slots = (caps & 0xffff) + 1;
	nc->page_size = 4096;

	/* controller requires we set the AQA register before enabling. */
	uint32_t aqa = NVME_ASQS | (NVME_ACQS << 16);
	nvme_reg_write32(nc, NVME_REG_AQA, aqa);

	/* allocate the admin queue */
	objid_t aq_id;
	int r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &aq_id);
	if(r)
		return r;
	twz_object_init_guid(&nc->qo, aq_id, FE_READ | FE_WRITE);

	r = twz_object_pin(&nc->qo, &nc->aq_pin, 0);
	if(r)
		return r;

	size_t q_total_len = NVME_ASQS * sizeof(struct nvme_cmd) + NVME_ASQS * sizeof(struct nvme_cmp);

	r = twz_object_ctl(&nc->qo, OCO_CACHE_MODE, 0, q_total_len, OC_CM_UC);
	if(r)
		return r;

	r = twz_device_map_object(&nc->co, &nc->qo, 0, q_total_len);
	if(r)
		return r;
#if 0
	nvme_queue *queues =
	  calloc(1024 /* TODO: read max nr queues */, sizeof(nvme_queue));
	nc->queues = queues;
	nc->nr_queues = 1;
#endif
	nc->nr_queues = 0;

	nvmeq_init(&nc->admin_queue,
	  (nvme_cmd *)((uint64_t)twz_object_base(&nc->qo)),
	  (nvme_cmp *)((uint64_t)twz_object_base(&nc->qo) + NVME_ASQS * sizeof(struct nvme_cmd)),
	  NVME_ASQS,
	  nvmec_get_doorbell(nc, 0, false),
	  nvmec_get_doorbell(nc, 0, true));
	nvme_reg_write64(nc, NVME_REG_ASQ, nc->aq_pin);
	nvme_reg_write64(nc, NVME_REG_ACQ, nc->aq_pin + NVME_ASQS * sizeof(struct nvme_cmd));

	if(!nc->msix) {
		/* disable all interrupts for now */
		nvme_reg_write32(nc, NVME_REG_INTMS, 0xFFFFFFFF);
	}

	/* set the command, and set the EN bit in a separate write. Note: this might not be necessary,
	 * but this isn't performance critical and the spec doesn't make it clear if these can be
	 * combined. This is safer. :) */
	nvme_reg_write32(nc, NVME_REG_CC, cmd);
	nvme_reg_write32(nc, NVME_REG_CC, cmd | NVME_CC_EN);

	nc->dstride = 1 << (NVME_CAP_DSTRD(caps) + 2);
	// fprintf(stderr, "[nvme] dstride is %d\n", nc->dstride);
	if(!nc->msix) {
		/* unmask all interrupts */
		nvme_reg_write32(nc, NVME_REG_INTMC, 0xFFFFFFFF);
	}
	return 0;
}

int nvmec_check_features(nvme_controller *nc)
{
	struct pcie_function_header *hdr = (struct pcie_function_header *)twz_device_getds(&nc->co);
	if(hdr->classid != 1 || hdr->subclassid != 8 || hdr->progif != 2) {
		fprintf(stderr,
		  "[nvme]: controller is not an NVMe controller (%x %x %x)\n",
		  hdr->classid,
		  hdr->subclassid,
		  hdr->progif);
		return -1;
	}

	uint64_t caps = nvme_reg_read64(nc, NVME_REG_CAP);
	uint32_t vs = nvme_reg_read32(nc, NVME_REG_VS);
	if(!(caps & (1ul << 37))) {
		fprintf(stderr, "[nvme] controller does not support NVM command set\n");
		return -1;
	}
	if(vs >> 16 == 0) {
		fprintf(stderr, "[nvme] controller reports bad version number (%x)\n", vs);
		return -1;
	}
	return 0;
}

void nvme_cmp_decode(uint64_t sp, uint32_t *dw0, uint32_t *dw3)
{
	*dw0 = sp >> 32;
	*dw3 = sp & 0xffffffff;
}

#define NVME_CMP_DW3_STATUS(x) ((x) >> 17)

#include <limits.h>

void nvmec_execute_cmd_async(struct nvme_cmd *cmd, struct nvme_request *req, nvme_queue *q)
{
	std::unique_lock<std::mutex> lck(q->reqs_lock);
	uint16_t cid = nvmeq_submit_cmd(q, cmd);
	q->reqs[cid] = req;
}

int nvmec_execute_cmd(struct nvme_cmd *cmd, nvme_queue *q, uint16_t *sr, uint32_t *cres)
{
	uint16_t cid = nvmeq_submit_cmd(q, cmd);

	uint64_t res = 0;
	{
		std::unique_lock<std::mutex> _lg(q->reqs_lock);
		while(q->sps[cid] == 0) {
			q->cvs[cid].wait(_lg);
		}
		res = q->sps[cid];
		q->sps[cid] = 0;
	}
	uint32_t cr, _sr;
	nvme_cmp_decode(res, &cr, &_sr);
	uint16_t status = NVME_CMP_DW3_STATUS(_sr);
	*sr = status;
	*cres = cr;
	return 0;
}

int nvmec_create_queues(nvme_controller *nc, size_t nrqueues, size_t slots)
{
	struct nvme_cmd cmd;
	nvme_cmd_init_setfeatures_nq(&cmd, nrqueues, nrqueues);

	uint32_t cres;
	uint16_t status;
	if(nvmec_execute_cmd(&cmd, &nc->admin_queue, &status, &cres))
		return -1;
	if(status) {
		fprintf(stderr, "[nvme] set features: %x\n", status);
		return -1;
	}

	size_t ncsq = cres & 0xffff;
	size_t nccq = cres >> 16;
	size_t cqs = MIN(ncsq, nccq);
	nrqueues = MIN(cqs, nrqueues);
	fprintf(stderr,
	  "[nvme] allocated %ld queue pairs, %ld slots (cqs = %ld bytes, sqs = %ld bytes)\n",
	  nrqueues,
	  slots,
	  sizeof(struct nvme_cmp) * slots,
	  sizeof(struct nvme_cmd) * slots);

	// nc->queues = (nvme_queue *)calloc(nrqueues, sizeof(nvme_queue));
	nc->queues = new nvme_queue[nrqueues];
	struct nvme_cmd *squeue_start =
	  (nvme_cmd *)(NVME_ASQS * (sizeof(struct nvme_cmd) + sizeof(struct nvme_cmp)));
	struct nvme_cmp *cqueue_start =
	  (nvme_cmp *)(NVME_ASQS * (sizeof(struct nvme_cmd) + sizeof(struct nvme_cmp))
	               + slots * nrqueues * sizeof(struct nvme_cmd));
	for(size_t i = 0; i < nrqueues; i++, squeue_start += slots, cqueue_start += slots) {
		int r;

		r = twz_object_ctl(&nc->qo,
		  OCO_CACHE_MODE,
		  squeue_start - OBJ_NULLPAGE_SIZE,
		  slots * sizeof(struct nvme_cmd) + OBJ_NULLPAGE_SIZE,
		  OC_CM_UC);
		if(r)
			return r;
		r = twz_object_ctl(&nc->qo,
		  OCO_CACHE_MODE,
		  cqueue_start - OBJ_NULLPAGE_SIZE,
		  slots * sizeof(struct nvme_cmp) + OBJ_NULLPAGE_SIZE,
		  OC_CM_UC);
		if(r)
			return r;
		nvmeq_init(&nc->queues[i],
		  (nvme_cmd *)((uint64_t)twz_object_base(&nc->qo) + (uint64_t)squeue_start),
		  (nvme_cmp *)((uint64_t)twz_object_base(&nc->qo) + (uint64_t)cqueue_start),
		  slots,
		  nvmec_get_doorbell(nc, i + 1, false),
		  nvmec_get_doorbell(nc, i + 1, true));

		r = twz_device_map_object(
		  &nc->co, &nc->qo, (uintptr_t)squeue_start, slots * sizeof(struct nvme_cmd));
		if(r)
			return r;
		r = twz_device_map_object(
		  &nc->co, &nc->qo, (uintptr_t)cqueue_start, slots * sizeof(struct nvme_cmp));
		if(r)
			return r;

		nvme_cmd_init_create_cq(
		  &cmd, nc->aq_pin + (uintptr_t)(cqueue_start), slots, i + 1, i % nc->nrvec);

		if(nvmec_execute_cmd(&cmd, &nc->admin_queue, &status, &cres))
			return -1;
		if(status) {
			fprintf(stderr, "[nvme] create cq: %x\n", status);
			return -1;
		}

		nvme_cmd_init_create_sq(
		  &cmd, nc->aq_pin + (uintptr_t)(squeue_start), slots, i + 1, i + 1, NVME_PRIORITY_HIGH);

		if(nvmec_execute_cmd(&cmd, &nc->admin_queue, &status, &cres))
			return -1;
		if(status) {
			fprintf(stderr, "[nvme] create sq: %x\n", status);
			return -1;
		}
	}
	nc->nr_queues = nrqueues;
	return 0;
}

int nvmec_identify(nvme_controller *nc)
{
	struct nvme_cmd cmd;
	void *ci_memory = (void *)((uintptr_t)twz_object_base(&nc->qo) + 0x200000);
	void *nsl_memory = (void *)((uintptr_t)twz_object_base(&nc->qo) + 0x200000 + 0x1000);
	void *ns_memory = (void *)((uintptr_t)twz_object_base(&nc->qo) + 0x200000 + 0x2000);
	// void *r_memory = (void *)((uintptr_t)twz_object_base(&nc->qo) + 0x200000 + 0x3000);

	int r = twz_device_map_object(&nc->co, &nc->qo, 0x200000, 0x4000);
	if(r)
		return r;

	nvme_cmd_init_identify(&cmd, 1, 0, nc->aq_pin + 0x200000);
	uint32_t cres;
	uint16_t status;
	if(nvmec_execute_cmd(&cmd, &nc->admin_queue, &status, &cres))
		return -1;
	if(status) {
		fprintf(stderr, "[nvme] identify failed (command error %x)\n", status);
		return -1;
	}

	nvme_controller_ident *ci = (nvme_controller_ident *)ci_memory;
	(void)ci; // TODO: check features

	struct nvme_namespace_ident *nsi = (struct nvme_namespace_ident *)ns_memory;
	uint32_t *nsl = (uint32_t *)nsl_memory;

	nvme_cmd_init_identify(&cmd, 2, 0, nc->aq_pin + 0x200000 + 0x1000);
	if(nvmec_execute_cmd(&cmd, &nc->admin_queue, &status, &cres))
		return -1;
	if(status) {
		fprintf(stderr, "[nvme] identify namespace IDs failed (command error %x)\n", status);
		return -1;
	}

	for(uint32_t n = 0; n < 1024; n++) {
		if(!nsl[n])
			continue;
		nvme_cmd_init_identify(&cmd, 0, nsl[n], nc->aq_pin + 0x200000 + 0x2000);
		if(nvmec_execute_cmd(&cmd, &nc->admin_queue, &status, &cres))
			return -1;
		if(status) {
			fprintf(stderr, "[nvme] identify namespace failed (command error %x)\n", status);
			return -1;
		}
		uint32_t lbaf = nsi->lbaf[nsi->flbas & 15];
		if(lbaf & 0xffff) {
			fprintf(stderr, "[nvme] namespace supports metadata, which I dont :)\n");
			continue;
		}
		struct nvme_namespace *ns = (nvme_namespace *)malloc(sizeof(*ns));
		uint32_t lba_size = (1 << ((lbaf >> 16) & 0xff));
		ns->size = nsi->nsze * lba_size;
		ns->use = nsi->nuse * lba_size;
		ns->cap = nsi->ncap * lba_size;
		ns->lba_size = lba_size;
		ns->id = nsl[n];
		ns->nc = nc;
		ns->next = nc->namespaces;
		nc->namespaces = ns;
		fprintf(stderr,
		  "[nvme] identified namespace (sz=%ld bytes, cap=%ld bytes, lba=%d bytes)\n",
		  ns->size,
		  ns->cap,
		  ns->lba_size);
	}

#if 0
	nvme_cmd_init_read(&cmd, nc->aq_pin + 0x200000 + 0x3000, 1, 0, 1, 512, 4096);
	if(nvmec_execute_cmd(nc, &cmd, &nc->queues[1], &status, &cres))
		return -1;
	if(status) {
		fprintf(stderr, "[nvme] identify namespace IDs failed (command error %x)\n", status);
		return -1;
	}
	fprintf(stderr, "READ: %x %x\n", status, cres);
	fprintf(stderr, "%s\n", (char *)r_memory);
#endif
	return 0;
}

struct nvme_cmp *nvmeq_cq_peek(nvme_queue *q, uint32_t i, bool *phase)
{
	uint32_t index = (q->cmpq.head + i) & (q->count - 1);
	struct nvme_cmp *c = (struct nvme_cmp *)&q->cmpq.entries[index];
	*phase = q->cmpq.phase ^ (index < q->cmpq.head);
	return c;
}

void nvmeq_sq_adv_head(nvme_queue *q, uint32_t head)
{
	bool wr = head < q->subq.head;
	q->subq.head = head;
	q->subq.phase ^= wr;
}

void nvmeq_cq_consume(nvme_queue *q, uint32_t c)
{
	uint32_t head = (q->cmpq.head + c) & (q->count - 1);
	bool wr = head < q->cmpq.head;
	q->cmpq.head = head;
	q->cmpq.phase ^= wr;
	*q->cmpq.head_doorbell = head;
}

#define NVME_CMP_DW2_HEAD(x) ((x)&0xffff)
#define NVME_CMP_DW3_PHASE (1 << 16)

void nvmeq_interrupt(nvme_controller *nc, nvme_queue *q)
{
	/* try to advance the queue. Note that, since cid is never zero, any all-zero entries can be
	 * totally skipped, regardless of phase */
	uint32_t i = 0;
	bool more = true;
	struct nvme_cmp *buf[16];
	while(i < 16) {
		bool phase;
		struct nvme_cmp *cmp = nvmeq_cq_peek(q, i, &phase);
		if(!!(cmp->cmp_dword[3] & NVME_CMP_DW3_PHASE) != phase) {
			more = false;
			break;
		}
		uint32_t head = NVME_CMP_DW2_HEAD(cmp->cmp_dword[2]);
		nvmeq_sq_adv_head(q, head);
		buf[i] = cmp;
		i++;
	}
	if(i == 0) {
		return;
	}
	if(i > 0) {
		nvmeq_cq_consume(q, i);
	}
	struct sys_thread_sync_args sas[16];
	size_t tsc = 0;
	for(uint32_t j = 0; j < i; j++) {
		uint16_t cid = buf[j]->cmp_dword[3] & 0xffff;
		uint64_t result = ((uint64_t)buf[j]->cmp_dword[0] << 32) | buf[j]->cmp_dword[3];
		nvme_request *req = NULL;
		{
			std::unique_lock<std::mutex> lck(q->reqs_lock);
			req = q->reqs[cid];
			q->reqs[cid] = NULL;
		}
		if(req) {
			uint32_t cr, _sr;
			nvme_cmp_decode(result, &cr, &_sr);
			uint16_t status = NVME_CMP_DW3_STATUS(_sr);
			req->bio.result = status ? BIO_RESULT_ERR : BIO_RESULT_OK;
			queue_complete(&nc->ext_qobj, (struct queue_entry *)&req->bio, 0);
			delete req;
		} else {
			std::unique_lock<std::mutex> lck(q->reqs_lock);
			q->sps[cid] = result;
			q->cvs[cid].notify_all();
		}
	}
	int r;
	if(tsc && (r = twz_thread_sync_multiple(tsc, sas, NULL)) < 0) {
		fprintf(stderr, "nvme interrupt thread_sync %d failed: %d\n", i, r);
		abort();
	}
	if(more)
		nvmeq_interrupt(nc, q);
}

void nvme_wait_for_event(nvme_controller *nc)
{
	kso_set_name(NULL, "nvme.event_handler");
	struct device_repr *repr = twz_device_getrepr(&nc->co);
	struct sys_thread_sync_args sa[MAX_DEVICE_INTERRUPTS + 1];
	twz_thread_sync_init(&sa[0], THREAD_SYNC_SLEEP, &repr->syncs[DEVICE_SYNC_IOV_FAULT], 0);
	for(int i = 1; i <= nc->nrvec; i++) {
		twz_thread_sync_init(&sa[i], THREAD_SYNC_SLEEP, &repr->interrupts[i - 1].sp, 0);
	}
	int x = 0;
	for(;;) {
		uint64_t iovf = atomic_exchange(&repr->syncs[DEVICE_SYNC_IOV_FAULT], 0);
		if(iovf & 1) {
			/* handled; retry */
			nvmeq_interrupt(nc, &nc->admin_queue);
			for(size_t i = 0; i < nc->nr_queues; i++) {
				nvmeq_interrupt(nc, &nc->queues[i]);
			}
			nc->sp_error = 1;
			int r = twz_thread_sync(THREAD_SYNC_WAKE, &nc->sp_error, INT_MAX, NULL);
			if(r) {
				fprintf(stderr, "[nvme] thread_sync error: %d\n", r);
				return;
			}
		} else if(iovf) {
			fprintf(stderr, "[nvme] unhandled IOMMU error!\n");
			exit(1);
		}
		bool worked = false;
		for(int i = 0; i < nc->nrvec; i++) {
			uint64_t irq = atomic_exchange(&repr->interrupts[i].sp, 0);
			if(irq) {
				worked = true;
				if(i == 0) {
					nvmeq_interrupt(nc, &nc->admin_queue);
				}
				for(size_t q = i; q < nc->nr_queues; q += nc->nrvec) {
					nvmeq_interrupt(nc, &nc->queues[q]);
				}
			}
		}
		if(!iovf && !worked) {
			int r = twz_thread_sync_multiple(nc->nrvec + 1, sa, NULL);
			if(r < 0) {
				fprintf(stderr, "[nvme] thread_sync error: %d\n", r);
				return;
			}
		}
	}
}

#include <twz/sys/dev/queue.h>

#include <set>
#include <thread>

void *ptm(void *arg)
{
	kso_set_name(NULL, "nvme.request-handler");
	std::set<std::pair<objid_t, size_t>> mapped;
	nvme_controller *nc = (nvme_controller *)arg;
	nvmec_create_queues(nc, nvme_get_ideal_nr_queues(), nc->max_queue_slots);
	// nvmec_identify(nc);

	while(1) {
		nvme_request *req = new nvme_request;
		queue_receive(&nc->ext_qobj, (struct queue_entry *)&req->bio, 0);
		size_t offset = req->bio.linaddr % OBJ_MAXSIZE;
		offset -= OBJ_NULLPAGE_SIZE;

#if 0
		debug_printf("[nvme] nvme got bio: %d %ld :: %lx (" IDFMT ")\n",
		  req->bio.qe.info,
		  req->bio.blockid,
		  req->bio.linaddr,
		  IDPR(req->bio.tmpobjid));
#endif

		if(mapped.find(std::make_pair(req->bio.tmpobjid, offset)) == mapped.end()) {
			twzobj tmpobj;
			twz_object_init_guid(&tmpobj, req->bio.tmpobjid, FE_READ);
			int nr_prep = 128;
			int r = twz_device_map_object(&nc->co, &tmpobj, offset, 0x1000 * nr_prep);
			if(r) {
				fprintf(stderr, "[nvme]: :( :( %d\n", r);
			}
			for(int i = 0; i < nr_prep; i++) {
				mapped.insert(std::make_pair(req->bio.tmpobjid, offset + 0x1000 * i));
			}
		}

		struct nvme_cmd cmd;
		nvme_cmd_init_read(&cmd, req->bio.linaddr, 1, req->bio.blockid * 8, 8, 512, 4096);
		nvmec_execute_cmd_async(&cmd, req, &nc->queues[0]);
	}

	return 0;
}

int main(int argc, char **argv)
{
	if(!argv[1] || argc < 3) {
		fprintf(stderr, "usage: nvme controller-name queue-name\n");
		return 1;
	}

	nvme_controller nc = {};
	int r = twz_object_init_name(&nc.co, argv[1], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "nvme: failed to open controller %s: %d\n", argv[1], r);
		return 1;
	}
	r = twz_object_init_name(&nc.ext_qobj, argv[2], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "nvme: failed to open queue\n");
		return 1;
	}

	printf("[nvme] starting NVMe controller %s\n", argv[1]);

	if(nvmec_check_features(&nc))
		return 1;

	if(nvmec_reset(&nc))
		return -1;

	if(nvmec_pcie_init(&nc))
		return -1;

	if(nvmec_init(&nc))
		return -1;

	if(nvmec_wait_for_ready(&nc))
		return -1;
	nc.init = true;

	std::thread thr(ptm, &nc);

	nvme_wait_for_event(&nc);
	return 0;
}

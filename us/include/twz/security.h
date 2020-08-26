#pragma once

#include <twz/_key.h>
#include <twz/_sctx.h>

int twz_cap_create(struct sccap **cap,
  objid_t target,
  objid_t accessor,
  uint32_t perms,
  struct screvoc *revoc,
  struct scgates *gates,
  uint16_t htype,
  uint16_t etype,
  twzobj *);

int twz_dlg_create(struct scdlg **dlg,
  void *item,
  size_t itemlen,
  objid_t delegator,
  uint32_t mask,
  struct screvoc *revoc,
  struct scgates *gates,
  uint16_t htype,
  uint16_t etype);

twzobj;
int twz_sctx_init(twzobj *obj, const char *name);
int twz_sctx_add(twzobj *obj,
  objid_t target,
  void *item,
  size_t itemlen,
  uint32_t,
  struct scgates *);
ssize_t twz_sctx_lookup(twzobj *obj, objid_t target);
ssize_t twz_sctx_next(twzobj *obj, objid_t target, ssize_t bucket);
void twz_sctx_set_gmask(twzobj *obj, uint32_t gmask);
int twz_sctx_add_dfl(twzobj *obj,
  objid_t target,
  uint32_t mask,
  struct scgates *gatemask,
  uint64_t flags);
int twz_key_new(twzobj *pri, twzobj *pub);

void twz_secure_api_create(twzobj *obj, const char *name);
// int twz_sctx_read(twzobj *obj, objid_t target, void *data, size_t *datalen);
// int twz_sctx_del(twzobj *obj, objid_t target, ssize_t bucketnum);

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <twz/alloc.h>
#include <twz/debug.h>
#include <twz/gate.h>
#include <twz/obj.h>
#include <twz/security.h>
#include <twz/view.h>

#include <twz/sys.h>

void *__twz_secapi_nextstack = NULL;
void *__twz_secapi_nextstack_backup = NULL;

__asm__(".global libtwzsec_gate_return;\n"
        "libtwzsec_gate_return:\n"
        "movabs __twz_secapi_nextstack_backup, %rax\n"
        "mfence\n"
        "movabs %rax, __twz_secapi_nextstack\n"
        "mfence\n"
        "movq %rdi, %rsi\n"
        "xorq %rdx, %rdx\n"
        "xorq %rdi, %rdi\n"
        "movq $6, %rax\n"
        "syscall\nud2");

extern int main();
extern int libtwz_panic();
extern int libtwzsec_gate_return();

int twz_context_add_perms(twzobj *sctx, twzobj *key, twzobj *obj, uint64_t perms)
{
	struct sccap *cap;
	int r = twz_cap_create(&cap,
	  twz_object_guid(obj),
	  twz_object_guid(sctx),
	  perms,
	  NULL,
	  NULL,
	  SCHASH_SHA1,
	  SCENC_DSA,
	  key);
	if(r)
		return r;

	/* probably get the length from some other function? */
	r = twz_sctx_add(sctx, twz_object_guid(obj), cap, sizeof(*cap) + cap->slen, ~0, NULL);
	free(cap);
	return r;
}

int twz_object_set_user_perms(twzobj *obj, uint64_t perms)
{
	const char *k = getenv("TWZUSERKEY");
	if(!k) {
		return -EINVAL;
	}
	objid_t pkeyid;
	if(!objid_parse(k, strlen(k), &pkeyid)) {
		return -EINVAL;
	}

	/* TODO: could try to verify that the public key is right */
	/*
	k = getenv("TWZUSERKU");
	if(!k) {
	    return -EINVAL;
	}
	objid_t ukeyid;
	if(!objid_parse(k, strlen(k), &ukeyid)) {
	    return -EINVAL;
	}
	*/

	k = getenv("TWZUSERSCTX");
	if(!k) {
		return -EINVAL;
	}
	objid_t ctxid;
	if(!objid_parse(k, strlen(k), &ctxid)) {
		return -EINVAL;
	}

	int r;
	twzobj obj_kr, obj_ctx;
	if((r = twz_object_init_guid(&obj_kr, pkeyid, FE_READ))) {
		return r;
	}

	if((r = twz_object_init_guid(&obj_ctx, ctxid, FE_READ | FE_WRITE))) {
		twz_object_release(&obj_kr);
		return r;
	}
	r = twz_context_add_perms(&obj_ctx, &obj_kr, obj, perms);

	twz_object_release(&obj_kr);
	twz_object_release(&obj_ctx);
	return r;
}

void twz_secure_api_create(twzobj *obj, const char *name)
{
	__twz_secapi_nextstack = (char *)malloc(0x2000) + 0x2000;
	__twz_secapi_nextstack_backup = __twz_secapi_nextstack;
	struct secure_api_header *hdr = twz_object_base(obj);

	twzobj pub, pri, context;
	twzobj orig_view, new_view;

	/* TODO: perms */
	if(twz_object_new(&context, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_USE)) {
		errx(1, "failed to make new object");
	}

	twz_sctx_init(&context, name);
	if(twz_object_new(&pri, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_USE)) {
		errx(1, "failed to make new object");
	}

	if(twz_object_new(&pub, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_USE)) {
		errx(1, "failed to make new object");
	}

	twz_key_new(&pri, &pub);

	twz_view_object_init(&orig_view);

	if(twz_object_new(&new_view, &orig_view, &pub, TWZ_OC_VOLATILE)) {
		abort(); // TODO
	}

	struct sccap *cap;
	twz_cap_create(&cap,
	  twz_object_guid(&new_view),
	  twz_object_guid(&context),
	  SCP_WRITE | SCP_READ | SCP_USE,
	  NULL,
	  NULL,
	  SCHASH_SHA1,
	  SCENC_DSA,
	  &pri);

	/* probably get the length from some other function? */
	twz_sctx_add(&context, twz_object_guid(&new_view), cap, sizeof(*cap) + cap->slen, ~0, NULL);

	twz_sctx_set_gmask(&context, SCP_EXEC);

	/* TODO: get these in a better way */
	twzobj exec = twz_object_from_ptr(main);
	twz_sctx_add_dfl(&context, twz_object_guid(&exec), SCP_EXEC, NULL, SCF_TYPE_REGRANT_MASK);

	exec = twz_object_from_ptr(libtwzsec_gate_return);
	twz_sctx_add_dfl(&context, twz_object_guid(&exec), SCP_EXEC, NULL, SCF_TYPE_REGRANT_MASK);

	exec = twz_object_from_ptr(libtwz_panic);
	twz_sctx_add_dfl(&context, twz_object_guid(&exec), SCP_EXEC, NULL, SCF_TYPE_REGRANT_MASK);

	exec = twz_object_from_ptr((void *)0x4000C0069697);
	twz_sctx_add_dfl(&context, twz_object_guid(&exec), SCP_EXEC, NULL, SCF_TYPE_REGRANT_MASK);

	exec = twz_object_from_ptr((void *)0x700080001000);
	twz_sctx_add_dfl(&context, twz_object_guid(&exec), SCP_EXEC, NULL, SCF_TYPE_REGRANT_MASK);

	exec = twz_object_from_ptr((void *)0x700100001000);
	twz_sctx_add_dfl(&context, twz_object_guid(&exec), SCP_EXEC, NULL, SCF_TYPE_REGRANT_MASK);

	// printf("\n\nAdding cap for lib  " IDFMT " to " IDFMT "\n",
	// IDPR(twz_object_guid(&new_view)),
	// IDPR(twz_object_guid(&context)));

	// twzobj view;
	// twz_view_object_init(&view);
	hdr->view = twz_object_guid(&new_view);
	hdr->sctx = twz_object_guid(&context);
	if(sys_attach(0, hdr->sctx, 0, KSO_SECCTX)) {
		abort();
		/* TODO: abort */
	}

	twz_view_set(&new_view, TWZSLOT_CVIEW, twz_object_guid(&new_view), VE_READ | VE_WRITE);

	struct sys_become_args ba = {
		.target_view = hdr->view,
	};
	if(sys_become(&ba, 0, SYS_BECOME_INPLACE)) {
		abort();
	}
	twzobj __v;
	twz_object_init_guid(&__v, hdr->view, FE_READ | FE_WRITE);
}

#define align_up(x, a) ({ ((x) + (a)-1) & ~(a - 1); })

int twz_sctx_init(twzobj *obj, const char *name)
{
	struct secctx *sc = twz_object_base(obj);
	if(name) {
		kso_set_name(obj, name);
	}
	sc->nbuckets = 1024;
	sc->nchain = 4096;
	twz_object_init_alloc(obj,
	  align_up(
	    sizeof(*sc) + sizeof(struct scbucket) * (sc->nbuckets + sc->nchain) + OBJ_NULLPAGE_SIZE,
	    0x1000));

	return 0;
}

void twz_sctx_set_gmask(twzobj *obj, uint32_t gmask)
{
	struct secctx *sc = twz_object_base(obj);
	sc->gmask = gmask;
}

static int __sctx_add_bucket(struct secctx *sc,
  objid_t target,
  void *ptr,
  uint32_t pmask,
  struct scgates *gatemask,
  uint64_t flags)
{
	size_t slot = target % sc->nbuckets;
	while(1) {
		struct scbucket *b = &sc->buckets[slot];
		if(b->target == 0) {
			b->target = target;
			b->data = twz_ptr_local(ptr);
			b->flags = (gatemask ? SCF_GATE : 0) | (flags & ~SCF_GATE);
			b->gatemask = gatemask ? *gatemask : (struct scgates){ 0 };
			b->pmask = pmask;
			break;
		}
		slot = b->chain;
		if(slot == 0) {
			for(size_t i = sc->nbuckets; i < sc->nbuckets + sc->nchain; i++) {
				struct scbucket *n = &sc->buckets[slot];
				if(n->chain == 0 && n->target == 0) {
					b->chain = slot = i;
					break;
				}
			}
			if(slot == 0) {
				return -ENOSPC;
			}
		}
	}
	return 0;
}

int twz_sctx_add_dfl(twzobj *obj,
  objid_t target,
  uint32_t mask,
  struct scgates *gatemask,
  uint64_t flags)
{
	struct secctx *sc = twz_object_base(obj);
	return __sctx_add_bucket(sc, target, NULL, mask, gatemask, flags);
}

int twz_sctx_add(twzobj *obj,
  objid_t target,
  void *item,
  size_t itemlen,
  uint32_t pmask,
  struct scgates *gatemask)
{
	struct secctx *sc = twz_object_base(obj);

	/* this is a bit of a hack with bootstrapped security contexts (generated outside twizzler).
	 * TODO: a better solution would be to properly support setting up an OA header from outside
	 * twizzler.
	 */

	/* this isn't thread safe, but... it's okay for now (see above TODO) */
	// if(oa->end == 0) {
	//	oa_hdr_init(obj, oa, align_up(sc->alloc.max, 0x1000), OBJ_TOPDATA);
	//}

	/* TODO: need to switch to new alloc API */
	void *data = NULL;
	twz_alloc(obj, itemlen, &data, 0, NULL, NULL);
	if(!data) {
		return -ENOMEM;
	}
	data = twz_object_lea(obj, data);
	memcpy(data, item, itemlen);

	int r = __sctx_add_bucket(sc, target, data, pmask, gatemask, 0);
	if(r) {
		/* TODO:free */
	}

	return r;
}

/*
struct sccap {
    objid_t target;
    objid_t accessor;
    struct screvoc rev;
    struct scgates gates;
    uint32_t perms;
    uint16_t magic;
    uint16_t flags;
    uint16_t htype;
    uint16_t etype;
    uint16_t slen;
    uint16_t pad;
    char sig[];
} __attribute__((packed));
*/

#define LTM_DESC
#include <tomcrypt.h>

#include <err.h>
static void sign_dsa(unsigned char *hash,
  size_t hl,
  const unsigned char *b64data,
  size_t b64len,
  unsigned char *sig,
  size_t *siglen)
{
	register_prng(&sprng_desc);
	ltc_mp = ltm_desc;
	size_t kdlen = b64len;
	unsigned char *kd = malloc(b64len);
	int e;
	// for(size_t i = 0; i < b64len; i++) {
	//	printf("%c", b64data[i]);
	//}
	if((e = base64_decode(b64data, b64len, kd, &kdlen)) != CRYPT_OK) {
		errx(1, "b64_decode: %s", error_to_string(e));
	}
	dsa_key k;
	if((e = dsa_import(kd, kdlen, &k)) != CRYPT_OK) {
		errx(1, "dsa_import: %s", error_to_string(e));
	}

	if((e = dsa_sign_hash(hash, hl, sig, siglen, NULL, find_prng("sprng"), &k)) != CRYPT_OK) {
		errx(1, "dsa_sign_hash: %s", error_to_string(e));
	}

	int stat;
	if((e = dsa_verify_hash(sig, *siglen, hash, hl, &stat, &k)) != CRYPT_OK) {
		errx(1, "dsa_verify_hash: %s", error_to_string(e));
	}

	if(!stat) {
		errx(1, "failed to test validate signature");
	}
}

int twz_key_new(twzobj *pri, twzobj *pub)
{
	/* probaby shouldn't do this on every function call */
	ltc_mp = ltm_desc;

	prng_state prng;
	int err;
	/* register yarrow */
	if(register_prng(&fortuna_desc) == -1) {
		printf("Error registering Fortuna\n");
		return -1;
	}
	/* setup the PRNG */
	if((err = rng_make_prng(128, find_prng("fortuna"), &prng, NULL)) != CRYPT_OK) {
		printf("Error setting up PRNG, %s\n", error_to_string(err));
		return -1;
	}
	/* make a 192-bit ECC key */
	// if((err = ecc_make_key(&prng, find_prng("yarrow"), 24, &mykey)) != CRYPT_OK) {
	//	printf("Error making key: %s\n", error_to_string(err));
	//	return -1;
	//}

	int e;
	dsa_key key;
	/* TODO: parameters */
	if((e = dsa_make_key(&prng, find_prng("fortuna"), 20, 128, &key)) != CRYPT_OK) {
		errx(1, "dsa_make_key: %s", error_to_string(e));
	}

	struct key_hdr *kh = twz_object_base(pri);
	kh->type = SCENC_DSA;
	kh->flags = TWZ_KEY_PRI;
	kh->keydatalen = 0x2000; /* TODO */

	unsigned char *tmp = malloc(0x1000);
	size_t explen = 0x1000;

	if((e = dsa_export(tmp, &explen, PK_PRIVATE, &key))) {
		errx(1, "dsa_export: %s", error_to_string(e));
	}

	const char *pri_line = "-----BEGIN PRIVATE KEY-----\n";
	const char *pri_line_end = "\n-----END PRIVATE KEY-----\n";
	const char *pub_line = "-----BEGIN PUBLIC KEY-----\n";
	const char *pub_line_end = "\n-----END PUBLIC KEY-----\n";

	strcpy((char *)(kh + 1), pri_line);
	unsigned char *kdstart = ((unsigned char *)(kh + 1)) + strlen(pri_line);
	if((e = base64_encode(tmp, explen, kdstart, &kh->keydatalen))) {
		errx(1, "base64_encode: %s", error_to_string(e));
	}
	strcpy((char *)kdstart + kh->keydatalen, pri_line_end);
	kh->keydata = twz_ptr_local(kdstart);

#if 0
	for(size_t i = 0; i < kh->keydatalen + 128; i++) {
		printf("%c ", ((unsigned char *)(kh + 1))[i]);
	}
#endif
	kh = twz_object_base(pub);
	kh->type = SCENC_DSA;
	kh->flags = 0;
	kh->keydatalen = 0x2000; /* TODO */

	explen = 0x1000;
	if((e = dsa_export(tmp, &explen, PK_PUBLIC, &key))) {
		errx(1, "dsa_export: %s", error_to_string(e));
	}

	strcpy((char *)(kh + 1), pub_line);
	kdstart = ((unsigned char *)(kh + 1)) + strlen(pub_line);
	if((e = base64_encode(tmp, explen, kdstart, &kh->keydatalen))) {
		errx(1, "base64_encode: %s", error_to_string(e));
	}
	strcpy((char *)kdstart + kh->keydatalen, pub_line_end);
	kh->keydata = twz_ptr_local(kdstart);

#if 0
	printf("\n");
	for(size_t i = 0; i < kh->keydatalen + 128; i++) {
		printf("%c ", ((unsigned char *)(kh + 1))[i]);
	}
	printf("\n");
#endif
	return 0;
}

int twz_cap_create(struct sccap **cap,
  objid_t target,
  objid_t accessor,
  uint32_t perms,
  struct screvoc *revoc,
  struct scgates *gates,
  uint16_t htype,
  uint16_t etype,
  twzobj *pri_key)
{
	if(htype != SCHASH_SHA1 || etype != SCENC_DSA)
		return -ENOTSUP;
	*cap = malloc(sizeof(**cap));
	(*cap)->target = target;
	(*cap)->accessor = accessor;
	(*cap)->rev = revoc ? *revoc : (struct screvoc){ 0 };
	(*cap)->gates = gates ? *gates : (struct scgates){ 0 };
	(*cap)->perms = perms;
	(*cap)->magic = SC_CAP_MAGIC;
	(*cap)->flags = 0;
	(*cap)->htype = htype;
	(*cap)->etype = etype;
	(*cap)->pad = 0;
	(*cap)->slen = 0;
	(*cap)->flags |= gates ? SCF_GATE : 0;
	(*cap)->flags |= revoc ? SCF_REV : 0;

	unsigned char sig[4096];
	size_t siglen = 0;
	unsigned char out[128];

	size_t keylen;
	unsigned char *keystart;

	// fprintf(stderr, "gate off = %x\n", (*cap)->gates.offset);

	struct key_hdr *kh = twz_object_base(pri_key);
	keystart = twz_object_lea(pri_key, kh->keydata);
	keylen = kh->keydatalen;

	while(siglen != (*cap)->slen || siglen == 0) {
		(*cap)->slen = siglen;
		_Alignas(16) hash_state hs;
		sha1_init(&hs);
		sha1_process(&hs, (unsigned char *)(*cap), sizeof(**cap));
		sha1_done(&hs, out);

		siglen = sizeof(sig);
		memset(sig, 0, siglen);
		sign_dsa(out, 20, (unsigned char *)keystart, keylen, sig, &siglen);
	}

	*cap = realloc(*cap, sizeof(**cap) + (*cap)->slen);
	memcpy((*cap) + 1, sig, (*cap)->slen);

	return 0;
}

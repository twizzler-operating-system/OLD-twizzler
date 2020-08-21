#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <twz/obj.h>
#include <twz/objctl.h>
#include <twz/twztry.h>
#include <unistd.h>
static bool opt_meta = false, opt_data = false, opt_kernel = false, opt_kpage = false;

static char *cache_mode_str[] = {
	[OC_CM_WB] = "wb",
	[OC_CM_UC] = "uc",
	[OC_CM_WT] = "wt",
	[OC_CM_WC] = "wc",
};

static int do_object_kernel(twzobj *obj)
{
	struct kernel_ostat st;
	int r;

	if((r = twz_object_kstat(obj, &st))) {
		return 0;
	}

	printf("KSTAT\n");
	char ff[8], *ffp = ff;
	*ffp++ = (st.flags & OS_FLAGS_PIN) ? 'n' : '-';
	*ffp++ = (st.flags & OS_FLAGS_KERNEL) ? 'k' : '-';
	*ffp++ = (st.flags & OS_FLAGS_PERSIST) ? 'p' : '-';
	*ffp++ = (st.flags & OS_FLAGS_HIDDEN) ? 'h' : '-';
	*ffp++ = (st.flags & OS_FLAGS_ALLOC) ? 'a' : '-';
	*ffp++ = (st.flags & OS_FLAGS_PAGER) ? 'g' : '-';
	*ffp++ = (st.flags & OS_FLAGS_SOURCED) ? 's' : '-';

	printf("  flags cm #sleep #derive kso nvreg\n");
	printf("%s %s %6d %7d %3d %5d\n",
	  ff,
	  cache_mode_str[st.cache_mode],
	  st.nr_sleepers,
	  st.nr_derivations,
	  st.kso_type,
	  st.nvreg);

	return 0;
}

static int do_object_kpage(twzobj *obj)
{
	printf("   page# flags level cowcount\n");
	for(size_t i = 0; i < OBJ_MAXSIZE / 0x1000; i++) {
		struct kernel_ostat_page st;
		if(twz_object_kstat_page(obj, i, &st) == 0) {
			if(st.flags & OS_PAGE_EXIST) {
				char ff[3], *ffp = ff;
				*ffp++ = (st.flags & OS_PAGE_COW) ? 'c' : '-';
				*ffp++ = (st.flags & OS_PAGE_MAPPED) ? 'm' : '-';
				*ffp = 0;
				printf("%8lx    %s %5d %8d\n", st.pgnr, ff, st.level, st.cowcount);
			}
		}
	}
	return 0;
}

static inline struct fotentry *_twz_object_get_fote(twzobj *obj, size_t e)
{
	struct metainfo *mi = twz_object_meta(obj);
	return (struct fotentry *)((char *)mi - sizeof(struct fotentry) * e);
}

static int do_object_meta(twzobj *obj)
{
	struct metainfo *mi = twz_object_meta(obj);
	printf("METAINFO\n");
	printf("flags pflags #fote meta-len                              KUID     size\n");
	if(mi->flags) {
		printf("    s ");
	} else {
		printf("    - ");
	}

	char pf[7], *pfp = pf;

	*pfp++ = (mi->flags & MIP_HASHDATA) ? 'h' : '-';
	*pfp++ = (mi->flags & MIP_DFL_READ) ? 'r' : '-';
	*pfp++ = (mi->flags & MIP_DFL_WRITE) ? 'w' : '-';
	*pfp++ = (mi->flags & MIP_DFL_EXEC) ? 'x' : '-';
	*pfp++ = (mi->flags & MIP_DFL_USE) ? 'u' : '-';
	*pfp++ = (mi->flags & MIP_DFL_DEL) ? 'd' : '-';
	*pfp = 0;

	printf("%s ", pf);

	printf("%5d %8d " IDFMT " %8ld\n", mi->fotentries, mi->milen, IDPR(mi->kuid), mi->sz);

	struct metaext *e = &mi->exts[0];

	printf("METAEXTs\nEntry              Tag Ptr\n");
	int i = 0;
	while((char *)e < (char *)mi + mi->milen) {
		void *p = atomic_load(&e->ptr);
		uint64_t tag = atomic_load(&e->tag);

		if(tag) {
			printf("%5d %16lx %p\n", i, tag, p);
		}

		e++;
		i++;
	}

	printf("FOREIGN OBJECT TABLE\nEntry  Flags Name/ID\n");
	for(unsigned i = 1; i < mi->fotentries; i++) {
		struct fotentry *fe = _twz_object_get_fote(obj, i);
		if(fe->flags == 0)
			continue;

		char ff[7], *ffp = ff;

		*ffp++ = (fe->flags & FE_NAME) ? 'n' : '-';
		*ffp++ = (fe->flags & FE_READ) ? 'r' : '-';
		*ffp++ = (fe->flags & FE_WRITE) ? 'w' : '-';
		*ffp++ = (fe->flags & FE_EXEC) ? 'x' : '-';
		*ffp++ = (fe->flags & FE_USE) ? 'u' : '-';
		*ffp++ = (fe->flags & FE_DERIVE) ? 'd' : '-';
		*ffp = 0;

		printf("%5d %s ", i, ff);
		if(fe->flags & FE_NAME) {
			char *name = twz_object_lea(obj, fe->name.data);
			printf("%s\n", name);
		} else {
			printf(IDFMT, IDPR(fe->id));
		}
		printf("\n");
	}

	return 0;
}

static int do_object_data(twzobj *obj)
{
	return 0;
}

static int do_object(char *name)
{
	int r;
	twzobj obj;

	/* first, try to resolve the name. */
	if((r = twz_object_init_name(&obj, name, 0))) {
		/* failed to resolve the name. We can try just treating the argument as an ID string */
		fprintf(stderr, "ostat: failed to resolve name `%s': %d\n", name, r);
		return -1;
	}

	int ret = 0;
	twztry
	{
		printf("OBJECT %s ID " IDFMT "\n", name, IDPR(twz_object_guid(&obj)));
		if(opt_meta) {
			ret = do_object_meta(&obj);
		}
		if(opt_data) {
			ret = do_object_data(&obj);
		}
		if(opt_kernel) {
			ret = do_object_kernel(&obj);
		}
		if(opt_kpage) {
			ret = do_object_kpage(&obj);
		}
	}
	twzcatch_all
	{
		fprintf(stderr, "ostat: failed to read object `%s': fault %d\n", name, twzcatch_fnum());
		ret = -2;
	}
	twztry_end;

	return ret;
}

static void usage(void)
{
	fprintf(stderr, "usage: ostat [-m] [-d] object_names...\n");
	fprintf(stderr, "flags:\n");
	fprintf(stderr, "       -m   Show metadata\n");
	fprintf(stderr, "       -d   Show data\n");
	fprintf(stderr, "       -k   Show kernel info\n");
	fprintf(stderr, "       -p   Show kernel pages info\n");
}

int main(int argc, char **argv)
{
	int c;
	while((c = getopt(argc, argv, "mdkp")) != EOF) {
		switch(c) {
			case 'm':
				opt_meta = true;
				break;
			case 'd':
				opt_data = true;
				break;
			case 'k':
				opt_kernel = true;
				break;
			case 'p':
				opt_kpage = true;
				break;
			default:
				usage();
				exit(1);
		}
	}

	int ret = 0;
	for(int i = optind; i < argc; i++) {
		int r = do_object(argv[i]);
		if(r > ret) {
			ret = r;
		}
	}

	return ret;
}

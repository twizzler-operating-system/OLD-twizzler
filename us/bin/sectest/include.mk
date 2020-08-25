SECTEST_SRCS=$(addprefix us/bin/sectest/,main.c)
SECTEST_OBJS=$(addprefix $(BUILDDIR)/,$(SECTEST_SRCS:.c=.o))

SECTEST2_SRCS=$(addprefix us/bin/sectest/,stlib.c)
SECTEST2_OBJS=$(addprefix $(BUILDDIR)/,$(SECTEST2_SRCS:.c=.o))

SECTEST_LIBS=-lubsan -Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive -ltwzsec -ltomcrypt -ltommath 
SECTEST_CFLAGS=-g -fPIC -fpie

$(BUILDDIR)/us/sysroot/usr/bin/st: $(SECTEST_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS) $(BUILDDIR)/us/sysroot/usr/lib/libtwzsec.so
	@echo "[LD]      $@"
	$(TWZCC) $(TWZLDFLAGS) -g -o $@ -MD $< $(SECTEST_LIBS)

$(BUILDDIR)/us/bin/sectest/%.o: us/bin/sectest/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(SECTEST_CFLAGS) -o $@ -c -MD $< -fPIC

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/st
#

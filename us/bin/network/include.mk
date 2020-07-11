NETWORK_SRCS=$(addprefix us/bin/network/,network.c)
NETWORK_OBJS=$(addprefix $(BUILDDIR)/,$(NETWORK_SRCS:.c=.o))

#NETWORK_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#NETWORK_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/network: $(NETWORK_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]	$@"
	@$(TWZCC) $(TWZLDFLAGS) -g -o $@ -MD $(NETWORK_OBJS) $(NETWORK_LIBS)

$(BUILDDIR)/us/bin/network/%.o: us/bin/network/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]	$@"
	@$(TWZCC) $(TWZCFLAGS) $(NETWORK_CFLAGS) -o $@ -c -MD $<

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/network

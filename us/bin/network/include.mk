NETWORK_SRCS=$(addprefix us/bin/network/,network.cpp common.cpp interface.cpp eth.cpp twz.cpp arp.cpp ipv4.cpp udp.cpp send.cpp twz_op.cpp)
NETWORK_OBJS=$(addprefix $(BUILDDIR)/,$(NETWORK_SRCS:.cpp=.o))

#NETWORK_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#NETWORK_CFLAGS=-DLOOPBACK_TESTING

$(BUILDDIR)/us/sysroot/usr/bin/network: $(NETWORK_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]	$@"
	@$(TWZCXX) $(TWZLDFLAGS) -g -o $@ -MD $(NETWORK_OBJS) $(NETWORK_LIBS)

$(BUILDDIR)/us/bin/network/%.o: us/bin/network/%.cpp $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]	$@"
	@$(TWZCXX) $(TWZCFLAGS) $(NETWORK_CFLAGS) -o $@ -c -MD $<

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/network

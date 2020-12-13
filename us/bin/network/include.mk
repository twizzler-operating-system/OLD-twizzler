NETWORK_SRCS=$(addprefix us/bin/network/,network.cpp common.cpp interface.cpp eth.cpp twz.cpp arp.cpp ipv4.cpp udp.cpp tcp.cpp encapsulate.cpp generic_ring_buffer.cpp udp_conn.cpp char_ring_buffer.cpp tcp_conn.cpp twz_op.cpp client.cpp client_handling_testing.cpp)
NETWORK_OBJS=$(addprefix $(BUILDDIR)/,$(NETWORK_SRCS:.cpp=.o))

#NETWORK_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#NETWORK_CFLAGS=-DLOOPBACK_TESTING

NETWORK_LIBS=-ltwzsec -ltommath -ltomcrypt   -lubsan

$(BUILDDIR)/us/sysroot/usr/bin/network: $(NETWORK_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]	$@"
	@$(TWZCXX) $(TWZLDFLAGS) -g -o $@ -MD $(NETWORK_OBJS) $(NETWORK_LIBS)

$(BUILDDIR)/us/bin/network/%.o: us/bin/network/%.cpp $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]	$@"
	@$(TWZCXX) $(TWZCFLAGS) $(NETWORK_CFLAGS) -o $@ -c -MD $< -fsanitize=undefined

-include $(NETWORK_OBJS:.o=.d)

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/network

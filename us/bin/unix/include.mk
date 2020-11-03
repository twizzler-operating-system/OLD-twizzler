UNIX_SRCS=$(addprefix us/bin/unix/,unix.cpp cmd.cpp state.cpp files.cpp mmap.cpp dir.cpp)
UNIX_OBJS=$(addprefix $(BUILDDIR)/,$(UNIX_SRCS:.cpp=.o))

#NETWORK_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#NETWORK_CFLAGS=-DLOOPBACK_TESTING

UNIX_LIBS=-ltwzsec -ltommath -ltomcrypt

$(BUILDDIR)/us/sysroot/usr/bin/unix: $(UNIX_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]	$@"
	@$(TWZCXX) $(TWZLDFLAGS) -g -o $@ -MD $(UNIX_OBJS) $(UNIX_LIBS)

$(BUILDDIR)/us/bin/unix/%.o: us/bin/unix/%.cpp $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]	$@"
	@$(TWZCXX) $(TWZCFLAGS) $(UNIX_CFLAGS) -std=gnu++17 -o $@ -c -MD $< -Ius/include

-include $(UNIX_OBJS:.o=.d)

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/unix

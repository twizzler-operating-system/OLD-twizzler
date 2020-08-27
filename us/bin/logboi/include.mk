LOGBOI_SRCS=$(addprefix us/bin/logboi/,logboi.cpp)
LOGBOI_OBJS=$(addprefix $(BUILDDIR)/,$(LOGBOI_SRCS:.cpp=.o))

#LOGBOI_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#LOGBOI_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/logboi: $(LOGBOI_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCXX) $(TWZLDFLAGS) -g -o $@ -MD $< $(LOGBOI_LIBS) -ltwzsec -ltomcrypt -ltommath

$(BUILDDIR)/us/bin/logboi/%.o: us/bin/logboi/%.cpp $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCXX) $(TWZCFLAGS) $(LOGBOI_CFLAGS) -o $@ -c -MD $< -Ius/include

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/logboi

-include $(LOGBOI_OBJS:.o=.d)

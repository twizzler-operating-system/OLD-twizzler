LIBTWZNET_SRC=$(addprefix us/libtwznet/,net.c buf.c)

LIBTWZNET_OBJ=$(addprefix $(BUILDDIR)/,$(LIBTWZNET_SRC:.c=.o))

$(BUILDDIR)/us/libtwznet/%.o: us/libtwznet/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) -fno-omit-frame-pointer -g -Ius/libtwznet/include -c -o $@ -MD -fPIC $<

$(BUILDDIR)/us/libtwznet/libtwznet.so: $(LIBTWZNET_OBJ) $(BUILDDIR)/us/sysroot/usr/lib/libtwz.so
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCC) -o $(BUILDDIR)/us/libtwznet/libtwznet.so -shared $(LIBTWZNET_OBJ)

-include $(LIBTWZNET_OBJ:.o=.d)

LIBTWZSEC_SRC=$(addprefix us/libtwzsec/,sctx.c)

LIBTWZSEC_OBJ=$(addprefix $(BUILDDIR)/,$(LIBTWZSEC_SRC:.c=.o))

$(BUILDDIR)/us/libtwzsec/%.o: us/libtwzsec/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) -fno-omit-frame-pointer -g -Ius/libtwzsec/include -c -o $@ -MD -fPIC $<

$(BUILDDIR)/us/libtwzsec/libtwzsec.so: $(LIBTWZSEC_OBJ) $(BUILDDIR)/us/sysroot/usr/lib/libtwz.so
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCC) -o $(BUILDDIR)/us/libtwzsec/libtwzsec.so -shared $(LIBTWZSEC_OBJ)

-include $(LIBTWZSEC_OBJ:.o=.d)

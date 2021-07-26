struct BstreamHdr {
	struct mutex lock;
	uint32_t flags;
	atomic_uint_least32_t head;
	atomic_uint_least32_t tail;
	uint32_t nbits;
	struct evhdr ev;
	struct twzio_hdr io;
	unsigned char data[];
};

static inline size_t bstream_hdr_size(uint32_t nbits)
{
	return sizeof(struct bstream_hdr) + (1ul << nbits);
}

#define BSTREAM_METAEXT_TAG 0x00000000bbbbbbbb



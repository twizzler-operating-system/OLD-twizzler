struct pbuf {
	struct pbuf *next;
	char data[];
};

struct pbuf_header {
	struct pbuf *first;
	size_t datalen;
	struct mutex lock;
	char *next;
};

struct pbuf *pbuf_alloc(twzobj *bufobj)
{
	struct pbuf_header *hdr = twz_object_base(bufobj);
	mutex_acquire(&hdr->lock);

	if(hdr->first) {
		struct pbuf *ret = hdr->first;
		hdr->first = hdr->first->next;
		mutex_release(&hdr->lock);
		return ret;
	}

	if((uintptr_t)twz_ptr_local(hdr->next) >= OBJ_TOPDATA) {
		mutex_release(&hdr->lock);
		return NULL;
	}
	struct pbuf *buf = (struct pbuf *)hdr->next;
	hdr->next += sizeof(*buf) + hdr->datalen;

	mutex_release(&hdr->lock);
	return buf;
}

void pbuf_release(twzobj *bufobj, struct pbuf *buf)
{
	struct pbuf_header *hdr = twz_object_base(bufobj);
	mutex_acquire(&hdr->lock);
	buf->next = hdr->first;
	hdr->first = twz_object_local(buf);
	mutex_release(&hdr->lock);
}

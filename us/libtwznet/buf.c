#include <stddef.h>
#include <twz/mutex.h>
#include <twz/obj.h>

#include <nstack/net.h>

struct pbuf_header {
	struct pbuf *first;
	size_t datalen;
	struct mutex lock;
	char *next;
};

size_t pbuf_datalen(twzobj *bufobj)
{
	struct pbuf_header *hdr = twz_object_base(bufobj);
	return hdr->datalen;
}

void pbuf_init(twzobj *bufobj, size_t datalen)
{
	struct pbuf_header *hdr = twz_object_base(bufobj);
	mutex_init(&hdr->lock);
	hdr->first = NULL;
	datalen = (datalen & ~15) + 16;
	hdr->datalen = datalen;

	hdr->next = (char *)twz_ptr_local(hdr + 1);
}

struct pbuf *pbuf_alloc(twzobj *bufobj)
{
	struct pbuf_header *hdr = twz_object_base(bufobj);
	mutex_acquire(&hdr->lock);

	if(hdr->first) {
		struct pbuf *ret = twz_object_lea(bufobj, hdr->first);
		hdr->first = twz_ptr_local(ret->next);
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
	return twz_object_lea(bufobj, buf);
}

void pbuf_release(twzobj *bufobj, struct pbuf *buf)
{
	struct pbuf_header *hdr = twz_object_base(bufobj);
	mutex_acquire(&hdr->lock);
	buf->next = hdr->first;
	hdr->first = twz_ptr_local(buf);
	mutex_release(&hdr->lock);
}

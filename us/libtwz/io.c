#include <twz/_err.h>
#include <twz/io.h>
#include <twz/obj.h>

#include <twz/debug.h>
ssize_t twzio_read(twzobj *obj, void *buf, size_t len, size_t off, unsigned flags)
{
	struct twzio_hdr *hdr = twz_object_getext(obj, TWZIO_METAEXT_TAG);
	if(!hdr || !hdr->read)
		return -ENOTSUP;

	void *_fn = twz_object_lea(obj, hdr->read);
	if(!_fn)
		return -EGENERIC;
	ssize_t (*fn)(twzobj *, void *, size_t, size_t, unsigned) = _fn;
	return fn(obj, buf, len, off, flags);
}

ssize_t twzio_write(twzobj *obj, const void *buf, size_t len, size_t off, unsigned flags)
{
	struct twzio_hdr *hdr = twz_object_getext(obj, TWZIO_METAEXT_TAG);
	if(!hdr || !hdr->write)
		return -ENOTSUP;

	void *_fn = twz_object_lea(obj, hdr->write);
	if(!_fn)
		return -EGENERIC;
	ssize_t (*fn)(twzobj *, const void *, size_t, size_t, unsigned) = _fn;
	return fn(obj, buf, len, off, flags);
}

ssize_t twzio_ioctl(twzobj *obj, int req, ...)
{
	va_list vp;
	va_start(vp, req);
	long arg = va_arg(vp, long);
	va_end(vp);
	struct twzio_hdr *hdr = twz_object_getext(obj, TWZIO_METAEXT_TAG);
	if(!hdr || !hdr->ioctl)
		return -ENOTSUP;

	void *_fn = twz_object_lea(obj, hdr->ioctl);
	if(!_fn)
		return -EGENERIC;
	ssize_t (*fn)(twzobj *, int, long) = _fn;
	return fn(obj, req, arg);
}

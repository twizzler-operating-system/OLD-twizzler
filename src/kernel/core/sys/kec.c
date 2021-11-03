#include <kec.h>
#include <syscall.h>
long syscall_kec_write(const void *buffer, size_t len, int flags)
{
	if(!verify_user_pointer(buffer, len))
		return -EINVAL;
	kec_write(buffer, len, flags & KEC_ALLOWED_USER_WRITE_FLAGS);
	return 0;
}

ssize_t syscall_kec_read(void *buffer, size_t len, int flags)
{
	if(!verify_user_pointer(buffer, len))
		return -EINVAL;
	return kec_read(buffer, len, flags & KEC_ALLOWED_USER_READ_FLAGS);
}

Twizzler Kernel Documentation                       {#mainpage}
=============================

Basic Structure
---------------
Twizzler's kernel is a micro-kernel like system, intended to push much functionality into userspace.
It's not quite an exokernel, as it still abstracts core system resources (CPU into threads, memory
into objects, etc). The kernel's primary services are objects and threads, managing how objects are
mapped into address spaces for threads, which are scheduled to run on CPUs. The kernel is also
responsible for security and programming security hardware like the MMU and the IOMMU.

The basic operational path of the kernel is as followings:
```
STARTUP: arch-startup ---> kernel init ---> kernel main ---> scheduler (thread_resume)
INTERRUPT: arch-specific-handling ---> core/interrupt.c ---> dispatch to handlers -> scheduler
SYSCALL: arch-specific-handling ---> core/syscall.c ---> call syscall handler -> scheduler
```
see \ref kernelinit for details on kernel startup. Once the kernel has initialized, it calls into
the schedule which tries to run threads that are ready to run, and falls back to performing
background tasks (like zeroing pages) if no threads are ready. If no background tasks need doing, it
falls back to the architecture-specific idle loop (which probably puts CPUs to sleep).

The kernel is non-blocking, that is, there is no way to block in the middle of an operation inside
the kernel. If a thread needs to block, the kernel will set its state as blocked and then just
continue. Once the scheduler runs, it simply doesn't consider that thread as runnable. Any operation
where the kernel might need to wait is ultimately sourced from userspace, so blocking userspace
threads is necessary, but blocking the kernel itself is not. Operations can be retried if necessary.
For example, on paging caused by a VM fault, the kernel will see that an object is not in-memory,
block the current thread, tie the thread's wake-up to a paging request, and then fire off that
request. Thus the thread will resume when the request completes, and the operation will retry.

The kernel is interruptible. While it cannot block, it can receive interrupts in kernel mode. Code
that uses locks thus inhibits interrupts when inside a lock. If an interrupt occurs in kernel mode,
it always resumes to the thread that was running when the interrupts occurs (because the kernel
cannot block).

On x86, the kernel runs in guest-mode of VTx. This is because it uses two-level address translation
internally to manage security contexts. Thus, there is additional code that must handle managing the
virtual machine abstractions on x86, but this is largely invisible.

Basic Terms and Things
----------------------

<b>Objects</b> are the primary currency of Twizzler.
Memory is abstracted into objects which are flat, large (1GB currently) regions of memory whose
contents can be used for anything. While there is a much more structured use for objects in
userspace, the kernel understands very little of this.  Instead, it is primarily concerned with
mapping objects in `vm_context`s (see \ref include/vmm.h) according to `views` (see \ref ksos). The
kernel has some other operations it can perform on objects, like read and writing them itself, and
allowing threads to sleep on words inside objects (like futex(2)).

<b>Threads</b> are computational units. They are schedulable entities which run on a CPU in
userspace. There are no kernel-space threads. A thread has a `vm_context` that defines the current
virtual address space, a set of security contexts that it is attached to, and one security context
that is active.

<b>Security Contexts</b> are themselves objects (see \ref ksos) that define access rights to objects
for threads that are attached to this context. Security permissions are applied to object space.

<b>Object Space</b> is an address space where objects are mapped in 2MB (on x86) "object-space
regions". Virtual memory maps to this address space, which maps to physical memory. There are
multiple object spaces, one for each security context, but each object chunk is always in the same
place across different object spaces, making management easier. Additionally, this address space is
presented to devices through the IOMMU.



Kernel Initialization        {#kernelinit}
=====================

Kernel initialization is broken up into several phases, each of which has several steps.

1. Phase 1 -- architecture-specific init. Note that dynamic memory allocation is unavailable until
   the init code calls \ref mm_register_region with usable, physical memory, after which we can use the
   bootstrap allocator (\ref mm_early_alloc).
  1. assembly phase. Typically this will involve enough bootstrapping to run a C environment for the
	 rest of architecture-specific init.
  2. arch-specific init. Prep the rest of the architecture-specific stuff, like CPU control
	 registers, features, etc.
  3. init bootstrap memory allocator. This is done by calling \ref mm_register_region on memory
	 regions to tell the system where usable memory is.
  4. percpu-regions init. Copies percpu data into memory for each CPU.

2. Phase 2 -- kernel init. The bulk of the kernel is initialized here. This is handled in \ref
   kernel_init, which is called by the architecture-independent code at the end of phase 1.
  1. switch to main memory manager. Before this, we can only use the bootstrap memory manager (\ref
	 mm_early_alloc). After this step, full memory management is available.
  2. run global constructors. This runs any functions declared with \ref __initializer or
	 __orderedinitializer. See below for more details.
  3. start secondary processors. All the above steps are run only by the bootstrap processor. This
	 step will start up all the secondary processors. The entry point for secondary processors is
	 probably in architecture-specific code which will redo part of phase 1 (to get CPUs ready).

3. Phase 3 -- kernel late init. All processors execute this phase, and at this point, the kernel is
   basically "ready to go".
  1. load or create the system root object. This object forms the KSO tree root. After this, the KSO
	 system is ready. This is only done by the BSP.
  2. BARRIER -- all processors will wait here until all arrive.
  3. run post-init functions. These are functions registered with \ref POST_INIT or similar (see
	 \ref include/init.h). These functions are sometimes run by just one or all processors. See below
	 for details.
  4. BARRIER -- all processors will wait here until all arrive.
  5. create the init thread. This is only done by the BSP. It creates a view object, a stack object,
	 and loads the init ELF and copies that into code and data objects, and creates and starts a
	 thread.
  6. All done! Call into the scheduler to start the main scheduling loop.

Global Constructors
-------------------

The kernel offers a service for running functions during phase 2.2 without actually listing all the
functions to call, making everything much more modular and cleaner. To create such a function, you
would do:

```
__initializer static void run_this(void)
{
	...
}
```

which will then run during phase 2.2 automatically. This is used all over the place to initialize
various parts of the kernel.

The order of running functions declared with `__initializer` is undefined. However, if you need to
control order, the kernel provides a way to specify "after":

```
#define FOO_INIT_ORDER 50

__orderedinitializer(FOO_INIT_ORDER) static void foo(void) {}

__orderedinitializer(__orderedafter(FOO_INIT_ORDER)) static void bar(void) {}
```

this will ensure that `bar` is called after `foo`.

Post-init Functions (or late-init functions)
-------------------------------------------- 
These functions operate similar to global constructors, except they are run in phase 3.3. See \ref
include/init.h. Importantly, we know for sure that all global constructors have been run, and that
the KSO system is ready. So we might use these to create device objects and busses and things, since
these need to be attached using the KSO system.

These functions can also be specified to run on just the BSP or by all processors (\ref POST_INIT or
\ref POST_INIT_ALLCPUS). They also support the same ordering semantics as global constructors
(\ref POST_INIT_ORDERED).



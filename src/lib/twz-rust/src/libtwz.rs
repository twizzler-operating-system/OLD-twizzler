use crate::obj::*;

#[repr(C)]
pub(crate) union ObjOrWord {
		obj: *mut std::ffi::c_void,
		word: *const std::sync::atomic::AtomicU64,
}

#[repr(C)]
pub(crate) struct QueueMultiSpec {
	pub(crate) obj_or_word: ObjOrWord,
	pub(crate) result: *mut std::ffi::c_void,
	pub(crate) cmp: u64,
	pub(crate) sq: i32,
	pub(crate) ret: i32,
}

impl QueueMultiSpec {
    pub(crate) fn new_subm<S: Copy , C: Copy >(queue: &crate::queue::Queue<S, C>, result: &mut crate::queue::QueueEntry<S>) -> QueueMultiSpec {
        queue.obj().alloc_libtwz_data();
        QueueMultiSpec {
            obj_or_word: ObjOrWord { obj: queue.obj().libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void },
            result: unsafe { std::mem::transmute::<*mut crate::queue::QueueEntry<S>, *mut std::ffi::c_void>(result as *mut crate::queue::QueueEntry<S>) },
            cmp: 0,
            sq: 0,
            ret: 0,
        }
    }

    pub(crate) fn new_sleep(word: &std::sync::atomic::AtomicU64, cmp: u64) -> QueueMultiSpec {
        QueueMultiSpec {
            obj_or_word: ObjOrWord { word: word as *const std::sync::atomic::AtomicU64 },
            result: std::ptr::null_mut(),
            cmp: cmp,
            sq: 0,
            ret: 0,
        }
    }

    pub(crate) fn is_queue(&self) -> bool {
        self.result != std::ptr::null_mut()
    }

    pub(crate) fn new_cmpl<S: Copy , C: Copy >(queue: &crate::queue::Queue<S, C>, result: &mut crate::queue::QueueEntry<C>) -> QueueMultiSpec {
        queue.obj().alloc_libtwz_data();
        QueueMultiSpec {
            obj_or_word: ObjOrWord { obj: queue.obj().libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void },
            result: unsafe { std::mem::transmute::<*mut crate::queue::QueueEntry<C>, *mut std::ffi::c_void>(result as *mut crate::queue::QueueEntry<C>)} ,
            cmp: 0,
            sq: 1,
            ret: 0,
        }
    }

    pub(crate) fn new_empty() -> QueueMultiSpec {
        QueueMultiSpec {
            obj_or_word: ObjOrWord { obj: std::ptr::null_mut() },
            result: std::ptr::null_mut(),
            cmp: 0,
            sq: 0,
            ret: 0,
        }
    }
}

pub(crate) mod twz_c {
    pub(crate) type LibtwzObjID = u128;
#[allow(improper_ctypes)]
#[allow(dead_code)]
    extern "C" {
        pub(crate) fn twz_name_resolve(_obj: *mut std::ffi::c_void, name: *const i8, _fn: *const std::ffi::c_void, flags: i32, id: *mut LibtwzObjID) -> i32;
        pub(crate) fn twz_name_assign(id: LibtwzObjID, name: *const i8) -> i32;
        pub(crate) fn twz_object_init_name(data: &mut [i8; 1024], name: *const i8, flags: i32) -> i32;
        pub(crate) fn twz_object_base(data: &mut [i8; 1024]) -> *mut i8;
        pub(crate) fn twz_object_from_ptr_cpp(p: *const i8, data: *mut std::ffi::c_void);
        pub(crate) fn twz_view_allocate_slot(_obj: *mut std::ffi::c_void, id: LibtwzObjID, flags: u32) -> i64;
        pub(crate) fn twz_alloc(obj: *mut std::ffi::c_void,
                                len: usize,
                                owner: *mut *const std::ffi::c_void,
                                flags: u64,
                                ctor: extern fn(*mut std::ffi::c_void, *mut std::ffi::c_void),
                                data: *mut std::ffi::c_void) -> i32;

        pub(crate) fn twz_free(obj: *mut std::ffi::c_void,
                                p: *const std::ffi::c_void,
                                owner: *mut *const std::ffi::c_void,
                                flags: u64);
        pub(crate) fn twz_object_create(flags: i32, kuid: LibtwzObjID, src: LibtwzObjID, id: *mut LibtwzObjID) -> i32;
        pub(crate) fn twz_object_init_alloc(obj: *mut std::ffi::c_void, offset: usize) -> i32;
        pub(crate) fn twz_fault_set(fault: i32, cb: Option<extern fn(i32, *mut std::ffi::c_void, *mut std::ffi::c_void)>, data: *mut std::ffi::c_void) -> i32;
        pub(crate) fn twz_fault_set_upcall_entry(cb: Option<extern fn()>, dbl_cb: Option<extern fn()>);
		pub(crate) fn twz_thread_exit(code: i32) -> !;

        pub(crate) fn queue_submit(obj: *mut std::ffi::c_void, item: *const std::ffi::c_void, flags: i32) -> i32;
        pub(crate) fn queue_receive(obj: *mut std::ffi::c_void, item: *mut std::ffi::c_void, flags: i32) -> i32;
        pub(crate) fn queue_complete(obj: *mut std::ffi::c_void, item: *const std::ffi::c_void, flags: i32) -> i32;
        pub(crate) fn queue_get_finished(obj: *mut std::ffi::c_void, item: *mut std::ffi::c_void, flags: i32) -> i32;
        pub(crate) fn queue_init_hdr(obj: *mut std::ffi::c_void, sqlen: usize, sqstride: usize, cqlen: usize, cqstride: usize) -> i32;
        pub(crate) fn queue_dequeue_multiple(count: usize, specs: *mut std::ffi::c_void) -> i64;
        pub(crate) fn bstream_write(obj: *mut std::ffi::c_void,
            ptr: *const u8, len: usize, flags: u32) -> i64;
        pub(crate) fn bstream_read(obj: *mut std::ffi::c_void,
            ptr: *mut u8, len: usize, flags: u32) -> i64;
        pub(crate) fn bstream_obj_init(obj: *mut std::ffi::c_void, hdr: *mut std::ffi::c_void, nbits: u32) -> i32;
        pub(crate) fn __twz_ptr_store_guid(obj: *mut std::ffi::c_void, ptr: *mut u64, dest_obj: *mut std::ffi::c_void, p: u64, f: u64) -> i32;
        pub(crate) fn __twz_object_lea_foreign(obj: *mut std::ffi::c_void, ptr: u64, mask: i32) -> u64;
        pub(crate) fn twz_object_guid(obj: *mut std::ffi::c_void) -> u128;
    }
}

pub(crate) fn twz_object_guid(obj: &Twzobj) -> ObjID {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::twz_object_guid(
            obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void)
    }
}

pub(crate) fn ptr_load_foreign(obj: &Twzobj, ptr: u64) -> u64 {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::__twz_object_lea_foreign(
            obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            ptr, !0)
    }
}

pub(crate) fn ptr_store_guid(obj: &Twzobj, pptr: &mut u64, dst: u64, dest_obj: &Twzobj) -> i32 {
    unsafe {
        obj.alloc_libtwz_data();
        dest_obj.alloc_libtwz_data();
        twz_c::__twz_ptr_store_guid(
            obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            pptr as *mut u64,
            dest_obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            dst,
            0xc/*TODO*/)
    }
}

pub(crate) fn bstream_write(obj: &Twzobj, data: &[u8], flags: u32) -> i64 {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::bstream_write(
            obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            data.as_ptr(), data.len(), flags)
    }
}

pub(crate) fn bstream_read(obj: &Twzobj, data: &mut [u8], flags: u32) -> i64 {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::bstream_read(
            obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            data.as_mut_ptr(), data.len(), flags)
    }
}

pub(crate) fn bstream_init(obj: &Twzobj, nbits: u32) -> i32 {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::bstream_obj_init(
            obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            std::ptr::null_mut(), nbits)
    }
}
pub(crate) fn queue_init_hdr<S, C>(obj: &Twzobj, sqlen: usize, cqlen: usize) -> i32 {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::queue_init_hdr(
            obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            sqlen, std::mem::size_of::<S>(),
            cqlen, std::mem::size_of::<C>())
    }
}

pub(crate) fn queue_submit<T>(obj: &Twzobj, item: &T, flags: i32) -> i32 {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::queue_submit(
            obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            std::mem::transmute::<&T, *const std::ffi::c_void>(item), flags)
    }
}

pub(crate) fn queue_receive<T>(obj: &Twzobj, item: &mut T, flags: i32) -> i32 {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::queue_receive(
            obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            std::mem::transmute::<&mut T, *mut std::ffi::c_void>(item), flags)
    }
}

pub(crate) fn queue_multi_wait(count: usize, specs: &mut [QueueMultiSpec]) -> i64 {
    unsafe {
        twz_c::queue_dequeue_multiple(count, std::mem::transmute::<*mut QueueMultiSpec, *mut std::ffi::c_void>(specs.as_mut_ptr()))
    }
}

pub(crate) fn queue_complete<T>(obj: &Twzobj, item: &T, flags: i32) -> i32 {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::queue_complete(
            obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            std::mem::transmute::<&T, *const std::ffi::c_void>(item), flags)
    }
}

pub(crate) fn queue_get_completion<T>(obj: &Twzobj, item: &mut T, flags: i32) -> i32 {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::queue_get_finished(
            obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            std::mem::transmute::<&mut T, *mut std::ffi::c_void>(item), flags)
    }
}

pub(crate) fn twz_object_init_alloc(obj: &Twzobj, offset: usize) -> i32 {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::twz_object_init_alloc(obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void, offset)
    }
}

pub(crate) fn twz_object_alloc_move<T>(obj: &Twzobj, owner: &mut crate::ptr::Pptr<T>, flags: u64, ctor: extern fn(&mut T, &T), item: T) -> i32
{
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::twz_alloc(obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            std::mem::size_of::<T>(),
            std::mem::transmute::<&mut crate::ptr::Pptr<T>, *mut *const std::ffi::c_void>(owner),
            flags,
            std::mem::transmute::<extern fn(&mut T, &T), extern fn(*mut std::ffi::c_void, *mut std::ffi::c_void)>(ctor),
            std::mem::transmute::<&T, *mut std::ffi::c_void>(&item))
    }
}

pub(crate) fn twz_object_free<T>(obj: &Twzobj, owner: &mut u64, flags: u64) {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::twz_free(obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
        std::mem::transmute::<u64, *const std::ffi::c_void>(*owner),
        std::mem::transmute::<&mut u64, *mut *const std::ffi::c_void>(owner),
        flags)
    }
}

pub(crate) fn twz_object_alloc_slice_move<T: Copy>(obj: &Twzobj, owner: &mut u64, flags: u64, slice: &[T]) -> i32
{
    struct CopyData<F> {
        len: isize,
        ptr: *const F,
    }
    let copy = CopyData {
        len: slice.len() as isize,
        ptr: slice.as_ptr(),
    };
    extern fn ctor<F: Copy>(ptr: *mut F, copy: &CopyData<F>) {
        for i in 0..copy.len {
            unsafe {
                *(ptr.offset(i)) = *copy.ptr.offset(i);
            }
        }
    }
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::twz_alloc(obj.libtwz_data.lock().unwrap().as_mut().unwrap().data as *mut std::ffi::c_void,
            std::mem::size_of::<T>() * slice.len(),
            std::mem::transmute::<&mut u64, *mut *const std::ffi::c_void>(owner),
            flags,
            std::mem::transmute::<extern fn(*mut T, &CopyData<T>), extern fn(*mut std::ffi::c_void, *mut std::ffi::c_void)>(ctor::<T>),
            std::mem::transmute::<&CopyData<T>, *mut std::ffi::c_void>(&copy))
    }
}


pub(crate) fn twz_object_create(flags: i32, kuid: ObjID, src: ObjID) -> Result<ObjID, i32> {
    let mut oid: twz_c::LibtwzObjID = 0;
    let res = unsafe {
        twz_c::twz_object_create(flags, kuid, src, &mut oid)
    };
    if res < 0 {
        Err(res)
    } else {
        Ok(oid)
    }
}

pub(crate) fn libtwz_twzobj_data_size() -> usize {
    /* TODO */
    1024
}

pub(crate) fn libtwz_twzobj_align_size() -> usize {
    16
}

pub(crate) const VE_READ: u32 = 0x4;
pub(crate) const VE_WRITE: u32 = 0x8;


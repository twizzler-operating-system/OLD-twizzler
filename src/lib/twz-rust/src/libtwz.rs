use crate::obj::*;

pub(crate) mod twz_c {
    pub(crate) type LibtwzObjID = u128;
#[allow(improper_ctypes)]
#[allow(dead_code)]
    extern "C" {
        pub(crate) fn twz_name_resolve(_obj: *mut std::ffi::c_void, name: *const i8, _fn: *const std::ffi::c_void, flags: i32, id: *mut LibtwzObjID) -> i32;
        pub(crate) fn twz_name_assign(id: LibtwzObjID, name: *const i8) -> i32;
        pub(crate) fn twz_object_init_name(data: &mut [i8; 1024], name: *const i8, flags: i32) -> i32;
        pub(crate) fn twz_object_base(data: &mut [i8; 1024]) -> *mut i8;
        pub(crate) fn __twz_object_lea_foreign(data: &mut [i8; 1024], offset: u64, mask: u32) -> *mut i8;
        pub(crate) fn twz_object_from_ptr_cpp(p: *const i8, data: *mut std::ffi::c_void);
        pub(crate) fn twz_view_allocate_slot(_obj: *mut std::ffi::c_void, id: LibtwzObjID, flags: u32) -> i64;
        pub(crate) fn twz_alloc(obj: *mut std::ffi::c_void,
                                len: usize,
                                owner: *mut *const std::ffi::c_void,
                                flags: u64,
                                ctor: extern fn(*mut std::ffi::c_void, *mut std::ffi::c_void),
                                data: *mut std::ffi::c_void) -> i32;

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


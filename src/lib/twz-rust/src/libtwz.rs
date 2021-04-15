use crate::obj::*;

pub(crate) mod twz_c {
    pub(crate) type LibtwzObjID = u128;
#[allow(improper_ctypes)]
#[allow(dead_code)]
    extern "C" {
        pub(crate) fn twz_name_resolve(_obj: *mut std::ffi::c_void, name: *const i8, _fn: *const std::ffi::c_void, flags: i32, id: *mut LibtwzObjID) -> i32;
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
    }
}

pub(crate) fn twz_object_init_alloc(obj: &mut Twzobj, offset: usize) -> i32 {
    unsafe {
        obj.alloc_libtwz_data();
        twz_c::twz_object_init_alloc(obj.libtwz_data.as_mut().unwrap().data as *mut std::ffi::c_void, offset)
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


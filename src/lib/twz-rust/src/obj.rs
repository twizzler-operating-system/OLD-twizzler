use crate::ptr::*;
use crate::libtwz::*;
use crate::*;
pub const OBJ_MAX_SIZE: u64 = 1 << 30;
pub const OBJ_NULLPAGE_SIZE: u64 = 0x1000;
pub type ObjID = u128;

pub(crate) struct LibtwzData {
    pub(crate) data: *mut u8,
}

unsafe impl Send for LibtwzData{}

pub(crate) fn id_from_lohi(lo: u64, hi: u64) -> ObjID {
    lo as u128 | (hi as u128) << 64
}

pub fn objid_split(id: ObjID) -> (u64, u64) {
    ((id >> 64) as u64, (id & 0xffffffffffffffff) as u64)
}

pub fn objid_join(hi: i64, lo: i64) -> ObjID {
    u128::from(lo as u64) | (u128::from(hi as u64)) << 64
}

impl LibtwzData {
    fn new() -> LibtwzData {
        LibtwzData {
            data: unsafe { std::alloc::alloc(
                          std::alloc::Layout::from_size_align(
                              libtwz_twzobj_data_size(),
                              libtwz_twzobj_align_size()
                              ).expect("failed to calculate memory layout for libtwz::twzobj")) }
        }
    }
}

impl std::ops::Drop for LibtwzData {
    fn drop(&mut self) {
        unsafe {
            std::alloc::dealloc(self.data,
                                std::alloc::Layout::from_size_align(
                                    libtwz_twzobj_data_size(),
                                    libtwz_twzobj_align_size()
                                    ).expect("failed to calculate memory layout for libtwz::twzobj"));
        }
    }
}

pub struct Twzobj {
    slot: u64,
    id: ObjID,
    pub(crate) libtwz_data: std::sync::Arc<std::sync::Mutex<Option<LibtwzData>>>,
}

impl std::ops::Drop for Twzobj {
    /* TODO: release the slot */
    fn drop(&mut self) {

    }
}

pub struct Tx {
}

impl Twzobj {
    pub fn id(&self) -> ObjID {
        self.id
    }

    pub(crate) fn alloc_libtwz_data(&self) {
        let mut data = self.libtwz_data.lock().unwrap();
        if data.is_none() {
            *data = Some(LibtwzData::new());
            unsafe {
                libtwz::twz_c::twz_object_from_ptr_cpp(std::mem::transmute::<&i8, *const i8>(self.base_unchecked::<i8>()), data.as_mut().unwrap().data as *mut std::ffi::c_void);
            }
        }
    }

    pub fn init_guid(id: ObjID) -> Result<Twzobj, TwzErr> {
        let slot = unsafe {
            twz_c::twz_view_allocate_slot(std::ptr::null_mut(), id, VE_READ | VE_WRITE)
        };
        if slot < 0 {
            return Err(TwzErr::OutOfSlots);
        }
        Ok(Twzobj {
            slot: slot as u64,
            id: id,
            libtwz_data: std::sync::Arc::new(std::sync::Mutex::new(None))
        })
    }

    pub fn init_name(name: &str) -> Result<Twzobj, TwzErr> {
        let mut id = 0;
        let s = std::ffi::CString::new(name).unwrap();
        let res = unsafe { twz_c::twz_name_resolve(std::ptr::null_mut(), s.as_ptr(), std::ptr::null(), 0, &mut id) };
        if res < 0 {
            return Err(TwzErr::NameResolve(-res));
        }
        Twzobj::init_guid(id)
    }

    pub const TWZ_OBJ_CREATE_HASHDATA: i32 = 0x1;
    pub const TWZ_OBJ_CREATE_DFL_READ: i32 = 0x4;
    pub const TWZ_OBJ_CREATE_DFL_WRITE: i32 = 0x8;
    pub const TWZ_OBJ_CREATE_DFL_EXEC: i32 = 0x10;
    pub const TWZ_OBJ_CREATE_DFL_USE: i32 = 0x20;
    pub const TWZ_OBJ_CREATE_DFL_DEL: i32 = 0x40;

    pub fn create<T>(flags: i32, base: Option<T>, kuid: Option<Twzobj>, src: Option<Twzobj>) -> Result<Twzobj, TwzErr> {
        let res = libtwz::twz_object_create(flags, kuid.map_or(0, |o| o.id), src.map_or(0, |o| o.id));
        match res {
            Err(e) => return Err(TwzErr::OSError(-e)),
            Ok(id) => {
                let mut obj = Twzobj::init_guid(id)?;
                let mut alloc_offset = std::mem::size_of::<T>();
                alloc_offset = (alloc_offset + 0x1000) & !0x1000; //TODO calc this better
                let res = libtwz::twz_object_init_alloc(&mut obj, alloc_offset);
                if res < 0 {
                    return Err(TwzErr::OSError(-res));
                }
                if let Some(base) = base {
                    let obj_base = unsafe { obj.base_unchecked_mut::<T>() };
                    *obj_base = base;
                    persist::flush_lines(obj_base);
                    persist::pfence();
                }
                Ok(obj)
            }
        }
    }

    pub fn move_item<T: Copy, E>(&self, item: T, p: &mut Pptr<T>, _tx: Option<&mut Tx>) -> Result<(), crate::TxResultErr<E>> {
        extern fn do_the_move<T: Copy>(tgt: &mut T, src: &T) {
            println!("do_the_move: {:p}", tgt);
            *tgt = *src;
        }
        let res = libtwz::twz_object_alloc_move(self, p, 0, do_the_move, item);
        if res < 0 {
            Err(crate::TxResultErr::OSError(-res))
        } else {
            Ok(())
        }
    }

    pub fn transaction<F, T, E>(&self, func: F) -> Result<T, crate::TxResultErr<E>>
        where F: FnOnce(&mut Tx) -> Result<T, crate::TxResultErr<E>> {
            let mut tx = Tx {};
            func(&mut tx)
        }

    pub fn lea<T>(&self, p: &Pptr<T>) -> &T {
        if p.is_internal() {
            unsafe { self.offset_lea::<T>(p.p) }
        } else {
            let r = libtwz::ptr_load_foreign(self, p.p);
            unsafe {
                std::mem::transmute::<u64, &T>(r)
            }
        }
    }

    pub fn lea_mut<T>(&self, p: &Pptr<T>, _tx: &mut Tx) -> &mut T {
        if p.is_internal() {
            unsafe { self.offset_lea_mut::<T>(p.p) }
        } else {
            let r = libtwz::ptr_load_foreign(self, p.p);
            unsafe {
                std::mem::transmute::<u64, &mut T>(r)
            }
        }
    }

    pub unsafe fn base_unchecked_mut<T>(&self) -> &mut T {
        std::mem::transmute::<u64, &mut T>(self.slot * OBJ_MAX_SIZE + OBJ_NULLPAGE_SIZE)
    }

    pub unsafe fn base_unchecked<T>(&self) -> &T {
        std::mem::transmute::<u64, &T>(self.slot * OBJ_MAX_SIZE + OBJ_NULLPAGE_SIZE)
    }

    pub fn base<T>(&self) -> &T {
        unsafe { std::mem::transmute::<u64, &T>(self.slot * OBJ_MAX_SIZE + OBJ_NULLPAGE_SIZE) }
    }

    pub fn base_mut<T>(&self, _tx: &mut Tx) -> &mut T {
        unsafe { std::mem::transmute::<u64, &mut T>(self.slot * OBJ_MAX_SIZE + OBJ_NULLPAGE_SIZE) }
    }

    pub unsafe fn offset_lea_mut<T>(&self, offset: u64) -> &mut T {
        if offset < OBJ_MAX_SIZE {
            return std::mem::transmute::<u64, &mut T>(self.slot * OBJ_MAX_SIZE + offset);
        }
        panic!("")
    }

    pub unsafe fn offset_lea<T>(&self, offset: u64) -> &T {
        if offset < OBJ_MAX_SIZE {
            return std::mem::transmute::<u64, &T>(self.slot * OBJ_MAX_SIZE + offset);
        }
        panic!("")
    }

    pub unsafe fn store_ptr_unchecked<T>(&self, pptr: &mut Pptr<T>, dest: &T, dest_obj: &Twzobj) -> Result<(), TwzErr> {
        let mut p = unsafe { std::mem::transmute::<*const T, u64>(dest as *const T) };
        p = p & (OBJ_MAX_SIZE-1);
        let res = libtwz::ptr_store_guid(self, &mut pptr.p, p, dest_obj);
        if res < 0 {
            Err(TwzErr::OSError(-res))
        } else {
            Ok(())
        }
    }


}



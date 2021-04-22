use crate::thread::TwzThread;

impl TwzThread {
    pub fn myself() -> TwzThread {
        let mut gsbase: u64;
        unsafe {
            asm!(
                "rdgsbase {}",
                out(reg) gsbase);
        }
        gsbase += crate::obj::OBJ_NULLPAGE_SIZE;
        TwzThread {
            header: unsafe { std::mem::transmute::<u64, *mut crate::thread::ThreadRepr>(gsbase) },
        }
    }
}



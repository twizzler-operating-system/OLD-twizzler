use crate::ptr::Pptr;

pub struct Vec<T> {
    buf: Pptr<T>,
    len: usize,
    cap: usize,
}

impl<T> Vec<T> {
    pub const fn new() -> Self {
        Vec {
            buf: Pptr::new_null(),
            len: 0,
            cap: 0,
        }
    }
}

#[derive(Copy, Clone)]
pub struct Pptr<T> {
    pub(crate) p: u64,
    _pd: std::marker::PhantomData<T>,
}

impl<T> Pptr<T> {
    pub fn new_null() -> Pptr<T> {
        Pptr {
            p: 0,
            _pd: std::marker::PhantomData,
        }
    }
}

impl<T> std::fmt::Pointer for Pptr<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:x}", self.p)?;
        if f.alternate() {
            write!(f, "<{}>", std::any::type_name::<T>())
        } else {
            Ok(())
        }
    }
}

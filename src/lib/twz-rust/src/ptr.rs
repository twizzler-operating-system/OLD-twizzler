pub struct Pptr<T> {
    pub(crate) p: u64,
    _pd: std::marker::PhantomData<T>,
}

impl<T> Clone for Pptr<T> {
    fn clone(&self) -> Self {
        Pptr {
            p: self.p,
            _pd: std::marker::PhantomData,
        }
    }
}

impl<T> Copy for Pptr<T>{}

impl<T: ?Copy + Sized> Pptr<T> {
    pub fn new_null() -> Pptr<T> {
        Pptr {
            p: 0,
            _pd: std::marker::PhantomData,
        }
    }

    pub fn is_internal(&self) -> bool {
        self.p < crate::obj::OBJ_MAX_SIZE
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

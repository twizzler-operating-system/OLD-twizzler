use crate::obj::Twzobj;
use crate::TwzErr;

#[derive(Clone)]
pub struct Pslice<T> {
    len: usize,
    pub(crate) p: u64,
    _pd: std::marker::PhantomData<T>,
}

impl<T> Pslice<T> {
    pub fn lea_slice<'a>(&self, obj: &'a Twzobj) -> &'a [T] {
        unsafe {
            let start = obj.offset_lea::<T>(self.p);
            std::slice::from_raw_parts::<'a, T>(start as *const T, self.len)
        }
    }
    
    pub const fn empty() -> Pslice<T> {
        Pslice {
            len: 0,
            p: 0,
            _pd: std::marker::PhantomData,
        }
    }
}

impl<T: Copy> Copy for Pslice<T> {}

impl<T: Copy> Pslice<T> {
    pub fn from_slice(obj: &Twzobj, data: &[T]) -> Result<Pslice<T>, TwzErr> {
        let mut slice = Self::empty();
        obj.move_slice(data, &mut slice.p)?;
        slice.len = data.len();
        Ok(slice)
    }

    pub fn free_slice(mut self, obj: &Twzobj) {
        obj.free_slice(&mut self);
    }
}

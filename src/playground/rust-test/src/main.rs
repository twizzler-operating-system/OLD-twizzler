
use twz;

#[derive(Debug)]
#[repr(C)]
struct Foo {
    x: u32,
}

//struct Obj {
//}

struct Pptr<T> {
    p: u64,
    pd: std::marker::PhantomData<T>
}

impl<T> std::ops::Deref for Pptr<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        unsafe {
        std::mem::transmute::<&u64, &Self::Target>(&self.p)
        }
    }
}

fn main() {
    let f = Box::new(Foo { x: 0 });
    println!("Hello, world from Rust on Twizzler!");
    twz::twz_test();
    println!(":: {:?}", f);
}

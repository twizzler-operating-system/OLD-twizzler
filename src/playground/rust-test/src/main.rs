
use twz;

/*
#[derive(Debug)]
#[repr(C)]
struct Foo {
    x: u32,
    p: Pptr<u32>,
}

struct Obj<T> {
    base: *mut T,
}

impl<T> Obj<T> {
    pub fn base_unchecked(&self) -> &T {
        unsafe { &*self.base }
    }

    pub fn base(&self) -> &mut T {
        unsafe { &mut *self.base }
    }   
}

#[repr(C)]
#[derive(Debug)]
struct Pptr<T> {
    p: u64,
    pd: std::marker::PhantomData<T>
}

#[repr(C)]
#[derive(Debug)]
struct Psafe<T> {
    data: T,
    pd: std::marker::PhantomData<T>
}

impl<T> Pptr<T> {
    pub fn new_null() -> Pptr<T> {
        Pptr { p: 0, pd: std::marker::PhantomData }
    }
}

impl<T> std::ops::Deref for Pptr<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        unsafe {
            std::mem::transmute::<u64, &Self::Target>(self.p)
        }
    }
}

struct Tx {
}


extern "C" {
    fn twz_name_resolve(o: *mut i32, name: *const i8, p: u64, fl: i32, id: &mut u128) -> i32;
}

fn transaction<T, F>(obj: &Obj<T>, f: F)
    where F: FnOnce(Tx)
{

}

impl Tx {
    fn record<'a, T>(&'a self, v: &'a mut T) -> &'a mut T {
        v
    }
}

fn gadad(obj: &Obj<Foo>)
{
    {
        let base = obj.base();
        transaction(obj, |tx: Tx| {
            let ref_x = tx.record(&mut base.x);
            *ref_x = 42;
        });
    }
}


use std::ffi::CString;
*/








struct Foo {
    /* some data */
    x: u32,
    /* persistent pointer to another struct Foo */
    p: twz::ptr::Pptr<Foo>,
}

fn new_test()
{
    let foo = Foo {
        x: 42, p: twz::ptr::Pptr::new_null(),
    };
    let obj = twz::obj::Twzobj::create(twz::obj::Twzobj::TWZ_OBJ_CREATE_DFL_READ | twz::obj::Twzobj::TWZ_OBJ_CREATE_DFL_WRITE, Some(foo), None, None).expect("ha");

    let base = obj.base::<Foo>();
    println!(":: {}", base.x);
}

fn access_test() {
    /* get a handle to the object (by name) */
    let obj = twz::obj::Twzobj::init_name("some_object").unwrap();

    /* update some fields! */
    obj.transaction::<_, _, ()>(|tx| {
        let base = obj.base_mut::<Foo>(tx);
        let foo = obj.lea_mut(&base.p, tx);

        foo.x = 42;
        //base.x += 1;
        println!("{}", base.x);
        println!("{}", foo.x);
        Ok(())
    }).unwrap();

    /* we can also get some read-only references */
    let base = obj.base::<Foo>();
    let foo = obj.lea(&base.p);
    println!("foo.x = {}", foo.x);

    /* and we can even just get a pointer without checking transaction stuff */
    let base = unsafe { obj.base_unchecked::<Foo>() };
    println!("base.x = {}", base.x);
}

fn main()
{
    new_test();
}

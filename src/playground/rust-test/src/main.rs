#![feature(asm)]
#![feature(naked_functions)]

use twz;

#[allow(dead_code)]
#[derive(Copy, Clone)]
struct Foo {
    /* some data */
    x: u32,
    /* persistent pointer to another struct Foo */
    p: twz::ptr::Pptr<Foo>,
}

#[derive(Copy, Clone, Debug)]
struct Bar {
    x: i32,
    y: i32,
}

twz::twz_gate!(1, __foo, foo (x: i32) { println!("Hi {}", x); });
twz::twz_gate!(2, __bar, bar (x: i32) { println!("Hi {}", x); });

use twz::queue::*;
fn queue_test()
{
    /* create a queue (creates an object under the hood). The submission queue will send items of
     * type Bar, as will the completion queue. Note that we'll actually be getting QueueEntry<Bar>
     * back when we receive things, because of how the queues work. */
    let queue = twz::queue::Queue::<Bar, Bar>::new_private(8, 8).expect("failed to create queue");
    /* let's make a queue entry and pass it the actual item we want to transfer */
    let bar = QueueEntry::new(Bar { x: 42, y: 69 }, 0);
    /* aaand send! Run this call back when the completion event is processed */
    queue.send_callback(bar, 0, |c| { 
        println!("completed! {:?}", c); 
    }).expect("failed to send");
    /* (imagine these next two statements are a different program / thread) receive the sent item */
    let res = queue.recv(0).expect("queue recv failed");
    /* and do whatever with `res' and then complete it */
    queue.complete(res, 0).unwrap();
    /* meanwhile in the sender program, we'd at some point want to check for completions */
    queue.check_completions_callback(0).unwrap();



    //queue.send(bar, 0);
    //let res = queue.recv(0).unwrap();
    //println!("{:?}", res);
    //let res = queue.recv(0).unwrap();
    //println!("{:?}", res);
}

#[allow(dead_code)]
fn new_test()
{
    let foo = Foo {
        x: 42, p: twz::ptr::Pptr::new_null(),
    };
    let obj = twz::obj::Twzobj::create(
        twz::obj::Twzobj::TWZ_OBJ_CREATE_DFL_READ | twz::obj::Twzobj::TWZ_OBJ_CREATE_DFL_WRITE, 
        Some(foo), None, None).expect("failed to create object");

    let base = unsafe { obj.base_unchecked_mut::<Foo>() };
    println!(":: {}", base.x);

    let foo2 = Foo {
        x: 69,
        p: twz::ptr::Pptr::new_null(),
    };
    obj.move_item::<_, ()>(foo2, &mut base.p, None).unwrap();
    let _foo_m = obj.lea(&base.p);
}

#[allow(dead_code)]
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
    let handler = std::thread::spawn(|| {
        println!("Hello from thread!");
    });
    println!("Hello from parent!");
    handler.join().unwrap();
    println!("Thread joined");
    queue_test();
 //   new_test();
}

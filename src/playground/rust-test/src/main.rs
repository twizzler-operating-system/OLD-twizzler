#![feature(asm)]
#![feature(naked_functions)]

use twz;

#[allow(dead_code)]
#[derive(Copy, Clone, Debug)]
struct Bar {
    x: i32,
    y: i32,
}

twz::twz_gate!(1, __foo, foo (a: i32, b: i32, c: i32, d: i32, e: i32, f: i32) {
    println!("Hi {} {} {} {} {} {}", a, b, c, d, e, f);
    twz::sapi_return!(0, 91, 92, 93, 94);
});

twz::twz_gate!(2, __bar, bar () { println!("Hi"); 0});

use twz::queue::*;

#[derive(Copy, Clone)]
#[repr(C)]
enum LogEventCmd {
    LogMsg = 0,
    Disconnect = 1,
    SetName = 2,
}

#[derive(Copy, Clone)]
#[repr(C)]
struct LogEvent {
    cmd: LogEventCmd,
    ptr: twz::ptr::Pptr<[u8; 4096]>,
    len: usize,
}

fn logboi_test()
{
    let obj = twz::obj::Twzobj::init_name("logboi").unwrap();
    let sapi = twz::gate::SecureApi::from_obj(obj);
    let rets = sapi.call(0, Some(&vec![0])).unwrap();
    let id = twz::obj::objid_join(rets[0], rets[1]);
    println!("GOT: {:x}", id);
    let obj = twz::obj::Twzobj::init_guid(id).expect("failed to open object");
    let queue: twz::queue::Queue<LogEvent, LogEvent> = twz::queue::Queue::from_obj(obj);


    let buffer = twz::obj::Twzobj::create::<()>(
        twz::obj::Twzobj::TWZ_OBJ_CREATE_DFL_READ | twz::obj::Twzobj::TWZ_OBJ_CREATE_DFL_WRITE, 
        None, None, None).expect("failed to create object");
    let buf_base = unsafe { buffer.base_unchecked_mut::<[u8; 4096]>() };
    buf_base[0] = 'h' as u8;
    buf_base[1] = 'e' as u8;
    buf_base[2] = 'l' as u8;
    buf_base[3] = 'l' as u8;
    buf_base[4] = 'o' as u8;
    buf_base[5] = '\0' as u8;
    let buf_base = unsafe { buffer.base_unchecked_mut::<[u8; 4096]>() };

    let mut qe = QueueEntry::new(LogEvent { cmd: LogEventCmd::LogMsg, ptr: twz::ptr::Pptr::new_null(), len: 6 }, 0);

    unsafe {
        queue.obj().store_ptr_unchecked(&mut qe.item_mut().ptr, buf_base, &buffer).unwrap();
        println!(":::: {:p}", qe.item().ptr);
    }

    queue.send_callback(qe, 0, |c| {
        println!("req done");
    }).unwrap();
    queue.check_completions_callback(0).unwrap();
    //stream.write(b"hello from bstream", 0).expect("Failed to log");
}

#[allow(dead_code)]
fn queue_test()
{
    let args = std::env::args();
    if args.len() == 1 {
        println!("SETTING UP SAPI");
        twz::sapi::sapi_create_name("rust-gate-test").expect("failed to init sapi");
    } else {
        println!("CALLING SAPI");
        let obj = twz::obj::Twzobj::init_name("rust-gate-test").unwrap();
        let sapi = twz::gate::SecureApi::from_obj(obj);
        let rets = sapi.call(1, Some(&vec![11, 22, 33, 44, 55, 66])).unwrap();
        println!("GOT: {:?}", rets);
    }

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
#[repr(C)]
#[derive(Copy, Clone)]
struct Foo {
    /* some data */
    x: u32,
    /* persistent pointer to another struct Foo */
    p: twz::ptr::Pptr<Foo>,
}

#[allow(dead_code)]
fn access_test() {
    /* get a handle to the object (by name) */
    let obj = twz::obj::Twzobj::init_name("some_object").expect("failed to open object");

    /* update some fields! */
    obj.transaction::<_, _, ()>(|tx| {
        let base = obj.base_mut::<Foo>(tx);
        let foo = obj.lea_mut(&base.p, tx);

        foo.x = 42;

        Ok(())
    }).expect("transaction failed");

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
    logboi_test();
 //   queue_test();
  //  let ten_millis = std::time::Duration::from_millis(1000);
    //loop {
      //  std::thread::sleep(ten_millis);
        //println!(".");
    //}
    
 //   new_test();
}

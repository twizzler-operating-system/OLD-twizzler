#![feature(asm)]
#![feature(naked_functions)]
#![feature(once_cell)] 

use twz;
use twz::queue::Queue;
use std::{lazy::SyncLazy, sync::Mutex};

const MAX_MSG_SIZE: usize = 4096;

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
    ptr: twz::ptr::Pptr<[u8; MAX_MSG_SIZE]>,
    len: usize,
}

struct Client {
    queue: Queue<LogEvent, LogEvent>,
    reprobj: twz::obj::Twzobj,
}

static CLIENTS: SyncLazy<Mutex<Vec<Client>>> = SyncLazy::new(|| Mutex::new(vec![]));
static BELL: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(0);

twz::twz_gate!(1, __logboi_open, logboi_open (flags: i32) {
    let queue = twz::queue::Queue::new_private(8, 8).expect("Failed to create queue");
    println!("creating object {:x}", queue.obj().id());
    let (hi, lo) = twz::obj::objid_split(queue.obj().id());
    CLIENTS.lock().unwrap().push(Client {
        queue: queue,
        reprobj: twz::thread::TwzThread::myself().repr_obj().unwrap()
    });
    BELL.fetch_add(1, std::sync::atomic::Ordering::SeqCst);
    unsafe {
        twz::sys::thread_sync(&mut [twz::sys::ThreadSyncArgs::new_wake(&BELL, u64::MAX)], None);
    }
    twz::sapi_return!(0, hi, lo, 0, 0);
});

fn init_sapi() {
    twz::sapi::sapi_create_name("logboi").expect("failed to init sapi");
}

fn main() {
    init_sapi();

    let mut clients = vec![];
    loop {
        let bell_val = BELL.load(std::sync::atomic::Ordering::SeqCst);
        {
            let mut new_clients = CLIENTS.lock().unwrap();
            while new_clients.len() > 0 {
                clients.push(new_clients.pop().unwrap());
                println!("Added new client {}", clients.len()-1);
            }
        }

        let mut v = vec![];
        let mut s = vec![];
        {
            for client in clients.iter() {
                v.push( (twz::queue::Direction::Submission, &client.queue) );
                s.push( (client.reprobj.base::<twz::thread::ThreadRepr>().syncs(twz::thread::SYNC_EXIT), 0) );
            }
        }
        s.push( (&BELL, bell_val) );
        let res = twz::queue::wait_multiple(v, s).expect("A");

        for (i, queue_event) in res.queue_results {
         //   println!("client {} has queue event: {:?}", i, queue_event);
            match queue_event {
                twz::queue::MultiResultState::ReadyS(s) => {
                    let buf = clients[i].queue.obj().lea(&s.item().ptr);
                    println!("{}", String::from_utf8(buf.split_at(s.item().len).0.to_vec()).unwrap());
                    clients[i].queue.complete(s, 0);
                },
                twz::queue::MultiResultState::Error(s) => println!("::: {}", s),
                _ => {},
            }
        }
        for (i, sleep_event) in res.sleep_results {
            //println!("client {} has sleep event: {:?}", i, sleep_event);
            if i < clients.len() {
                println!("removed client {}", i);
                clients.remove(i);
            }
        }
    }

    /*
    let ten_millis = std::time::Duration::from_millis(1000);
    loop {
        std::thread::sleep(ten_millis);
        for client in CLIENTS.lock().unwrap().iter() {
            let qe = client.queue.recv(0).unwrap();
            println!("::: GOT LOGEVENT {}", qe.item().x);
            //let mut buffer = [0; 1024];
            /*
            let res = client.stream.read(&mut buffer, twz::bstream::NONBLOCK);
            match res {
                Err(twz::TwzErr::OSError(11)) => {},
                Err(e) => println!("error {:?}", e),
                Ok(len) => println!("read {} bytes from {:x}: {:?}", len, client.reprobj.id(), std::str::from_utf8(buffer.split_at(len).0).unwrap()),
            }
            */
        }
    }*/
}

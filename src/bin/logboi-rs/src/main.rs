#![feature(asm)]
#![feature(naked_functions)]
#![feature(once_cell)] 

use twz;
use twz::bstream::BStream;

use std::{lazy::SyncLazy, sync::Mutex};

static STREAMS: SyncLazy<Mutex<Vec<BStream>>> = SyncLazy::new(|| Mutex::new(vec![]));



twz::twz_gate!(1, __logboi_open, logboi_open (flags: i32) {
    let stream = twz::bstream::BStream::new_private(12).expect("Failed to create stream");
    println!("creating object {:x}", stream.obj().id());
    let (hi, lo) = twz::obj::objid_split(stream.obj().id());
    STREAMS.lock().unwrap().push(stream);
    twz::sapi_return!(0, hi, lo, 0, 0);
});


fn init_sapi() {
    twz::sapi::sapi_create_name("logboi").expect("failed to init sapi");
}



fn main() {
    println!("Hello, world!");
    init_sapi();
    let ten_millis = std::time::Duration::from_millis(1000);
    loop {
        std::thread::sleep(ten_millis);
        for stream in STREAMS.lock().unwrap().iter() {
            let mut buffer = [0; 1024];
            let res = stream.read(&mut buffer, twz::bstream::NONBLOCK);
            match res {
                Err(twz::TwzErr::OSError(11)) => {},
                Err(e) => println!("error {:?}", e),
                Ok(len) => println!("read {} bytes: {:?}", len, std::str::from_utf8(buffer.split_at(len).0).unwrap()),
            }
        }
    }
}

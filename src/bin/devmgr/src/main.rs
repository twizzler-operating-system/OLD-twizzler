#![feature(asm)]
#![feature(naked_functions)]
#![feature(once_cell)] 

use twz;
//use std::{lazy::SyncLazy, sync::Mutex};

/*twz::twz_gate!(1, __logboi_open, logboi_open (flags: i32) {
    twz::sapi_return!(0, 0, 0, 0, 0);
});*/

fn get_root_kso() -> twz::obj::Twzobj {
    twz::obj::Twzobj::init_guid(twz::kso::KSO_ROOT_ID).expect("failed to open KSO root")
}

fn main() {
    println!("Hello!");

    let root = get_root_kso();
    let roothdr = root.base::<twz::kso::KSORootHdr>();

    for c in roothdr {
        println!("{:?}", c);
    }

    //twz::sapi::sapi_create_name("devmgr").expect("failed to init sapi");
}

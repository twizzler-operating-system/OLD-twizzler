#![feature(asm)]
#![feature(naked_functions)]
#![feature(once_cell)] 

use twz;
//use std::{lazy::SyncLazy, sync::Mutex};

/*twz::twz_gate!(1, __logboi_open, logboi_open (flags: i32) {
    twz::sapi_return!(0, 0, 0, 0, 0);
});*/

mod busses;
mod bus;

fn get_root_kso() -> twz::obj::Twzobj {
    twz::obj::Twzobj::init_guid(twz::kso::KSO_ROOT_ID).expect("failed to open KSO root")
}

use std::convert::TryInto;
fn main() {
    println!("Hello!");

    let root = twz::kso::get_root().unwrap();
    
    let subtree = root.get_subtree(twz::kso::KSOType::Device).unwrap();
    let dir = subtree.get_dir().unwrap();
    println!("{}", dir.len());
    for c in dir {
        let kso: twz::kso::KSO = c.try_into().unwrap();
        println!("{:?} :: {}", c, kso.name());
        let mut dev = kso.into_device();
        for dc in dev.get_children() {
            let kso: twz::kso::KSO = dc.try_into().unwrap();
            println!("   {:?} :: {}", dc, kso.name());
        }

        
        let res = dev.get_child_mmio::<u8>(0);
        println!("{}", res.is_ok());
        if let Ok((h, m)) = res {
            println!(":: {:p} {:p} :: {:x}", h, m, h.length);
        }
        

        //1let chobj: twz::obj::Twzobj = c.try_into().unwrap();
    }

    //twz::sapi::sapi_create_name("devmgr").expect("failed to init sapi");
}

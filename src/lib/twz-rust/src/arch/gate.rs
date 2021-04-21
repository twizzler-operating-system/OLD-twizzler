use crate::gate::SecureApi;
const GATE_SIZE: usize = 64;
const GATE_OFFSET: usize = 0x1400;

fn gate_call_rip(gate: i32) -> u64 {
    gate as u64 * GATE_SIZE as u64 + GATE_OFFSET as u64
}

impl SecureApi {
    pub fn call(&self, gate: i32, args: Option<&[i64]>) -> Result<[i64; 4], crate::TwzErr> {
        let mut become_args = crate::sys::BecomeArgs {                                                            
            target_view: self.view,
            target_rip: gate_call_rip(gate),
            sctx_hint: self.sctx,
            ..Default::default()
		};
        if let Some(args) = args {
        for (i, arg) in args.iter().enumerate() {
            match i {
                0 => become_args.rdi = *arg,
                1 => become_args.rsi = *arg,
                2 => become_args.rdx = *arg,
                3 => become_args.r10 = *arg,
                4 => become_args.r8 = *arg,
                5 => become_args.r9 = *arg,
                _ => return Err(crate::TwzErr::Invalid),
            }
        }
        }
        let res = unsafe { crate::sys::attach(0, self.sctx, 0, 2 /* TODO */) };
        if res < 0 {
            return Err(crate::TwzErr::OSError(-res as i32));
        }
        let res = unsafe { crate::sys::r#become(&become_args as *const crate::sys::BecomeArgs, 0, 0) };
        match res {
            Err(res) => Err(crate::TwzErr::OSError(-res as i32)),
            Ok(res) => Ok( [res.rdi, res.rsi, res.rdx, res.r10] )
        }
    }
}

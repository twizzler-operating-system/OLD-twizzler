
const GATE_SIZE: usize = 64;
const GATE_OFFSET: usize = 0x1400;

#[repr(C)]
pub struct SecureApiHeader {
	sctx: crate::obj::ObjID,
	view: crate::obj::ObjID,
}

pub struct SecureApi {
    obj: crate::obj::Twzobj,
    view: crate::obj::ObjID,
    sctx: crate::obj::ObjID,
}

fn gate_call_rip(gate: i32) -> u64 {
    gate as u64 * GATE_SIZE as u64 + GATE_OFFSET as u64
}

impl SecureApi {
    pub fn from_obj(obj: crate::obj::Twzobj) -> SecureApi {
        let (sctx, view) = {
            let base = unsafe { obj.base_unchecked::<SecureApiHeader>() };
            (base.sctx, base.view)
        };
        SecureApi {
            obj: obj,
            sctx: sctx,
            view: view,
        }
    }

    pub fn call(&self, gate: i32) -> Result<i64, crate::TwzErr> {
        let args = crate::sys::BecomeArgs {                                                            
            target_view: self.view,
            target_rip: gate_call_rip(gate),
            sctx_hint: self.sctx,
            ..Default::default()
		};
        let res = unsafe { crate::sys::attach(0, self.sctx, 0, 2 /* TODO */) };
        if res < 0 {
            return Err(crate::TwzErr::OSError(res as i32));
        }
        Ok(unsafe{crate::sys::r#become(&args as *const crate::sys::BecomeArgs, 0, 0)})
    }
}

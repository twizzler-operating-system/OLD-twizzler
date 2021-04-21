#[repr(C)]
pub struct SecureApiHeader {
	pub(crate) sctx: crate::obj::ObjID,
	pub(crate) view: crate::obj::ObjID,
}

pub struct SecureApi {
    obj: crate::obj::Twzobj,
    pub(crate) view: crate::obj::ObjID,
    pub(crate) sctx: crate::obj::ObjID,
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

    pub fn get_obj(&self) -> &crate::obj::Twzobj {
        &self.obj
    }
}

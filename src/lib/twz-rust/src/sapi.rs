pub fn sapi_create_name(name: &str) -> Result<crate::obj::Twzobj, crate::TwzErr> {
    crate::arch::sapi::init();
    /* TODO: no more dfl write */
    let id = crate::libtwz::twz_object_create(crate::obj::Twzobj::TWZ_OBJ_CREATE_DFL_READ | crate::obj::Twzobj::TWZ_OBJ_CREATE_DFL_WRITE, 0, 0).map_err(|e| crate::TwzErr::OSError(e))?;
    let obj = crate::obj::Twzobj::init_guid(id)?;
    let res = crate::arch::sapi::wrap_twz_secure_api_create(&obj, name);
    if res < 0 {
        return Err(crate::TwzErr::OSError(-res));
    }
    let s = std::ffi::CString::new(name).unwrap();
    let res = unsafe { crate::libtwz::twz_c::twz_name_assign(obj.id(), s.as_ptr()) };
    if res < 0 {
        return Err(crate::TwzErr::OSError(-res));
    }
    Ok(obj)
}



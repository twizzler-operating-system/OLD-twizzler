
use crate::obj::Twzobj;
use crate::libtwz;
use crate::TwzErr;

pub struct BStream {
    obj: Twzobj,
}

fn create(flags: i32, nbits: u32) -> Result<Twzobj, TwzErr> {
    let id = libtwz::twz_object_create(flags, 0, 0).map_err(|e| TwzErr::OSError(e))?;
    let obj = Twzobj::init_guid(id)?;
    let res = libtwz::bstream_init(&obj, nbits);
    if res < 0 {
        Err(TwzErr::OSError(res))
    } else {
        Ok(obj)
    }
}

pub const NONBLOCK: u32 = 1;

impl BStream {
    pub fn from_obj(obj: Twzobj) -> BStream {
        BStream {
            obj: obj,
        }
    }

    pub fn obj(&self) -> &Twzobj {
        &self.obj
    }

    pub fn new_private(nbits: u32) -> Result<BStream, TwzErr> {
        Ok(BStream {
            obj: create(crate::obj::Twzobj::TWZ_OBJ_CREATE_DFL_READ | crate::obj::Twzobj::TWZ_OBJ_CREATE_DFL_WRITE, nbits)?,
        })
    }

    pub fn write(&self, data: &[u8], flags: u32) -> Result<usize, TwzErr> {
        let res = libtwz::bstream_write(&self.obj, data, flags);
        if res < 0 {
            return Err(TwzErr::OSError(-res as i32));
        }
        Ok(res as usize)
    }

    pub fn read(&self, data: &mut [u8], flags: u32) -> Result<usize, TwzErr> {
        let res = libtwz::bstream_read(&self.obj, data, flags);
        if res < 0 {
            return Err(TwzErr::OSError(-res as i32));
        }
        Ok(res as usize)
    }
}

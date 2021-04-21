use crate::gate::SecureApi;
use crate::obj::Twzobj;
use crate::obj::id_from_lohi;
pub struct Logboi {
    sapi: SecureApi,
    logobj: Option<Twzobj>,
}

impl Logboi {
    pub fn from_sapi(sapi: SecureApi) -> Logboi {
        Logboi {
            sapi: sapi,
            logobj: None,
        }
    }

    pub fn open(&mut self) -> Result<(), crate::TwzErr> {
        let res = self.sapi.call(0, None)?;
        self.logobj = Some(
            Twzobj::init_guid(id_from_lohi(res[0] as u64, res[1] as u64))?
        );
        Ok(())
    }
}

use crate::kso::{KSOAttachIterator,KSOHdr,KSODirAttachments,KSO,KSOType};
use crate::obj::Twzobj;
use crate::TwzErr;
#[repr(C)]
pub struct DeviceInterrupt {
    local: u16,
    vector: u16,
    flags: u32,
    sync: std::sync::atomic::AtomicU64,
}

pub const MAX_DEVICE_SYNCS: usize = 2;
pub const MAX_DEVICE_INTERRUPTS: usize = 32;

pub const DEVICE_CHILD_DEVICE: u64 = 0;
pub const DEVICE_CHILD_MMIO: u64 = 1;
pub const DEVICE_CHILD_INFO: u64 = 2;

#[repr(C)]
pub struct KSODevice {
    pub hdr: KSOHdr,
    pub bustype: u64,
    pub devtype: u64,
    pub devid: u64,
    pub syncs: [std::sync::atomic::AtomicU64; MAX_DEVICE_SYNCS],
    pub interrupts: [DeviceInterrupt; MAX_DEVICE_INTERRUPTS],
    pub attached: KSODirAttachments,
}

pub struct Device {
    kso: KSO,
    infos: Vec<Option<KSO>>,
    mmios: Vec<Option<KSO>>,
}

#[repr(C)]
pub struct DeviceMMIOHdr {
    hdr: KSOHdr,
    pub info: u64,
    pub flags: u64,
    pub length: u64,
    pub resv: u64,
}

impl KSO {
    pub fn into_device(self) -> Device {
        Device {
            kso: self,
            infos: vec![],
            mmios: vec![],
        }
    }
}

use std::convert::TryFrom;
impl Device {
    pub fn get_children<'a>(&'a self) -> KSOAttachIterator<'a> {
        self.kso.get_dir().unwrap().into_iter()
    }

    pub fn get_mmio_child_obj<'a>(&'a mut self, idx: usize) -> Result<&'a KSO, TwzErr> {
        if idx >= self.mmios.len() {
            self.mmios.resize(idx + 1, None);
        }
        if self.mmios[idx].is_none() {
            let mut count = 0;
            let mut obj: Option<KSO> = None;
            for child in self.get_children() {
                if child.id != 0 && (child.info & 0xffffffff) == DEVICE_CHILD_MMIO && child.attype == KSOType::Data as u32 {
                    if count == idx {
                        obj = Some(KSO::try_from(child)?);
                    }
                }
            }
            self.mmios[idx] = obj;
        }
        if let Some(ref obj) = self.mmios[idx] {
            Ok(obj)
        } else {
            Err(TwzErr::Invalid)
        }
    }
}

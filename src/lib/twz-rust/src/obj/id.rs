use super::Twzobj;
use std::sync::Arc;

pub type ObjID = u128;

pub fn objid_from_parts(upper: u64, lower: u64) -> ObjID {
	((upper as ObjID) << 64) | lower as ObjID
}

pub fn objid_parse(s: &str) -> Option<ObjID> {
	let mut s = s.trim();
	let radix = 16;
	if s.contains(":") {
		/* parse upper and lower separately */
		let mut split = s.split(":");
		let upper = split.next()?.trim_start_matches("0x");
		let lower = split.next()?.trim_start_matches("0x");
		let upper_num = u64::from_str_radix(upper, radix).ok()?;
		let lower_num = u64::from_str_radix(lower, radix).ok()?;
		Some(objid_from_parts(upper_num, lower_num))
	} else {
		if s.contains("0x") {
			s = s.trim_start_matches("0x");
		}
		ObjID::from_str_radix(s, radix).ok()
	}
}

impl<T> Twzobj<T> {
	pub fn id(&self) -> ObjID {
		if self.internal.id == 0 {
			todo!();
		}
		self.internal.id
	}

	pub(crate) fn set_id(&mut self, id: ObjID) {
		Arc::get_mut(&mut self.internal).unwrap().id = id;
	}
}

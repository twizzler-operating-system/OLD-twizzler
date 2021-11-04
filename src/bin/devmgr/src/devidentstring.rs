use std::fmt::Write;

impl crate::devtree::DeviceIdent {
	pub fn human_readable_string(&self) -> String {
		let mut s = String::new();
		write!(
			&mut s,
			"{:>8?} :: {:4x} {:4x} :: {:4x} {:4x}",
			self.bustype, self.class, self.subclass, self.vendor_id, self.device_id
		);
		s
	}
}

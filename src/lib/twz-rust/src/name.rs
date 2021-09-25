use crate::obj::id::ObjID;
use crate::obj::{GTwzobj, ProtFlags, Twzobj};

crate::bitflags! {
	pub struct NameResolveFlags: u32 {
		const FOLLOW_SYMLINKS = 0x1;
		const READLINK = 0x2;
	}
}

#[derive(Debug, Copy, Clone)]
pub enum NameError {
	NotFound,
	Loop,
	NotDir,
}

#[derive(Debug, Copy, Clone)]
#[repr(u8)]
enum NameEntType {
	Regular,
	Namespace,
	Symlink,
}

const NAME_ENT_VALID: u32 = 0x1;

const NAMESPACE_MAGIC: u64 = 0xa13a1300bbbbcccc;

#[derive(Debug, Copy, Clone)]
#[repr(C, packed)]
pub(crate) struct NameEnt {
	id: ObjID,
	flags: u32,
	resv0: u16,
	ent_type: u8,
	resv1: u8,
	dlen: u64,
}

impl NameEnt {
	fn new(id: ObjID, ty: NameEntType) -> NameEnt {
		NameEnt {
			id,
			flags: 0,
			resv0: 0,
			ent_type: ty as u8,
			resv1: 0,
			dlen: 0,
		}
	}

	fn name(&self) -> &[u8] {
		let name_bytes = unsafe { crate::flexarray::flexarray_get_array_start::<Self, u8>(self) };
		let slice = unsafe { core::slice::from_raw_parts(name_bytes, self.dlen as usize) };
		let name_len = slice.iter().position(|x| *x == 0).unwrap_or(self.dlen as usize);
		&slice[0..name_len]
	}

	fn symtarget(&self) -> &[u8] {
		let name_bytes = unsafe { crate::flexarray::flexarray_get_array_start::<Self, u8>(self) };
		let slice = unsafe { core::slice::from_raw_parts(name_bytes, self.dlen as usize) };
		let name_len = slice.iter().position(|x| *x == 0).unwrap_or(self.dlen as usize);
		let slice = &slice[(name_len + 1)..self.dlen as usize];
		let sym_len = slice
			.iter()
			.position(|x| *x == 0)
			.unwrap_or(self.dlen as usize - (name_len + 1));
		&slice[0..sym_len]
	}

	fn next(&self) -> Option<&Self> {
		unsafe {
			let this = self as *const Self;
			let offset = self.dlen as usize + std::mem::size_of::<Self>();
			let offset = (offset + 15) & !15;
			let ret = this
				.cast::<u8>()
				.offset(offset as isize)
				.cast::<Self>()
				.as_ref()
				.unwrap();
			if ret.dlen == 0 {
				None
			} else {
				Some(ret)
			}
		}
	}
}

#[derive(Debug, Copy, Clone)]
#[repr(C, packed)]
pub(crate) struct NamespaceHdr {
	magic: u64,
	version: u32,
	flags: u32,
}

impl NamespaceHdr {
	fn get_first_ent(&self) -> Option<&NameEnt> {
		let entry = unsafe { crate::flexarray::flexarray_get_array_start::<Self, NameEnt>(self) };
		let ret = unsafe { entry.as_ref().unwrap() };
		if ret.dlen == 0 {
			None
		} else {
			Some(ret)
		}
	}
}

pub(crate) type NameResult = Result<(Vec<u8>, NameEnt), NameError>;

fn lookup_entry<'a>(nhdr: &'a NamespaceHdr, element: &[u8]) -> Result<&'a NameEnt, NameError> {
	if nhdr.magic != NAMESPACE_MAGIC {
		return Err(NameError::NotDir);
	}

	let mut entry = nhdr.get_first_ent();
	while entry.is_some() {
		let e = entry.unwrap();
		let name = e.name();
		fn compare(a: &[u8], b: &[u8]) -> bool {
			println!(
				"{:?} {:?} {} {}",
				std::str::from_utf8(a),
				std::str::from_utf8(b),
				a.len(),
				b.len()
			);
			if a.len() == b.len() {
				for (ai, bi) in a.iter().zip(b.iter()) {
					if *ai != *bi {
						return false;
					}
				}
				return true;
			}
			false
		}
		if compare(element, name) {
			return Ok(e);
		}

		entry = e.next();
	}

	Err(NameError::NotFound)
}

pub(crate) fn hier_resolve_name(
	root: &Twzobj<NamespaceHdr>,
	cwd: &Twzobj<NamespaceHdr>,
	mut path: &[u8],
	flags: NameResolveFlags,
) -> NameResult {
	/* trim off any leading '/'s, and choose which starting point */
	let (mut start, mut path) = {
		let mut found = false;
		while path.len() > 0 && path[0] == b'/' {
			found = true;
			path = &path[1..];
		}
		if found {
			(Twzobj::clone(root), path)
		} else {
			(Twzobj::clone(cwd), path)
		}
	};

	while path.len() > 0 && path[path.len() - 1] == b'/' {
		path = &path[0..(path.len() - 1)];
	}

	if path.len() == 0 {
		return Ok((vec![b'/'], NameEnt::new(start.id(), NameEntType::Namespace)));
	}

	let elements: Vec<&[u8]> = path.split(|x| *x == b'/').collect();
	let mut next: Option<Twzobj<NamespaceHdr>> = Some(start);
	let mut ret_name_ent = None;
	for i in 0..elements.len() {
		let last = i == elements.len() - 1;
		let first = i == 0;
		let element = elements[i];
		println!("ELEMENT: `{}' {}", std::str::from_utf8(element).unwrap(), element.len());
		if element.len() == 0 {
			continue;
		}

		let nobj = next.ok_or(NameError::NotDir)?;

		let nhdr = nobj.base();
		let result: &NameEnt = lookup_entry(&*nhdr, element)?;

		if last {
			ret_name_ent = Some((result.name().to_vec(), *result));
		}

		match result.ent_type {
			x if x == NameEntType::Namespace as u8 => {
				next = Some(Twzobj::<NamespaceHdr>::init_guid(result.id, ProtFlags::READ));
			}
			x if x == NameEntType::Regular as u8 => {
				next = None;
			}
			x if x == NameEntType::Symlink as u8 => {
				if flags.contains_any(NameResolveFlags::FOLLOW_SYMLINKS)
					|| (flags.contains_any(NameResolveFlags::READLINK) && !last)
				{
					let symlink_name = result.symtarget();
					let (symlink_name, symlink_resolved_nameent) = hier_resolve_name(root, &nobj, symlink_name, flags)?;
					if symlink_resolved_nameent.ent_type == NameEntType::Namespace as u8 {
						next = Some(Twzobj::<NamespaceHdr>::init_guid(
							symlink_resolved_nameent.id,
							ProtFlags::READ,
						));
					} else {
						next = None;
					}
					if last {
						ret_name_ent = Some((
							ret_name_ent.unwrap_or((symlink_name, symlink_resolved_nameent)).0,
							symlink_resolved_nameent,
						));
					}
				} else {
					if last && flags.contains_any(NameResolveFlags::READLINK) {
						let symlink_name = result.symtarget();
						return Ok((symlink_name.to_vec(), *result));
					}
					next = None;
				}
			}
			_ => return Err(NameError::NotFound),
		}
	}

	let ret_name_ent = ret_name_ent.ok_or(NameError::NotFound)?;
	Ok(ret_name_ent)
}

pub fn name_test(id: ObjID) {
	let nameroot = Twzobj::<NamespaceHdr>::init_guid(id, ProtFlags::READ);

	let lookup_name = b"/bin/sh";
	let result = hier_resolve_name(&nameroot, &nameroot, lookup_name, NameResolveFlags::READLINK);
	match result {
		Ok(result) => {
			let (name, ent) = result;
			let name = std::str::from_utf8(&name).unwrap();
			println!(
				"{} ==> name: `{}' {:?}",
				std::str::from_utf8(lookup_name).unwrap(),
				name,
				ent
			)
		}
		Err(result) => println!("{:?}", result),
	}
}

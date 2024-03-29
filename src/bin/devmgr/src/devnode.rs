use std::collections::HashMap;
use std::{lazy::SyncLazy, sync::Mutex};
use twz::obj::ObjID;

#[derive(Debug)]
pub struct DeviceNode {
	pub name: String,
	pub id: ObjID,
}

struct DeviceNodeState {
	rootcounters: HashMap<String, u32>,
}

impl DeviceNodeState {
	fn new() -> DeviceNodeState {
		DeviceNodeState {
			rootcounters: HashMap::new(),
		}
	}
}

static STATE: SyncLazy<Mutex<DeviceNodeState>> = SyncLazy::new(|| Mutex::new(DeviceNodeState::new()));

pub fn publish(node: &DeviceNode) -> Result<(), ()> {
	let mut path = String::from("/dev/");
	path.push_str(&node.name);
	println!("[devmgr] publish node {}", path);
	/* TODO: dont unwrap */
	twz::name::bind_name(&path, node.id).unwrap();
	Ok(())
}

pub fn allocate(pairs: &[(&str, ObjID)]) -> Vec<DeviceNode> {
	let mut state = STATE.lock().unwrap();
	pairs
		.iter()
		.map(|(rootname, id)| {
			let num = if let Some(val) = state.rootcounters.get(*rootname) {
				let val = *val;
				state.rootcounters.insert(String::from(*rootname), val + 1);
				val + 1
			} else {
				state.rootcounters.insert(String::from(*rootname), 0);
				0
			};
			let name = format!("{}{}", rootname, num);
			DeviceNode { name: name, id: *id }
		})
		.collect()
}

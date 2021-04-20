use crate::libtwz;

type Result<T> = std::result::Result<T, crate::TwzErr>;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct QueueEntry<T> {
    cmd_id: u32,
    info: u32,
    item: T,
}

impl<T> QueueEntry<T> {
    pub fn new(item: T, info: u32) -> QueueEntry<T> {
        QueueEntry {
            cmd_id: 0,
            info: info,
            item: item,
        }
    }
}

struct QueueCompleter<T> {
    callback: fn(item: &T),
}

/*
fn is_eagain<T>(r: crate::queue::Result<T>) -> bool {
    if let Err(e) = r {
        if let crate::TwzErr::OSError(e) = e {
            return e == 11; //TODO
        }
    }
    return false;
}
*/

struct QueueHandler<T> {
    outstanding: std::collections::HashMap<u32, QueueCompleter<T>>,
    ids: std::vec::Vec<u32>,
    idcounter: u32,
}

pub struct Queue<S, C> {
    obj: crate::obj::Twzobj,
    handler: std::sync::Arc<std::sync::Mutex<QueueHandler<C>>>,
    _pd: std::marker::PhantomData<(S, C)>,
}

pub fn send<T: Copy>(obj: &crate::obj::Twzobj, item: T, flags: i32) -> crate::queue::Result<()> {
    let res = libtwz::queue_submit(obj, &item, flags);
    if res < 0 {
        Err(crate::TwzErr::OSError(res))
    } else {
        Ok(())
    }
}

pub fn recv<T: Copy>(obj: &crate::obj::Twzobj, flags: i32) -> crate::queue::Result<T> {
    let mut item: T = unsafe { std::mem::MaybeUninit::zeroed().assume_init() };
    let res = libtwz::queue_receive(obj, &mut item, flags);
    if res < 0 {
        Err(crate::TwzErr::OSError(res))
    } else {
        Ok(item)
    }
}

pub fn complete<T: Copy>(obj: &crate::obj::Twzobj, item: T, flags: i32) -> crate::queue::Result<()> {
    let res = libtwz::queue_complete(obj, &item, flags);
    if res < 0 {
        Err(crate::TwzErr::OSError(res))
    } else {
        Ok(())
    }
}

pub fn get_completion<T: Copy>(obj: &crate::obj::Twzobj, flags: i32) -> crate::queue::Result<T> {
    let mut item: T = unsafe { std::mem::MaybeUninit::zeroed().assume_init() };
    let res = libtwz::queue_get_completion(obj, &mut item, flags);
    if res < 0 {
        Err(crate::TwzErr::OSError(res))
    } else {
        Ok(item)
    }
}

pub fn create<S, C>(flags: i32, sqlen: usize, cqlen: usize, kuid: Option<crate::obj::Twzobj>) -> crate::queue::Result<crate::obj::Twzobj> {
    let id = libtwz::twz_object_create(flags, kuid.map_or(0, |o| o.id()), 0).map_err(|e| crate::TwzErr::OSError(e))?;
    let obj = crate::obj::Twzobj::init_guid(id)?;
    let res = libtwz::queue_init_hdr::<S, C>(&obj, sqlen, cqlen);
    if res < 0 {
        Err(crate::TwzErr::OSError(res))
    } else {
        Ok(obj)
    }
}

impl<S: Copy, C: Copy> Queue<S, C> {
    pub fn new_private(sqlen: usize, cqlen: usize) -> crate::queue::Result<Queue<S, C>> {
        Ok(Queue {
            obj: create::<QueueEntry<S>, QueueEntry<C>>(crate::obj::Twzobj::TWZ_OBJ_CREATE_DFL_READ | crate::obj::Twzobj::TWZ_OBJ_CREATE_DFL_WRITE, sqlen, cqlen, None)?,
            handler: std::sync::Arc::new(std::sync::Mutex::new(QueueHandler {
                outstanding: std::collections::HashMap::new(),
                ids: vec![],
                idcounter: 0,
            })),
            _pd: std::marker::PhantomData,
        })
    }

    pub fn from_obj(obj: crate::obj::Twzobj) -> Queue<S, C> {
        Queue {
            obj: obj,
            handler: std::sync::Arc::new(std::sync::Mutex::new(QueueHandler {
                outstanding: std::collections::HashMap::new(),
                ids: vec![],
                idcounter: 0,
            })),
            _pd: std::marker::PhantomData,
        }
    }

    pub fn obj(&self) -> &crate::obj::Twzobj {
        &self.obj
    }

    pub fn send(&self, item: QueueEntry<S>, flags: i32) -> crate::queue::Result<()> {
        send::<QueueEntry<S>>(&self.obj, item, flags)
    }

    pub fn recv(&self, flags: i32) -> crate::queue::Result<QueueEntry<C>> {
        recv::<QueueEntry<C>>(&self.obj, flags)
    }
    
    pub fn complete(&self, item: QueueEntry<S>, flags: i32) -> crate::queue::Result<()> {
        complete::<QueueEntry<S>>(&self.obj, item, flags)
    }

    pub fn get_completion(&self, flags: i32) -> crate::queue::Result<QueueEntry<C>> {
        get_completion::<QueueEntry<C>>(&self.obj, flags)
    }

    pub fn send_callback(&self, mut item: QueueEntry<S>, flags: i32, callback: fn(item: &C)) -> crate::queue::Result<()> {
        let id = {
            let mut handler = self.handler.lock().unwrap();
            let id = if let Some(id) = handler.ids.pop() {
                id
            } else {
                let id = handler.idcounter;
                handler.idcounter += 1;
                id
            };
            handler.outstanding.insert(id, QueueCompleter { callback: callback });
            id
        };
        item.info = id;
        let res = self.send(item, flags);
        if res.is_err() {
            let mut handler = self.handler.lock().unwrap();
            handler.outstanding.remove(&id);
            handler.ids.push(id);
        }
        res
    }

    pub fn check_completions_callback(&self, flags: i32) -> crate::queue::Result<Option<QueueEntry<C>>> {
        let res = self.get_completion(flags)?;
        let completer = {
            let mut handler = self.handler.lock().unwrap();
            let completer = handler.outstanding.remove(&res.info);
            if completer.is_some() {
                handler.ids.push(res.info);
            }
            completer
        };
        if let Some(completer) = completer {
            (completer.callback)(&res.item);
            Ok(None)
        } else {
            Ok(Some(res))
        }
    }
}

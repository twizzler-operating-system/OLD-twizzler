use crate::sys::thread_sync;
use crate::sys::ThreadSyncArgs;
use std::cell::UnsafeCell;
use std::ops::{Deref, DerefMut};
use std::sync::atomic::AtomicU64;
use std::sync::atomic::Ordering;

#[repr(C)]
#[derive(Default)]
pub struct TwzMutex<T> {
	sleep: AtomicU64,
	resetcode: AtomicU64,
	data: UnsafeCell<T>,
}

pub struct TwzMutexGuard<'a, T> {
	mutex: &'a TwzMutex<T>,
}

/*impl<T: Default> TwzMutex<T> {
	fn new2() -> TwzMutex<T> {
		TwzMutex {
			sleep: AtomicU64::new(0),
			resetcode: AtomicU64::new(0),
			..Default::default() //data: UnsafeCell::new(t),
		}
	}
}*/

impl<T: Copy> TwzMutex<T> {
	pub fn new(t: T) -> TwzMutex<T> {
		TwzMutex {
			sleep: AtomicU64::new(0),
			resetcode: AtomicU64::new(0),
			data: UnsafeCell::new(t),
		}
	}
}

/*impl<T: Default> Default for TwzMutex<T> {
	fn default() -> Self {
		TwzMutex::<T>::new2()
	}
}*/

static RESET_CODE: AtomicU64 = AtomicU64::new(0);

impl<'a, T> TwzMutex<T> {
	fn maybe_bust_lock(&self) {
		if RESET_CODE.load(Ordering::SeqCst) == 0 {
			let r = crate::sys::kconf(crate::sys::KCONF_RDRESET, 0);
			/* this code will always be the same within the same power cycle. We can afford a few
			 * overwrites. */
			RESET_CODE.store(r as u64, Ordering::SeqCst);
		}

		if self.resetcode.load(Ordering::SeqCst) != RESET_CODE.load(Ordering::SeqCst) {
			/* might need to bust the lock. Make sure everyone still comes in here until we're done by
			 * changing the resetcode to -1. If we exchange and get 1, we wait. If we don't get -1,
			 * then we're the one that's going to bust the lock. */
			let value = self.resetcode.swap(!0u64, Ordering::SeqCst);
			if value != !0u64 {
				/* bust lock, and then store new reset code */
				self.sleep.store(0, Ordering::SeqCst);
				self.resetcode
					.store(RESET_CODE.load(Ordering::SeqCst), Ordering::SeqCst);
			} else {
				while self.resetcode.load(Ordering::SeqCst) == !0u64 {
					std::hint::spin_loop();
				}
			}
		}
	}

	fn do_lock(&self) {
		/* try to grab the lock by spinning */
		for _ in 0..100 {
			if self
				.sleep
				.compare_exchange(0, 1, Ordering::SeqCst, Ordering::SeqCst)
				.is_ok()
			{
				return;
			}
			std::hint::spin_loop();
		}

		/* indicate waiting */
		let mut v = self.sleep.swap(2, Ordering::SeqCst);

		/* actually sleep for the lock */
		let mut ts = vec![ThreadSyncArgs::new_sleep(&self.sleep, 2)];
		while v != 0 {
			thread_sync(&mut ts, None);

			v = self.sleep.swap(2, Ordering::SeqCst);
		}
	}

	pub fn lock(&'a self) -> TwzMutexGuard<'a, T> {
		self.maybe_bust_lock();
		self.do_lock();
		TwzMutexGuard { mutex: self }
	}

	pub fn unlock(&self) {
		if self.sleep.load(Ordering::SeqCst) == 2 {
			self.sleep.store(0, Ordering::SeqCst);
		} else if self.sleep.swap(0, Ordering::SeqCst) == 1 {
			return;
		}

		let mut ts = vec![ThreadSyncArgs::new_wake(&self.sleep, 1)];
		thread_sync(&mut ts, None);
	}
}

impl<T> Deref for TwzMutexGuard<'_, T> {
	type Target = T;
	fn deref(&self) -> &Self::Target {
		unsafe { &*self.mutex.data.get() }
	}
}

impl<T> DerefMut for TwzMutexGuard<'_, T> {
	fn deref_mut(&mut self) -> &mut Self::Target {
		unsafe { &mut *self.mutex.data.get() }
	}
}

impl<T> Drop for TwzMutexGuard<'_, T> {
	fn drop(&mut self) {
		self.mutex.unlock()
	}
}

impl<T: Clone + Copy> Clone for TwzMutex<T> {
	fn clone(&self) -> Self {
		TwzMutex::new(self.lock().clone())
	}
}

unsafe impl<T> Send for TwzMutex<T> where T: Send {}
unsafe impl<T> Sync for TwzMutex<T> where T: Send {}

unsafe impl<T> Send for TwzMutexGuard<'_, T> where T: Send {}
unsafe impl<T> Sync for TwzMutexGuard<'_, T> where T: Send + Sync {}

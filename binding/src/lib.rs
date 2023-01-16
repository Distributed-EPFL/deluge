pub use self::ffi::api::*;


mod ffi {
    extern crate libc;


    use libc::{c_int, c_void};
    use std::option::Option;


    pub const DELUGE_SUCCESS: c_int = 0;
    pub const DELUGE_FAILURE: c_int = -1;
    pub const DELUGE_NODEV: c_int = -2;
    pub const DELUGE_OUT_OF_GMEM: c_int = -3;
    pub const DELUGE_OUT_OF_LMEM: c_int = -4;
    pub const DELUGE_CANCEL: c_int = -5;


    #[repr(C)]
    #[derive(Debug, Copy, Clone)]
    pub struct deluge {
	_unused: [u8; 0],
    }

    #[allow(non_camel_case_types)]
    pub type deluge_t = *mut deluge;


    #[repr(C)]
    #[derive(Debug, Copy, Clone)]
    pub struct highway {
	_unused: [u8; 0],
    }

    #[allow(non_camel_case_types)]
    pub type highway_t = *mut highway;


    extern "C" {
	pub fn deluge_create(deluge: *mut deluge_t) -> c_int;

	pub fn deluge_destroy(deluge: deluge_t);

	pub fn deluge_highway_create(
            deluge: deluge_t,
            highway: *mut highway_t,
            key: *const u64
	) -> c_int;

	pub fn deluge_highway_destroy(highway: highway_t);

	pub fn deluge_highway_space(highway: highway_t) -> usize;

	pub fn deluge_highway_alloc(
	    highway: highway_t,
	    len: usize
	) -> c_int;

	pub fn deluge_highway_schedule(
            highway: highway_t,
            elems: *const u64,
            nelem: usize,
            cb: Option<
		unsafe extern "C" fn(
                    arg1: c_int,
                    arg2: *mut u64,
                    arg3: *const c_void,
		),
		>,
            user: *const c_void,
	) -> c_int; 
    }


    pub mod api {
	use std::{
	    convert::TryInto,
	    future::Future,
	    pin::Pin,
	    ptr,
	    slice,
	    sync::{Arc, Mutex},
	    task::{Context, Poll, Waker},
	};

	use super::*;


	#[derive(Clone, Debug, PartialEq)]
	pub enum Error {
	    Failure,        // Implementation issue, use debug mode
	    NoDev,          // Not suitable device
	    OutOfGmem,      // Not enough device global memory
	    OutOfLmem,      // Not enough device local memory
	    Cancel,         // Job canceled
	}

	pub struct Deluge {
	    inner: deluge_t,
	}

	pub struct Highway {
	    inner: highway_t,
	}

	pub struct HighwayHashSum<'a> {
	    runner: &'a Highway,
	    inner: Arc<Mutex<HighwayHashSumData>>,  // TODO: lifetime > Arc
	}

	#[derive(Debug)]
	enum HighwayHashSumState {
	    Created,                             // Created(&'a &[u64])
	    Running(Option<Waker>),
	    Finished(Result<[u64; 5], Error>),
	}

	struct HighwayHashSumData {
	    input: Vec<u64>,                     // put that as a ref in state
	    state: HighwayHashSumState,
	}


	fn code_to_err(code: c_int) -> Error {
	    match code {
		DELUGE_FAILURE => Error::Failure,
		DELUGE_NODEV => Error::NoDev,
		DELUGE_OUT_OF_GMEM => Error::OutOfGmem,
		DELUGE_OUT_OF_LMEM => Error::OutOfLmem,
		DELUGE_CANCEL => Error::Cancel,
		_ => Error::Failure
	    }
	}


	impl Deluge {
	    pub fn new() -> Result<Self, Error> {
		unsafe {
		    let mut inner: deluge_t = ptr::null_mut();

		    match deluge_create(&mut inner) {
			DELUGE_SUCCESS => Ok(Self { inner }),
			code => Err(code_to_err(code))
		    }
		}
	    }

	    pub fn new_highway(
		&mut self,
		key: &[u64; 4]
	    ) -> Result<Highway, Error> {
		unsafe {
		    let mut inner: highway_t = ptr::null_mut();

		    match deluge_highway_create(self.inner, &mut inner,
						key.as_ptr()) {
			DELUGE_SUCCESS => Ok(Highway { inner }),
			code => Err(code_to_err(code))
		    }
		}
	    }
	}

	impl Drop for Deluge {
	    fn drop(&mut self) {
		unsafe {
		    deluge_destroy(self.inner);
		}
	    }
	}

	
	extern "C" fn update_cb(
	    status: c_int,
	    res: *mut u64,
	    user: *const c_void
	) {
	    let arc = unsafe {
		Arc::from_raw(user as *const Mutex<HighwayHashSumData>)
	    };
	    let mut data = arc.lock().unwrap();

	    match &mut data.state {
		HighwayHashSumState::Running(waker) => {
		    if let Some(waker) = waker.take() {
			waker.wake();
		    }

		    data.state = HighwayHashSumState::Finished(match status {
			DELUGE_SUCCESS => Ok(unsafe {
			    slice::from_raw_parts(res, 5)
			}.try_into().unwrap()),
			code => Err(code_to_err(code))
		    });
		},

		_ => panic!("corrupted state!")
	    }
	}

	impl Highway {
	    pub fn space(&mut self) -> usize {
		unsafe {
		    deluge_highway_space(self.inner)
		}
	    }

	    pub fn alloc(&mut self, len: usize) -> Result<(), Error> {
		unsafe {
		    match deluge_highway_alloc(self.inner, len) {
			DELUGE_SUCCESS => Ok(()),
			code => Err(code_to_err(code))
		    }
		}
	    }

	    pub fn hashsum(&mut self, elems: &[u64]) -> HighwayHashSum<'_> {
		HighwayHashSum::new(self, elems)		
	    }
	}
	
	impl Drop for Highway {
	    fn drop(&mut self) {
		unsafe {
		    deluge_highway_destroy(self.inner);
		}
	    }
	}



	impl<'a> HighwayHashSum<'a> {
	    fn new(runner: &'a mut Highway, input: &[u64]) -> Self {
		let data = Arc::new(Mutex::new(HighwayHashSumData {
		    input: input.to_vec(),
		    state: HighwayHashSumState::Created,
		}));

		HighwayHashSum {
		    runner,
		    inner: data
		}
	    }
	}

	impl<'a> Future for HighwayHashSum<'a> {
	    type Output = Result<[u64; 5], Error>;

	    fn poll(
		self: Pin<&mut Self>,
		cx: &mut Context<'_>
	    ) -> Poll<Self::Output> {
		let mut data = self.inner.lock().unwrap();

		match &data.state {
		    HighwayHashSumState::Created => {
			match unsafe {
			    deluge_highway_schedule(
				self.runner.inner, data.input.as_ptr(),
				data.input.len(), Some(update_cb),
				Arc::into_raw(Arc::clone(&self.inner)) as *const c_void
			    )
			} {
			    DELUGE_SUCCESS => {
				data.state = HighwayHashSumState::Running
				    (Some(cx.waker().clone()));
				Poll::Pending
			    },
			    code => Poll::Ready(Err(code_to_err(code)))
			}
		    },

		    HighwayHashSumState::Finished(res) =>
			Poll::Ready(res.clone()),

		    _ => Poll::Pending
		}
	    }
	}
    }
}




#[cfg(test)]
mod tests {
    use async_std::task;
    use super::*;

    #[test]
    fn highway() {
	let key: [u64; 4] = [ 0, 1, 2, 3 ];
	let mut rh = Deluge::new().and_then(|mut d| d.new_highway(&key));

	assert!(rh.is_ok());

	match rh.as_mut().unwrap().space() {
	    space if space > 0 => {
		assert!(rh.as_mut().unwrap().alloc(space).is_ok());
		assert!(task::block_on(rh.as_mut().unwrap().hashsum(&[ 0, 1, 2, 3, 4 ])) == Ok([8599386087226438984, 5313723462932329624, 10655888748344772545, 427566931447375920, 4]));
	    },
	    space => assert!(space == 0)
	}
    }
}


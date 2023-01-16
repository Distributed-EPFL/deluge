extern crate libc;

use libc::{c_int, c_void};

use std::{
    convert::TryInto,
    future::Future,
    pin::Pin,
    ptr,
    slice,
    sync::Mutex,
    task::{Context, Poll, Waker},
};


mod ffi {
    use std::option::Option;
    use super::{c_int, c_void};


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
}


#[derive(Clone, Debug, PartialEq)]
pub enum Error {
    Failure,        // Implementation issue, use debug mode
    NoDev,          // Not suitable device
    OutOfGmem,      // Not enough device global memory
    OutOfLmem,      // Not enough device local memory
    Cancel,         // Job canceled
}

pub struct Deluge {
    inner: ffi::deluge_t,
}

pub struct Highway {
    inner: ffi::highway_t,
}

pub struct HighwayHashSum<'a> {
    inner: Mutex<HighwayHashSumState<'a>>,
}

enum HighwayHashSumState<'a> {
    Created(&'a Highway, &'a[u64]),
    Running(Option<Waker>),
    Finished(Result<[u64; 5], Error>),
}


fn code_to_err(code: c_int) -> Error {
    match code {
	ffi::DELUGE_FAILURE => Error::Failure,
	ffi::DELUGE_NODEV => Error::NoDev,
	ffi::DELUGE_OUT_OF_GMEM => Error::OutOfGmem,
	ffi::DELUGE_OUT_OF_LMEM => Error::OutOfLmem,
	ffi::DELUGE_CANCEL => Error::Cancel,
	_ => Error::Failure
    }
}


impl Deluge {
    pub fn new() -> Result<Self, Error> {
	unsafe {
	    let mut inner: ffi::deluge_t = ptr::null_mut();

	    match ffi::deluge_create(&mut inner) {
		ffi::DELUGE_SUCCESS => Ok(Self { inner }),
		code => Err(code_to_err(code))
	    }
	}
    }

    pub fn new_highway(
	&mut self,
	key: &[u64; 4]
    ) -> Result<Highway, Error> {
	unsafe {
	    let mut inner: ffi::highway_t = ptr::null_mut();

	    match ffi::deluge_highway_create(
		self.inner, &mut inner, key.as_ptr()
	    ) {
		ffi::DELUGE_SUCCESS => Ok(Highway { inner }),
		code => Err(code_to_err(code))
	    }
	}
    }
}

impl Drop for Deluge {
    fn drop(&mut self) {
	unsafe { ffi::deluge_destroy(self.inner); }
    }
}

	
extern "C" fn update_cb(
    status: c_int,
    res: *mut u64,
    user: *const c_void
) {
    let mut state = unsafe {
	&*(user as *const Mutex<HighwayHashSumState>)
    }.lock().unwrap();

    match &mut *state {
	HighwayHashSumState::Running(waker) => {
	    if let Some(waker) = waker.take() {
		waker.wake();
	    }

	    *state = HighwayHashSumState::Finished(match status {
		ffi::DELUGE_SUCCESS => Ok(unsafe {
		    slice::from_raw_parts(res, 5)
		}.try_into().unwrap()),
		code => Err(code_to_err(code))
	    });
	},

	_ => panic!("corrupted state!")
    }
}

impl Highway {
    pub fn space(&self) -> usize {
	unsafe { ffi::deluge_highway_space(self.inner) }
    }

    pub fn alloc(&mut self, len: usize) -> Result<(), Error> {
	unsafe {
	    match ffi::deluge_highway_alloc(self.inner, len) {
		ffi::DELUGE_SUCCESS => Ok(()),
		code => Err(code_to_err(code))
	    }
	}
    }

    pub fn hashsum<'a>(&'a self, elems: &'a[u64]) -> HighwayHashSum<'a> {
	HighwayHashSum::new(self, elems)
    }
}

impl Drop for Highway {
    fn drop(&mut self) {
	unsafe { ffi::deluge_highway_destroy(self.inner); }
    }
}



impl<'a> HighwayHashSum<'a> {
    fn new(runner: &'a Highway, input: &'a[u64]) -> Self {
	HighwayHashSum {
	    inner: Mutex::new(HighwayHashSumState::Created(runner, input)),
	}
    }
}

impl<'a> Future for HighwayHashSum<'a> {
    type Output = Result<[u64; 5], Error>;

    fn poll(
	self: Pin<&mut Self>,
	cx: &mut Context<'_>
    ) -> Poll<Self::Output> {
	let mut state = self.inner.lock().unwrap();

	match &*state {
	    HighwayHashSumState::Created(runner, input) => {
		match unsafe {
		    ffi::deluge_highway_schedule(
			runner.inner, input.as_ptr(), input.len(),
			Some(update_cb),
			&self.inner as *const _ as *const c_void
		    )
		} {
		    ffi::DELUGE_SUCCESS => {
			*state = HighwayHashSumState::Running
			    (Some(cx.waker().clone()));
			Poll::Pending
		    },
		    code => Poll::Ready(Err(code_to_err(code)))
		}
	    },

	    HighwayHashSumState::Finished(res) => Poll::Ready(res.clone()),

	    _ => Poll::Pending
	}
    }
}


#[cfg(test)]
mod tests {
    use async_std::task;
    use super::*;


    #[test]
    fn deluge() {
	let deluge = Deluge::new();

	assert!(deluge.is_ok());

	let deluge = Deluge::new();

	assert!(deluge.is_ok());
    }

    #[test]
    fn highway_dispatch() {
	let mut deluge = Deluge::new().unwrap();
	let mut highway = deluge.new_highway(&[0, 0, 0, 0]);

	assert!(highway.is_ok());
	assert!(highway.as_mut().unwrap().space() > 0);

	{
	    let mut highway2 = deluge.new_highway(&[1, 1, 1, 1]);

	    assert!(highway2.is_ok());
	    assert!(highway2.as_mut().unwrap().space() > 0);
	}

	assert!(highway.as_mut().unwrap().space() > 0);
    }

    #[test]
    fn highway_station() {
	let mut highway = Deluge::new().unwrap()
	    .new_highway(&[0, 0, 0, 0]).unwrap();
	let space = highway.space();

	assert!(space > 0);
	assert!(highway.alloc(space).is_ok());
    }

    #[test]
    fn highway_station_overalloc() {
	let mut highway = Deluge::new().unwrap()
	    .new_highway(&[0, 0, 0, 0]).unwrap();

	let space = highway.space();

	assert!(space > 0);
	assert!(highway.alloc(space + 1).is_err());
    }

    #[test]
    fn highway_station_peer_overalloc() {
	let mut deluge = Deluge::new().unwrap();
	let mut highway0 = deluge.new_highway(&[0, 0, 0, 0]).unwrap();
	let mut highway1 = deluge.new_highway(&[1, 1, 1, 1]).unwrap();

	let space = highway0.space();

	assert!(space > 0);
	assert!(highway0.alloc(space).is_ok());
	assert!(highway1.alloc(1).is_err());
    }


    #[test]
    fn highway_station_peer_alloc() {
	let mut deluge = Deluge::new().unwrap();

	{
	    let mut highway = deluge.new_highway(&[0, 0, 0, 0]).unwrap();
	    let space = highway.space();

	    assert!(space > 0);
	    assert!(highway.alloc(space).is_ok());
	}

	let mut highway = deluge.new_highway(&[1, 1, 1, 1]).unwrap();
	let space = highway.space();

	assert!(space > 0);
	assert!(highway.alloc(space).is_ok());
    }

    #[test]
    fn hashsum_1() {
	let mut highway = Deluge::new().unwrap()
	    .new_highway(&[0, 0, 0, 0]).unwrap();

	highway.alloc(1).unwrap();

	let r0 = highway.hashsum(&[ 0 ]);
	let r42 = highway.hashsum(&[ 42 ]);
	let rx42 = highway.hashsum(&[ 0x42 ]);

	assert!(task::block_on(r0) == Ok([
	    0x0,
	    0x0701193da083522b,
	    0x6b99c091c72ef638,
	    0x24dc7fc2c8b68d6a,
	    0xac3786ba2a5e196a,
	]));

	assert!(task::block_on(r42) == Ok([
	    0x0,
	    0xfd00061d65b9bf7a,
	    0x84b3581cde02ecd3,
	    0xaf7b369076ebe2eb,
	    0xfbd66042f30b2659,
	]));

	assert!(task::block_on(rx42) == Ok([
	    0x0,
	    0xe4bb8aab495594b0,
	    0xa4121785c37f59e7,
	    0xdf4fd944e18a068c,
	    0x2edfa2ccc62dab81,
	]));
    }

    #[test]
    fn hashsum_3() {
	let mut highway = Deluge::new().unwrap()
	    .new_highway(&[0, 0, 0, 0]).unwrap();

	highway.alloc(1).unwrap();

	assert!(task::block_on(highway.hashsum(&[ 0, 42, 0x42 ])) == Ok([
	    0x1,
	    0xe8bcaa064f92a656,
	    0x945f303468b13cf3,
	    0xb3a78f98212c76e2,
	    0xd6ed89c9e396eb44,
	]));
    }

    #[test]
    fn hashsum_131072() {
	let input = (0..131072).collect::<Vec<u64>>();
	let mut highway = Deluge::new().unwrap().new_highway(&[
	    0, 0, 0, 0
	]).unwrap();

	highway.alloc(1).unwrap();

	assert!(task::block_on(highway.hashsum(&input)) == Ok([
	    0xffb6,
	    0xc8ae5496e8d216e5,
	    0x84fee0def30639fe,
	    0xb486b72eb5d5d3a9,
	    0x1dbb0f7a65c6f59c,
	]));

	let mut highway = Deluge::new().unwrap().new_highway(&[
	    0x55551badb002ffff,
	    0x1010cafebabe0101,
	    0x1234deadbeef4321,
	    0x0000deadc0de0000
	]).unwrap();

	highway.alloc(1).unwrap();

	assert!(task::block_on(highway.hashsum(&input)) == Ok([
	    0xffca,
	    0x1647d33b8317b1d0,
	    0xbe3da9ac48240bbf,
	    0x02c7914b8f418d38,
	    0xe50b1599653c514f,
	]));
    }
}

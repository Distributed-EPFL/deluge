#[cfg(all(feature = "opencl", feature = "native"))]
compile_error!("must choose only one of `opencl` and `native`");

#[cfg(all(not(feature = "opencl"), not(feature = "native")))]
compile_error!("must choose one of `opencl` and `native`");


#[cfg(feature = "opencl")]
use libc::{c_int, c_void};

#[cfg(feature = "opencl")]
use libdeluge_sys as ffi;

use std::{
    future::Future,
    pin::Pin,
    task::{Context, Poll},
};


#[derive(Clone, Debug, PartialEq)]
pub enum Error {
    Failure,        // Implementation issue, use debug mode
    NoDev,          // Not suitable device
    NoMem,          // Not enough memory
    Cancel,         // Job canceled
}

#[cfg(feature = "opencl")]
fn code_to_err(code: c_int) -> Error {
    match code {
	ffi::DELUGE_FAILURE => Error::Failure,
	ffi::DELUGE_NODEV => Error::NoDev,
	ffi::DELUGE_NOMEM => Error::NoMem,
	ffi::DELUGE_CANCEL => Error::Cancel,
	_ => Error::Failure
    }
}


#[cfg(feature = "opencl")]
pub mod hashsum64 {
    use std::{
	convert::TryInto,
	ptr,
	slice,
	sync::Mutex,
	task::Waker,
    };
    use super::*;


    pub struct Dispatch {
	inner: ffi::dispatch_t,
    }

    pub struct Future<'a> {
	inner: Mutex<State<'a>>,
    }

    enum State<'a> {
	Created(&'a Dispatch, &'a[u64]),
	Running(Option<Waker>),
	Finished(Result<[u8; 40], Error>),
    }


    impl Dispatch {
	pub fn new_blake3(
	    key: &[u8; 32],
	) -> Result<Self, Error> {
	    unsafe {
		let mut inner: ffi::dispatch_t = ptr::null_mut();

		match ffi::deluge_hashsum64_blake3_create(
		    &mut inner, key.as_ptr()
		) {
		    ffi::DELUGE_SUCCESS => Ok(Dispatch { inner }),
		    code => Err(code_to_err(code))
		}
	    }
	}

	pub fn schedule<'a>(&'a self, elems: &'a[u64]) -> Future<'a> {
	    Future::new(self, elems)
	}
    }

    impl Drop for Dispatch {
	fn drop(&mut self) {
	    unsafe { ffi::deluge_dispatch_destroy(self.inner); }
	}
    }

    impl<'a> Future<'a> {
	fn new(dispatch: &'a Dispatch, elems: &'a[u64]) -> Self {
	    Self {
		inner: Mutex::new(State::Created(dispatch, elems)),
	    }
	}
    }

    extern "C" fn finish(
	status: c_int,
	result: *mut u8,
	user: *const c_void
    ) {
	let mut state = unsafe {
	    &*(user as *const Mutex<State>)
	}.lock().unwrap();

	match &mut *state {
	    State::Running(waker) => {
		if let Some(waker) = waker.take() {
		    waker.wake();
		}

		*state = State::Finished(match status {
		    ffi::DELUGE_SUCCESS => Ok(unsafe {
			slice::from_raw_parts(result, 40)
		    }.try_into().unwrap()),
		    code => Err(code_to_err(code))
		});
	    },

	    _ => panic!("corrupted state!")
	}
    }

    impl<'a> super::Future for Future<'a> {
	type Output = Result<[u8; 40], Error>;

	fn poll(
	    self: Pin<&mut Self>,
	    cx: &mut Context<'_>
	) -> Poll<Self::Output> {
	    let mut state = self.inner.lock().unwrap();

	    match &*state {
		State::Created(dispatch, elems) => {
		    match unsafe {
			ffi::deluge_hashsum64_schedule(
			    dispatch.inner, elems.as_ptr(), elems.len(),
			    Some(finish),
			    &self.inner as *const _ as *const c_void
			)
		    } {
			ffi::DELUGE_SUCCESS => {
			    *state = State::Running
				(Some(cx.waker().clone()));
			    Poll::Pending
			},
			code => Poll::Ready(Err(code_to_err(code)))
		    }
		},

		State::Finished(res) => Poll::Ready(res.clone()),

		_ => Poll::Pending
	    }
	}
    }
}


#[cfg(feature = "native")]
pub mod hashsum64 {
    use blake3::keyed_hash;
    use num::{BigUint, Zero};
    use super::*;


    pub struct Dispatch {
	key: [u8; 32],
    }

    pub struct Future<'a> {
	_dispatch: &'a Dispatch,
	_elems: &'a[u64],
	result: [u8; 40],
    }


    impl Dispatch {
	pub fn new_blake3(
	    key: &[u8; 32],
	) -> Result<Self, Error> {
	    Ok(Self { key: key.clone() })
	}

	pub fn schedule<'a>(&'a self, elems: &'a[u64]) -> Future<'a> {
	    Future::new(self, elems)
	}
    }

    impl<'a> Future<'a> {
	fn new(dispatch: &'a Dispatch, elems: &'a[u64]) -> Self {
	    let mut sum = BigUint::zero();

	    for elem in elems {
		let bytes = Self::hash_single(dispatch, *elem);
		sum += BigUint::from_bytes_be(&bytes);
	    }

	    let sumbe = sum.to_bytes_be();
	    let mut result = [0; 40];
	    let (_, val) = result.split_at_mut(40 - sumbe.len());

	    val.copy_from_slice(&sumbe);

	    Self {
		_dispatch: dispatch,
		_elems: elems,
		result,
	    }
	}

	fn hash_single(dispatch: &'a Dispatch, elem: u64) -> [u8; 40] {
	    let mut result = [0; 40];
	    let (pad, val) = result.split_at_mut(8);

	    pad.copy_from_slice(&(0 as u64).to_be_bytes());
	    val.copy_from_slice(keyed_hash(&dispatch.key, &elem.to_be_bytes())
				.as_bytes());

	    result
	}
    }

    impl super::Future for Future<'_> {
	type Output = Result<[u8; 40], Error>;

	fn poll(
	    self: Pin<&mut Self>,
	    _cx: &mut Context<'_>
	) -> Poll<Self::Output> {
	    Poll::Ready(Ok(self.result.clone()))
	}
    }
}


#[cfg(test)]
mod tests {
    use async_std::task;
    use super::*;


    #[test]
    fn blake3_new() {
	let dispatch = hashsum64::Dispatch::new_blake3(&[0; 32]);

	assert!(dispatch.is_ok());

	let dispatch = hashsum64::Dispatch::new_blake3(&[1; 32]);

	assert!(dispatch.is_ok());
    }

    #[test]
    fn blake3_singleton() {
	let disp = hashsum64::Dispatch::new_blake3(&[0; 32]).unwrap();

	assert!(task::block_on(disp.schedule(&[ 0x0000000000000000 ])) == Ok([
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x69, 0xed, 0xf2, 0xeb, 0xde, 0x07, 0x8f, 0xad,
	    0x5f, 0xa6, 0x4a, 0x0b, 0x7f, 0x9b, 0x0f, 0xd3,
	    0x28, 0x6e, 0x39, 0x26, 0xd6, 0x3e, 0xc1, 0x10,
	    0xdf, 0x00, 0x66, 0xef, 0xcb, 0x77, 0x65, 0x80,
	]));

	assert!(task::block_on(disp.schedule(&[ 0x0000000000000001 ])) == Ok([
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x82, 0x90, 0x95, 0xe6, 0x5d, 0x1b, 0x42, 0x9a,
	    0xad, 0x42, 0x04, 0xa6, 0x33, 0xbd, 0x63, 0x80,
	    0x87, 0x3f, 0x47, 0xc2, 0xb1, 0xf4, 0x18, 0x95,
	    0xe1, 0x8b, 0x6c, 0xfa, 0xf3, 0x41, 0xe6, 0xde, 
	]));

	assert!(task::block_on(disp.schedule(&[ 0xfedcba9876543210 ])) == Ok([
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x40, 0x35, 0x1c, 0xc8, 0xaf, 0xcd, 0x9b, 0xec,
	    0x83, 0x96, 0xc0, 0x08, 0x38, 0x0f, 0xd8, 0xa7,
	    0x3b, 0x8b, 0x13, 0xbb, 0xd9, 0x47, 0xd7, 0xf7,
	    0x27, 0x0b, 0xb7, 0x34, 0x49, 0x14, 0x17, 0x00,
	]));

	let disp = hashsum64::Dispatch::new_blake3(&[
	    0xaa, 0x55, 0xcc, 0x33, 0x66, 0x99, 0xa5, 0x5a,
	    0xac, 0xca, 0xa3, 0x3a, 0xa6, 0x6a, 0xa9, 0x9a,
	    0x5c, 0xc5, 0x53, 0x35, 0x56, 0x65, 0x59, 0x95,
	    0xc3, 0x3c, 0xc6, 0x6c, 0xc9, 0x9c, 0x69, 0x96,
	]).unwrap();

	assert!(task::block_on(disp.schedule(&[ 0x0000000000000000 ])) == Ok([
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x5f, 0x25, 0x1e, 0xa1, 0x12, 0x60, 0x33, 0x6e,
	    0x68, 0x7a, 0x2b, 0xc5, 0x43, 0xa6, 0xed, 0x83,
	    0x84, 0x4a, 0x59, 0xe8, 0xa2, 0xe8, 0xf0, 0xe0,
	    0x9e, 0x29, 0x87, 0x14, 0x61, 0x03, 0x86, 0xe2,
	]));

	assert!(task::block_on(disp.schedule(&[ 0x0000000000000001 ])) == Ok([
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0xb7, 0x9c, 0xcd, 0x9d, 0xfb, 0x32, 0xfc, 0x2e,
	    0xc1, 0x2a, 0xd1, 0x91, 0x62, 0x7c, 0xc6, 0x1f,
	    0x7a, 0xf5, 0xd2, 0x20, 0xa2, 0x10, 0xb6, 0xc0,
	    0xd8, 0xc9, 0x9c, 0xc3, 0x92, 0x52, 0x6f, 0x5c,
	]));

	assert!(task::block_on(disp.schedule(&[ 0xfedcba9876543210 ])) == Ok([
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x28, 0xc7, 0xf8, 0x8a, 0x55, 0x40, 0xff, 0x6a,
	    0x68, 0x9e, 0x09, 0x09, 0xa3, 0xb4, 0xbe, 0x6e,
	    0x3a, 0x62, 0xd4, 0xcd, 0x83, 0x19, 0x4d, 0x8f,
	    0xad, 0x94, 0x18, 0xba, 0x70, 0xfd, 0x91, 0x64,
	]));
    }

    #[test]
    fn blake3_batch() {
	let disp = hashsum64::Dispatch::new_blake3(&[
	    0xaa, 0x55, 0xcc, 0x33, 0x66, 0x99, 0xa5, 0x5a,
	    0xac, 0xca, 0xa3, 0x3a, 0xa6, 0x6a, 0xa9, 0x9a,
	    0x5c, 0xc5, 0x53, 0x35, 0x56, 0x65, 0x59, 0x95,
	    0xc3, 0x3c, 0xc6, 0x6c, 0xc9, 0x9c, 0x69, 0x96,
	]).unwrap();

	assert!(task::block_on(disp.schedule(&[
	    0x0000000000000000,
	    0x0000000000000001,
	    0xfedcba9876543210,
	])) == Ok([
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	    0x3f, 0x89, 0xe4, 0xc9, 0x62, 0xd4, 0x2f, 0x07,
	    0x92, 0x43, 0x06, 0x60, 0x49, 0xd8, 0x72, 0x11,
	    0x39, 0xa3, 0x00, 0xd6, 0xc8, 0x12, 0xf5, 0x31,
	    0x24, 0x87, 0x3c, 0x92, 0x64, 0x53, 0x87, 0xa2,
	]));

	assert!(task::block_on(disp.schedule(&[
	    0x1c80317fa3b1799d, 0xbdd640fb06671ad1,
	    0x3eb13b9046685257, 0x23b8c1e9392456de,
	    0x1a3d1fa7bc8960a9, 0xbd9c66b3ad3c2d6d,
	    0x8b9d2434e465e150, 0x972a846916419f82,
	    0x0822e8f36c031199, 0x17fc695a07a0ca6e,
	    0x3b8faa1837f8a88b, 0x9a1de644815ef6d1,
	    0x8fadc1a606cb0fb3, 0xb74d0fb132e70629,
	    0xb38a088ca65ed389, 0x6b65a6a48b8148f6,
	    0x72ff5d2a386ecbe0, 0x4737819096da1dac,
	    0xde8a774bcf36d58b, 0xc241330b01a9e71f,
	    0x28df6ec4ce4a2bbd, 0x6c307511b2b9437a,
	    0x47229389571aa876, 0x371ecd7b27cd8130,
	    0xc37459eef50bea63, 0x1a2a73ed562b0f79,
	    0x6142ea7d17be3111, 0x5be6128e18c26797,
	    0x580d7b71d8f56413, 0x43b7a3a69a8dca03,
	    0x0b1f9163ce9ff57f, 0x759cde66bacfb3d0,
	])) == Ok([
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e,
	    0x8e, 0x73, 0xe3, 0x3c, 0xb1, 0x05, 0xf3, 0xe4,
	    0x37, 0x0a, 0x3d, 0xc4, 0x36, 0xd6, 0xc8, 0xef,
	    0x34, 0x7d, 0x39, 0x60, 0x50, 0xa3, 0x2d, 0x3e,
	    0xc5, 0x63, 0x0b, 0x5d, 0x3a, 0x8b, 0xe6, 0x7e,
	]));

	assert!(task::block_on(disp.schedule(
	    (0 .. 65536).collect::<Vec<u64>>().as_slice())
	) == Ok([
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xb2,
	    0xc9, 0x56, 0xa2, 0x95, 0x17, 0xf8, 0x25, 0x60,
	    0xf5, 0x0b, 0x17, 0xbc, 0xba, 0x5d, 0x1a, 0xb9,
	    0x0b, 0x6d, 0xbc, 0xbd, 0xd4, 0x4b, 0x39, 0x21,
	    0xcf, 0xd9, 0x9f, 0xed, 0xe4, 0x64, 0xea, 0x4b,
	]));
    }
}

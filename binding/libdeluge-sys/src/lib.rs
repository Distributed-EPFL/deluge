extern crate libc;


use libc::{c_int, c_void};
use std::option::Option;


pub const DELUGE_SUCCESS: c_int = 0;
pub const DELUGE_FAILURE: c_int = -1;
pub const DELUGE_NODEV: c_int = -2;
pub const DELUGE_NOMEM: c_int = -3;
pub const DELUGE_CANCEL: c_int = -4;


#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct dispatch {
    _unused: [u8; 0],
}

#[allow(non_camel_case_types)]
pub type dispatch_t = *mut dispatch;


extern "C" {
    pub fn deluge_init() -> c_int;

    pub fn deluge_finalize();

    pub fn deluge_dispatch_destroy(dispatch: dispatch_t);

    pub fn deluge_hashsum64_schedule(
	dispatch: dispatch_t,
	elems: *const u64,
	nelem: usize,
	cb: Option<
	    unsafe extern "C" fn (
		status: c_int,
		result: *mut u8,
		user: *const c_void,
	    ),
	    >,
	user: *const c_void,
    ) -> c_int;

    pub fn deluge_hashsum64_blake3_create(
	dispatch: *mut dispatch_t,
	key: *const u8
    ) -> c_int;
}

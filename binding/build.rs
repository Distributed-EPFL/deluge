use std::{
    env,
    process::Command,
    string::String,
};


fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    let mut cflags = String::from("CFLAGS=");
    let mut ldflags = String::from("LDFLAGS=");

    if let Ok("false") = env::var("DEBUG").as_deref() {
	cflags.push_str(" -DNDEBUG");
    }

    if let Ok(opt) = env::var("OPT_LEVEL") {
	if let Ok(lvl) = opt.parse::<u8>() {
	    if lvl >= 1 {
		cflags.push_str(" -O2 -mtune=generic");
	    }
	    if lvl >= 3 {
		cflags.push_str(" -flto");
		ldflags.push_str(" -flto");
	    }
	}
    }

    let makeopts = vec![ "-C", "..", "clean", "all", &cflags, &ldflags ];

    Command::new("make")
	.args(&makeopts)
	.status()
	.expect("failed to make!");

    println!("cargo:rustc-link-search=../lib");
    println!("cargo:rustc-link-lib=static=deluge");
    println!("cargo:rustc-link-lib=dylib=OpenCL");
}

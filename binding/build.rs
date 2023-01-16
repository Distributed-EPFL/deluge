use std::{
    env,
    process::Command,
    string::String,
};


fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=../Makefile");
    println!("cargo:rerun-if-changed=../deluge");

    let mut cflags = String::from("CFLAGS=");
    let mut ldflags = String::from("LDFLAGS=");

    if let Ok("false") = env::var("DEBUG").as_deref() {
	cflags.push_str(" -DNDEBUG");
    }

    let archflags = match env::var("CARGO_CFG_TARGET_ARCH") {
	Ok(arch) => format!("-march={}", arch.replace("_", "-")),
	_ => String::from("-mtune=generic"),
    };

    if let Ok(opt) = env::var("OPT_LEVEL") {
	if let Ok(lvl) = opt.parse::<u8>() {
	    if lvl >= 1 {
		cflags.push_str(format!(" -O2 {}", archflags).as_str());
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

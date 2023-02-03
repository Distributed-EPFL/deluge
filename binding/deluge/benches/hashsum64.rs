use criterion::{
    BenchmarkId,
    Criterion,
    criterion_group,
    criterion_main,
};
use deluge::hashsum64::Dispatch;
use tokio::runtime::Runtime;


fn blake3(c: &mut Criterion) {
    let rt = Runtime::new().unwrap();
    let dispatch = Dispatch::new_blake3(&[0; 32]).unwrap();

    for s in [ 1, 32, 128, 1024, 4096, 16384, 65536, 131072, 262144 ] {
	c.bench_with_input(BenchmarkId::new("batch-size", s), &s, |b, &s| {
	    let batch = (0 .. s).collect::<Vec<u64>>();

            b.to_async(&rt).iter(|| dispatch.schedule(batch.as_slice()));
	});
    }
}


criterion_group!(benches, blake3);
criterion_main!(benches);

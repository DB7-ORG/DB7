sudo apt-get install libabsl-dev

# DB7 ✅ ❌

implement compression schemes ❌
- dictionary encoding ✅
- optimize dictionary encoding more ✅
- run len encoding / one value ✅
- bitpacking ✅
- fsst ✅
- frequency encoding ✅
- add floating point compressions ❌
- support for NULLS ✅

check out todo template for decode single in bitpacking ❌
check out todos and fix them before there is too many ❌

improvements to existing decompressions
- add overflow when decoding rle ✅
- check out btrblocks dict simd gather approach ✅
- wtf is this _mm256_cvtepi32_pd, _mm256_mul_pd ✅
- roaring bitmaps ❌
- seprate bitpacking from dict ✅
- fuse rle and dict decomperssion to avoid intermediate array ❌
- optimize append only dictionary for integer and double types ✅
- bencharked something similar to swiss dictionary and my scalar version ✅

- refactor code for compression so function caller does the allocations and alignment ✅
- single value compression probably sucks try block decompression ❌

# RESEARCH

- better understand performance difference between these approaches ✅

//  entries = (MapEntry *)calloc(hash_capacity, sizeof(MapEntry));

// (MapEntry *)mmap(
//     NULL,
//     hash_capacity * sizeof(MapEntry),
//     PROT_READ | PROT_WRITE,
//     MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, // Pre-fault pages
//     -1, 0);

// entries = (MapEntry *)malloc(hash_capacity * sizeof(MapEntry));
// memset(entries, 0, hash_capacity * sizeof(MapEntry));

- read paper for fsst https://raw.githubusercontent.com/cwida/fsst/master/fsstcompression.pdf ✅
- read implementation https://github.com/cwida/fsst/blob/master/fsst.h ✅

# BENCHMARK

- decode single in bitpacking ✅

git clone https://github.com/google/benchmark.git
cd benchmark
cmake -E make_directory build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DBENCHMARK_DOWNLOAD_DEPENDENCIES=ON \
      -DBENCHMARK_ENABLE_TESTING=OFF \
      -S . -B build
cmake --build build --config Release -j$(nproc)
sudo cmake --build build --config Release --target install
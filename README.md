sudo apt-get install libabsl-dev
g++ -O3 -march=native -S -masm=intel atest.cpp -o template.s -fverbose-asm

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
- patched FOR ❌

check out todo template for decode single in bitpacking ❌
check out todos and fix them before there is too many ❌

improvements to existing decompressions
- add overflow when decoding rle ✅
- check out btrblocks dict simd gather approach ✅
- wtf is this _mm256_cvtepi32_pd, _mm256_mul_pd ✅
- roaring bitmaps ✅ more detailed reading ❌
- seprate bitpacking from dict ✅
- fuse rle and dict decomperssion to avoid intermediate array ❌
- optimize append only dictionary for integer and double types ✅
- bencharked something similar to swiss dictionary and my scalar version ✅
- python code generation that manually writes bitpacking code for all uX types ✅// TODO replace w common

- refactor code for compression so function caller does the allocations and alignment ✅
- single value compression probably sucks try block decompression ❌

- add __restrict to all compressions ❌

framework
- should always bitpack codes after dictionary encoding (check out sorted dict alternatives) ❌
- templated pfor version ❌ 
- i might want to apply something else on exceptions in pfor (might be a bad idea because its shifted bits which is semi random values) ❌
- add a dictionary that is a single bitmap so collisions are less punishable (might be a bad idea) ❌
- estimate pfor size and evaluate that to see how well does the estimate predict ✅
- move all compressions to coresponding hpp file ✅
- validate other compression estimates and move estimate functions to compressions ✅
- AlignedSTLAllocator is unsafe for pfor 1'000'000 entries ✅
(std::vector<u8> bytescontainer; was the problem) should decide what size to use ❌
- can i change stats after an estimation ❌
- why do i get UNDERESTIMATE in [exceptions at 3 distinct bit widths] ❌
- optimize dict in stat collecting it takes almost all of the time overhead ❌
- TODO this is not handling a case where i have really large string that is repeaded many times
    // and rest of small strings. This can be problematic when doing dictionary encoding and trying
    // to estimate the size.
    // One way to handle this to build a hash map where we only compare 64 bit hash values and act like there
    // is no different values w same hash to avoid memcpy which should be as fast as integer statistics.
    // Then later calculate EV (might be better approaches)


- change pfor scalar packing exceptions is awful ❌

OPTIONAL
- reqrite bitpacking using fancy recursive templates ❌

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
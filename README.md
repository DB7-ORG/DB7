sudo apt-get install libabsl-dev

# DB7 ✅ ❌

implement compression schemes ❌
- dictionary encoding ✅ ❌
- optimize dictionary encoding more ❌
- run len encoding / one value ❌
- frequency encoding ❌

# RESEARCH

- better understand performance difference between these approaches

//  entries = (MapEntry *)calloc(hash_capacity, sizeof(MapEntry));

// (MapEntry *)mmap(
//     NULL,
//     hash_capacity * sizeof(MapEntry),
//     PROT_READ | PROT_WRITE,
//     MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, // Pre-fault pages
//     -1, 0);

// entries = (MapEntry *)malloc(hash_capacity * sizeof(MapEntry));
// memset(entries, 0, hash_capacity * sizeof(MapEntry));

- read paper for fsst https://raw.githubusercontent.com/cwida/fsst/master/fsstcompression.pdf
- read implementation https://github.com/cwida/fsst/blob/master/fsst.h

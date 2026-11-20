# Commands

to install clangd -> install clangd extension and disable intelisense in vsc

to run perf -> perf report -i perf.data -f

to bear -> sudo apt install bear
make clean
bear -- make

to raise the limit for max memory reserved -> sudo prlimit --memlock=unlimited:unlimited --pid $$

# Implement features:
  - thread safety for file descriptors
  - i dont like fsm hacking should plan it first

# Index:
  - check out when encoding utf8proc_decompose
    so i have more controle and less allocations
  - i dont like storing root in metadata page of the index


# Index creation:

  IDEAS FOR WAYS TO MAKE CATALOG WORK ON DISK (NOT LIKE DUCKDB PERSISTING CHANGES ON COMMIT):

  1. When bootstrapping the database read from disk and create all catalog objects making the
    catalog be completely in memory. Catalog entry will have its oid and pageId and idx in the page.
    Every drop will mark entry as deleted in memory and then go and make changes to disk. We need
    to make sure when undo runs that we remove deleted flag in memory in case of abort. We can do
    that by adding the raw pointer to entry so we can mark it as invalid.
    Create will work in a similar way.
  2. More complex design involves not fetching anything into memory untill the entry is needed by
    some transaction. For now its just extra complexity and we can ignore this.
  3. Make the catalog in memory only where changes in it will be logged to WAL and later by some
    background thread will be persisted to disk. So there isnt any overhead for the user thread,
    but there is more preassure on the background workers. This would require seperate enty in
    undo log so when we rollback we know the entry is used for catalog in memory and we need to
    do different logic compared to page approach.

  online index creation:

  - publish index in BUILDING state
  - start scanning the heap
  - when we get a tuple:
    (tuple commited) add it to index
    (tuple not commited) add it to index, get next value in chain then repeat
  - GC will later collect aborted values
  - plan how to populate index safely
  - see if u should abort running writers if create index runs concurrently

# Transactions:
  - TODO transaction Try()

# PAX && DATA TABLE && FSM:
  - TODO storage we dont have error handling for is writable

# Catalog:
  - fix catalog to store data to a file
  - solve // TODO catalog

# MVCC version manager:
  - consider lock free hmaps for partitions

# Buffer pool:
  - background writer
  - disk err handling
  - // TODO ClearIOInProgress() && ClearWriteInProgress() && SetError;
  - central memory manager component in buffer pool

# Disk scheduler:
  - track how many pages are in fligh for each transaction and make it yield for a while untill the q empties
    (added size aprox for a producer use it)

# Task scheduler:
  - rewrite stream to not pop the task right away from the ring                       // DONT_NEED_NOW dont really need it yet might add later
  - figure out a way to do non blocking io in scheduler (seperate component ideally)  // IN PROGRESS
  - // TODO does this virtual cost add up                                             // DONT_NEED_NOW Should benchmark 
  
# Data table:
  - // TODO handle constant vectors
  - redesign columns chunk                                                            // DO_FIRST

# Buffer pool scheduling mechanism:
  - need to check parking in buffer pool
  - TODO remove this
  - waiter list, how to send mutex and io_cv
  - parking lot for pages

# Execution 
  - read some papers about vectorization and compilation


# Papers

**1. How to structure the compiler (theory of code generation)**
- **Rompf & Amin, "Functional Pearl: A SQL to C Compiler in 500 Lines of Code" (ICFP 2015).** It shows that a query compiler is just a staged interpreter (the first Futamura projection). Short, and probably the best single thing to read after HyPer.
- **Tahboub, Essertel, Rompf, "How to Architect a Query Compiler, Revisited" (SIGMOD 2018).** The LB2 system: the same idea at full scale.
- **Shaikhha et al., "How to Architect a Query Compiler" (SIGMOD 2016)** and **Klonatos et al., "Building Efficient Query Engines in a High-Level Language" (LegoBase, VLDB 2014).** These lower a query through a stack of DSLs in several steps instead of generating code in one pass.

**2. Compiled vs. vectorized execution**
- **Sompolski, Zukowski, Boncz, "Vectorization vs. Compilation in Query Execution" (DaMoN 2011).** The original comparison.
- **Kersten et al., "Everything You Always Wanted to Know About Compiled and Vectorized Queries But Were Afraid to Ask" (VLDB 2018).** Required reading.
- **Menon, Mowry, Pavlo, "Relaxed Operator Fusion" (VLDB 2018).** Adds staging points inside a compiled pipeline so prefetching and SIMD become possible.
- **Gubner & Boncz, "Charting the Design Space of Query Execution using VOILA" (VLDB 2021).**
- **InkFuse / "Incremental Fusion" (Wagner, Kohn, Neumann).** It unifies the two approaches and measures itself against DuckDB and Umbra, with competitive performance for low-latency processing.
- **Photon (Databricks, SIGMOD 2022).** Explains why a big production team chose vectorization over code generation. A useful counterweight.

**3. Compilation latency (the most active topic right now)**
- **Kohn, Leis, Neumann, "Adaptive Execution of Compiled Queries" (ICDE 2018).** Starts by interpreting the generated IR, then switches to compiled code per morsel once it is ready.
- **Kersten, Leis, Neumann, "Tidy Tuples and Flying Start" (VLDB Journal 2021).** Covers all stages of compiling query execution engines and how to reduce compilation overhead, using Umbra's own IR and a fast backend that skips LLVM.
- **Neumann, "Evolution of a Compiling Query Engine" (VLDB 2021).** A short retrospective on HyPer → Umbra and what changed.
- **Funke et al., Flounder IR / ReSQL (VLDB Journal 2022).** Swaps the general-purpose LLVM IR for a database-specific IR and reports speedups up to 5.5× over HyPer's LLVM-based compiler.
- **Gruber et al., FireARM.** A low-latency backend for Umbra, plus an evaluation of code generation approaches across different architectures.
- **Xu & Kjolstad, "Copy-and-Patch Compilation" (OOPSLA 2021).** Not a database paper, but the stencil-based JIT technique fits query engines very well (CPython's JIT uses it too).

**4. Building on newer compiler infrastructure (MLIR and others)**
- **Jungmair, Kohn, Giceva, LingoDB (VLDB 2022).** Query optimization and compilation done as MLIR dialects. A good one if you want to go beyond LLVM IR.
- **Ladner, Döllerer, Engelke, "Maintainable, Low-Latency and High-Quality Query Compilation with MLIR and TPDE" (ADMS @ VLDB 2026).** MLIR engines pay 50–150 ms of compilation for typical analytical SQL queries. Their TPDE backend generates machine code up to 66x faster, even outperforming Hyper's compilation backend.
- **Grulich et al., Nautilus: "Towards Unifying Query Interpretation and Compilation" (CIDR 2023) and the SIGMOD 2024 follow-up.** The core idea is a trace-based, multi-backend JIT that bridges easy engine engineering and high-speed execution. You write the operators as interpreters, and they get traced into code.
- **Haffner & Dittrich, mutable (EDBT 2023).** Compiles to WebAssembly and lets V8's tiered JIT handle adaptivity.
- **Weld (CIDR 2017) and Voodoo (VLDB 2016).** Intermediate languages that sit between relational algebra and the hardware.

**5. Production practice**
- **FastCompose (PVLDB vol. 19, 2026, from the Redshift team).** It tackles compilation cold starts with "composition", so that no query waits for g++ compilation. A good look at how this is handled in a real system that compiles to C++.

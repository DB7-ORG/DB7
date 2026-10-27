in buffer pool i can hold partition lock while checking the page and if its a stale entry i can remove it right away without starting from scratch!!!!
much better than current design?????

thread safety for file descriptors

- Consider vectorised read

# i dont like Reserve in buffer pool

# i dont like storing root in metadata page of the index

# i dont like fsm hacking should plan it first

perf report -i perf.data -f

TODO
https://www.cs.cit.tum.de/fileadmin/w00cfj/dis/papers/btrees-are-back.pdf

- check out when encoding utf8proc_decompose
  so i have more controle and less allocations

- understand nomove_if

- consider hyper delete w garbage collection.

see should i use cas loop to delete stuff since i need to keep that info in the page also so GC can later remove tombstne
but gc can also add it later to a page which also seems convenient

NOTE:

install clangd extension and disable intelisense in vsc

sudo apt install bear
make clean
bear -- make

solve // TODO catalog

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

Data table:
- fix catalog mvcc to save in undo buffer

//TODO fsm assumes u have only one idx_t in header

 <!-- static LogRecord *Initialize(byte *const head, const timestamp_t txn_begin, table_id t_id,
                               page_id p_id, u32 idx) -->

- plan how to populate index safely 
- see if u should abort running writers if create index runs concurrently
- TODO transaction Try()

- TODO storage we dont have error handling for is writable(should also add fsm)

online index creation:
- publish index in BUILDING state
- start scanning the heap
- when we get a tuple:
  (tuple commited)      add it to index
  (tuple not commited)  add it to index, get next value in chain then repeat
- GC will later collect aborted values 

 this command raises the limit for max memory reserved 
 sudo prlimit --memlock=unlimited:unlimited --pid $$ 

- fix index table id being 0

- lower this limit to 1 after testing
  if (!disk_mng_.CreateOpenFile(oid_, 10'000)) { throw IO_EXCEPTION("Could not create/open file"); }

- vectorized preadv and pwritev in iouring
- implement ^
- refactor files to not be header only
- figure out access strategy
- figure out deadlock scenarios w ring buffers strategy
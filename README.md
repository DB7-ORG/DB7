# Commands

to install clangd -> install clangd extension and disable intelisense in vsc

to run perf -> perf report -i perf.data -f

to bear ->  sudo apt install bear
            make clean
            bear -- make

to raise the limit for max memory reserved -> sudo prlimit --memlock=unlimited:unlimited --pid $$

# Stuff i need to do

- thread safety for file descriptors

- i dont like storing root in metadata page of the index

- i dont like fsm hacking should plan it first

- check out when encoding utf8proc_decompose
  so i have more controle and less allocations


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

online index creation:

- publish index in BUILDING state
- start scanning the heap
- when we get a tuple:
  (tuple commited) add it to index
  (tuple not commited) add it to index, get next value in chain then repeat
- GC will later collect aborted values

- plan how to populate index safely
- see if u should abort running writers if create index runs concurrently
- TODO transaction Try()

- TODO storage we dont have error handling for is writable

Data table:

- fix catalog mvcc to save in undo buffer

 <!-- static LogRecord *Initialize(byte *const head, const timestamp_t txn_begin, table_id t_id,
                               page_id p_id, u32 idx) -->

- consider lock free hmaps for partitions

TODO clone projetc and benchmark it with and without mutexes
git clone https://github.com/DB7-ORG/DB7.git
cd DB7
git checkout a824245abac0985ae4573e829a6552570d44bf34

-compare old and new wakeup mechanisms

- background writer
- disk err handling
- // TODO ClearIOInProgress() && ClearWriteInProgress() && SetError;
- central memory manager component in buffer pool

- error handling in the scheduler
- rewrite stream to not pop the task right away from the ring
- figure out a way to do non blocking io in scheduler (seperate component ideally)
- figure out a way to store local state in a task
- figure out ownerships for a task
- parking lot for pages 

- track how many pages are in fligh for each transaction and make it yield for a while untill the q empties
 (added size aprox for a producer use it)
- waiter list, how to send mutex and io_cv
- ownership for Task and Executor
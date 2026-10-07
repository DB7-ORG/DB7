in buffer pool i can hold partition lock while checking the page and if its a stale entry i can remove it right away without starting from scratch!!!!
much better than current design?????

figure out how to deserialize catalog entries (read)

thread safety for file descriptors

* Consider vectorised read

# i dont like Reserve in buffer pool 
# i dont like storing root in metadata page of the index 
# i dont like fsm hacking should plan it first


perf report -i perf.data -f

add some tests after to guarantee ur btree works

// TODO CATALOG UNCOMMENT


TODO 
https://www.cs.cit.tum.de/fileadmin/w00cfj/dis/papers/btrees-are-back.pdf
* Head optimizations seems easy to implement

* allocators can be added later

* check out when encoding utf8proc_decompose
  so i have more controle and less allocations
* make varlen more compatible

* understand nomove_if

* consider hyper delete w garbage collection.

// TODO fix index
fix this

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



Need a new Table abstraction
- should be a chain of DataTables for versioning
- shoudl have a way to easily access its schema (should contain vector of column description like in catalog)
- should contain indexes and manage their state along with the heap tuples along with checking foreign keys
- should be compatible with DataChunk so its easy to generate and modify it

- need to add number of columns to pax layout so add column/remove works
- fix catalog mvcc to save in undo buffer
- add table abstraction

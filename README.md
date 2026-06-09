
figure out how to structure ProjectedRowsBuilder!!!!

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
* take a look at new EncodeFields
* make varlen more compatible
* figure out what to do about mvcc

* tbb::spin_mutex latch_; consider this latch

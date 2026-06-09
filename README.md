
figure out how to structure ProjectedRowsBuilder!!!!

in buffer pool i can hold partition lock while checking the page and if its a stale entry i can remove it right away without starting from scratch!!!!
much better than current design?????

figure out how to deserialize catalog entries (read)

figure out how to manage errors for disk and other

thread safety for file descriptors

* Consider vectorised read

# i dont like Reserve in buffer pool 
# i dont like storing root in metadata page of the index 
# i dont like fsm hacking should plan it first


perf report -i perf.data -f




refactor exisitng split for varlen

add some tests after to guarantee ur btree works

// TODO CATALOG UNCOMMENT


TODO 
https://www.cs.cit.tum.de/fileadmin/w00cfj/dis/papers/btrees-are-back.pdf
* Head optimizations seems easy to implement

* check to see what happens w duplicate data in the index and how it holds

* allocators can be added later

* check out when encoding utf8proc_decompose
* so i have more controle and less allocations
* take a look at new EncodeFields
* make varlen more compatible 
* consider moving layout to storage
* schema should not calc offsets i should create a layout for pax
* figure out what to do about mvcc

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

* fix valgrind later

* need to add different parsing of slots when its max len and prefix handling

* when i get the bug i get invalid prefix len for some reason
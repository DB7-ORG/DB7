
figure out how to structure ProjectedRowsBuilder!!!!

in buffer pool i can hold partition lock while checking the page and if its a stale entry i can remove it right away without starting from scratch!!!!
much better than current design?????

figure out how to deserialize catalog entries (read)

figure out how to manage errors for disk and other

thread safety for file descriptors

* clock sync
    sudo ntpdate pool.ntp.org

* Consider vectorised read

# i dont like Reserve in buffer pool 
# i dont like storing root in metadata page of the index 
# i dont like fsm hacking should plan it first
# i dont like that index isnt flexible for now 

thread_local page_id tl_state_buf[16];
thread_local u32 tl_state_size = 0;

perf report -i perf.data -f
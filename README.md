
figure out how to structure ProjectedRowsBuilder!!!!

in buffer pool i can hold partition lock while checking the page and if its a stale entry i can remove it right away without starting from scratch!!!!
much better than current design?????

redesign in tla+ buffer pool with io in progress flag
and refactor code or tla to match the names
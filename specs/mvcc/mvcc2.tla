---- MODULE mvcc ----
EXTENDS Integers, Sequences, FiniteSets, TLC

CONSTANTS NUM_THREADS, OPERATIONS_COUNT, NUM_DISK_PAGES, NUM_POOL_PAGES

VARIABLES disk, buffer_pool, delta_hm, timestamp, undo_buffer, value, page_id, txn_ts, count, page, pc

vars == <<disk, buffer_pool, delta_hm, timestamp, undo_buffer, value, page_id, txn_ts, count, page, pc>>

Threads == 1..NUM_THREADS
Ops     == {"read", "insert", "update", "delete"}

NULL_DELTA == [thread |-> 0, idx |-> 0]
Ptr(t, i)  == [thread |-> t, idx |-> i]

InPool(pid) == \E s \in 1..NUM_POOL_PAGES : buffer_pool[s].pageId = pid
InDisk(pid) == \E s \in 1..NUM_DISK_PAGES : disk[s].pageId = pid
SlotOf(pid) == CHOOSE i \in 1..NUM_POOL_PAGES : buffer_pool[i].pageId = pid

Init ==
    /\ disk        = [p \in 1..NUM_DISK_PAGES |-> [values |-> 0]]
    /\ buffer_pool = [s \in 1..NUM_POOL_PAGES |-> [pageId |-> 0, values |-> 0, delta |-> NULL_DELTA]]
    /\ delta_hm    = [p \in 1..NUM_DISK_PAGES |-> NULL_DELTA]
    /\ timestamp   = 1
    /\ undo_buffer = [t \in Threads |-> <<>>]
    /\ value       = [t \in Threads |-> 0]
    /\ page_id     = [t \in Threads |-> 0]
    /\ txn_ts      = [t \in Threads |-> 0]
    /\ count       = [t \in Threads |-> 0]
    /\ page        = [t \in Threads |-> [pageId |-> 0, values |-> 0, delta |-> NULL_DELTA]]
    /\ pc          = [t \in Threads |-> "Start"]

Start(t) ==
    /\ pc[t] = "Start"
    /\ value'     = [value  EXCEPT ![t] = t]
    /\ txn_ts'    = [txn_ts EXCEPT ![t] = timestamp]
    /\ timestamp' = timestamp + 1
    /\ pc'        = [pc EXCEPT ![t] = "Pick"]
    /\ UNCHANGED <<disk, buffer_pool, delta_hm, undo_buffer, page_id, count, page>>

Pick(t) ==
    /\ pc[t] = "Pick"
    /\ \E pid \in 1..NUM_DISK_PAGES, op \in Ops :
         /\ page_id' = [page_id EXCEPT ![t] = pid]
         /\ value'   = [value   EXCEPT ![t] = value[t] + 1000]
         /\ pc'      = [pc EXCEPT ![t] =
              CASE op = "read"   -> "Read"
                [] op = "insert" -> "Insert"
                [] op = "update" -> "Update"
                [] op = "delete" -> "Delete"]
    /\ pc'        = [pc EXCEPT ![t] = op]
    /\ UNCHANGED <<disk, buffer_pool, delta_hm, timestamp, undo_buffer, txn_ts, count, page>>

DeltaHMGet(pid) == delta_hm[pid]

DeltaHMSet(pid, delta) == delta_hm' = [delta_hm EXCEPT ![pid] = delta]

Read(t) ==
    /\ pc[t] = "Read"
    /\ \/ InPool(page_id[t])
       \/ /\ InDisk(page_id[t])
          /\ \E victim \in 1..NUM_POOL_PAGES :
            /\ disk' = [disk EXCEPT ![buffer_pool[victim].pageId].values = buffer_pool[victim].values]
            /\ buffer_pool' = [buffer_pool EXCEPT ![victim] = [
                                                pageId |-> page_id[t],
                                                values |-> disk[page_id[t]].values,
                                                delta  |-> delta_hm[page_id[t]]]]
            /\ delta_hm' = [delta_hm EXCEPT ![page_id[t]] = disk[buffer_pool[victim].pageId]]
    /\ pc'        = [pc EXCEPT ![t] = "IncCount"]
    /\ UNCHANGED <<timestamp, undo_buffer, value, page_id, txn_ts, count>>

Insert(t) == 
    /\ pc[t] = "Insert"
    /\ !InPool(page_id[t])
    /\ !InDisk(page_id[t])
    /\ \E victim \in 1..NUM_POOL_PAGES :
       /\ disk' = [disk EXCEPT ![buffer_pool[victim].pageId].values = buffer_pool[victim].values]
       /\ buffer_pool' = [buffer_pool EXCEPT ![victim] = [
                                                pageId |-> page_id[t],
                                                values |-> value,
                                                delta  |-> NULL_DELTA]]
       /\ delta_hm' = [delta_hm EXCEPT ![page_id[t]] = disk[buffer_pool[victim].pageId]]
    /\ pc'        = [pc EXCEPT ![t] = "IncCount"]
    /\ UNCHANGED <<timestamp, undo_buffer, value, page_id, txn_ts, count>>


Update(t) == 
    /\ pc[t] = "Update"
    /\ \/ /\ InPool(page_id[t])
          /\ buffer_pool' = [buffer_pool EXCEPT ![SlotOf(page_id[t])] = [
                                                pageId |-> page_id[t],
                                                values |-> value,
                                                delta  |-> Ptr(t,Len(undo_buffer[t]) + 1) ]]
          /\ LET rec    == [    type      |-> "update",
                                timestamp |-> txn_ts[t],
                                pid       |-> page_id[t],
                                next      |-> buffer_pool[SlotOf(page_id[t])].delta,
                                value     |-> buffer_pool[SlotOf(page_id[t])].values ]   
             IN undo_buffer' = [undo_buffer EXCEPT ![t] = Append(@, rec)]
          /\ UNCHANGED <<disk, delta_hm>>
       \/ /\ InDisk(page_id[t])
          /\ \E victim \in 1..NUM_POOL_PAGES :
            /\ disk' = [disk EXCEPT ![buffer_pool[victim].pageId].values = buffer_pool[victim].values]
            /\ buffer_pool' = [buffer_pool EXCEPT ![victim] = [
                                                pageId |-> page_id,
                                                values |-> value,
                                                delta  |-> Ptr(t,Len(undo_buffer[t]) + 1)]]
            /\ delta_hm' = [delta_hm EXCEPT ![buffer_pool[victim].pageId] = buffer_pool[victim].delta]
            /\ LET rec    == [  type      |-> "update",
                                timestamp |-> txn_ts[t],
                                pid       |-> page_id[t],
                                next      |-> delta_hm[page_id[t]],
                                value     |-> disk[page_id[t]].values ]   
               IN undo_buffer' = [undo_buffer EXCEPT ![t] = Append(@, rec)]
    /\ pc'        = [pc EXCEPT ![t] = "IncCount"]
    /\ UNCHANGED <<timestamp, value, page_id, txn_ts, count>>

Delete(t) == 
    /\ pc[t] = "Delete"
    \/ /\ InPool(page_id[t])
          /\ buffer_pool' = [buffer_pool EXCEPT ![SlotOf(page_id[t])] = [
                                                pageId |-> page_id[t],
                                                values |-> 0,
                                                delta  |-> Ptr(t,Len(undo_buffer[t]) + 1) ]]
          /\ LET rec    == [    type      |-> "delete",
                                timestamp |-> txn_ts[t],
                                pid       |-> page_id[t],
                                next      |-> buffer_pool[SlotOf(page_id[t])].delta,
                                value     |-> buffer_pool[SlotOf(page_id[t])].values ]   
             IN undo_buffer' = [undo_buffer EXCEPT ![t] = Append(@, rec)]
          /\ UNCHANGED <<disk, delta_hm>>
       \/ /\ InDisk(page_id[t])
          /\ \E victim \in 1..NUM_POOL_PAGES :
            /\ disk' = [disk EXCEPT ![buffer_pool[victim].pageId].values = buffer_pool[victim].values]
            /\ buffer_pool' = [buffer_pool EXCEPT ![victim] = [
                                                pageId |-> page_id,
                                                values |-> value,
                                                delta  |-> Ptr(t,Len(undo_buffer[t]) + 1)]]
            /\ delta_hm' = [delta_hm EXCEPT ![buffer_pool[victim].pageId] = buffer_pool[victim].delta]
            /\ LET rec    == [  type      |-> "update",
                                timestamp |-> txn_ts[t],
                                pid       |-> page_id[t],
                                next      |-> delta_hm[page_id[t]],
                                value     |-> disk[page_id[t]].values ]   
               IN undo_buffer' = [undo_buffer EXCEPT ![t] = Append(@, rec)]
    /\ pc'        = [pc EXCEPT ![t] = "IncCount"]
    /\ UNCHANGED <<timestamp, value, page_id, txn_ts, count>>

IncCount(t) ==
    /\ pc[t] = "IncCount"
    /\ count' = [count EXCEPT ![t] = count[t] + 1]
    /\ pc' = [pc EXCEPT ![t] =
         IF count[t] + 1 = OPERATIONS_COUNT THEN "Done" ELSE "PickOperation"]
    /\ UNCHANGED <<disk, buffer_pool, delta_hm, timestamp, undo_buffer, value, page_id, txn_ts, page>>

Done ==
    /\ \A t \in Threads : pc[t] = "Done"
    /\ UNCHANGED vars

Next ==
    \/ \E t \in Threads :
         \/ Start(t)  \/ Pick(t)    \/ Read(t)   \/ Insert(t)
         \/ Update(t) \/ Delete(t)  \/ IncCount(t)
    \/ Done

Spec == Init /\ [][Next]_vars

Termination == <>(\A t \in Threads : pc[t] = "Done")

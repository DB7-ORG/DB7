---- MODULE mvcc2 ----
EXTENDS Integers, Sequences, FiniteSets, TLC

CONSTANTS NUM_THREADS, OPERATIONS, OPERATIONS_COUNT, NUM_DISK_PAGES, NUM_POOL_PAGES

VARIABLES disk, buffer_pool, delta_hm, timestamp, undo_buffer, value, page_id, txn_ts, count, read_set, pc

vars == <<disk, buffer_pool, delta_hm, timestamp, undo_buffer, value, page_id, txn_ts, count, read_set, pc>>

Threads == 1..NUM_THREADS

UNCOMMITTED == 1000000

EMPTY == 0

TOMBSTONE == -1

IsUncommitted(ts) == ts >= UNCOMMITTED

TxnIdOf(ts) == ts - UNCOMMITTED

UncommitedTs(ts) == ts + UNCOMMITTED

NULL_DELTA == [thread |-> EMPTY, idx |-> EMPTY]
Ptr(t, i)  == [thread |-> t, idx |-> i]

InPool(pid) == \E s \in 1..NUM_POOL_PAGES : buffer_pool[s].pageId = pid
InDisk(pid) == disk[pid].values /= EMPTY
SlotOf(pid) == CHOOSE i \in 1..NUM_POOL_PAGES : buffer_pool[i].pageId = pid

Init ==
    /\ disk        = [p \in 1..NUM_DISK_PAGES |-> [values |-> EMPTY]]
    /\ buffer_pool = [s \in 1..NUM_POOL_PAGES |-> [pageId |-> EMPTY, values |-> EMPTY, delta |-> NULL_DELTA]]
    /\ delta_hm    = [p \in 1..NUM_DISK_PAGES |-> NULL_DELTA]
    /\ timestamp   = 1
    /\ undo_buffer = [t \in Threads |-> <<>>]
    /\ value       = [t \in Threads |-> EMPTY]
    /\ page_id     = [t \in Threads |-> EMPTY]
    /\ txn_ts      = [t \in Threads |-> EMPTY]
    /\ count       = [t \in Threads |-> EMPTY]
    /\ read_set    = [t \in Threads |-> <<>>]
    /\ pc          = [t \in Threads |-> "Start"]

Start(t) ==
    /\ pc[t] = "Start"
    /\ value'     = [value  EXCEPT ![t] = t]
    /\ txn_ts'    = [txn_ts EXCEPT ![t] = timestamp]
    /\ timestamp' = timestamp + 1
    /\ pc'        = [pc EXCEPT ![t] = "Pick"]
    /\ UNCHANGED <<disk, buffer_pool, delta_hm, undo_buffer, page_id, count, read_set>>

Pick(t) ==
    /\ pc[t] = "Pick"
    /\ \E pid \in 1..NUM_DISK_PAGES, op \in OPERATIONS :
         /\ page_id' = [page_id EXCEPT ![t] = pid]
         /\ value'   = [value   EXCEPT ![t] = value[t] + 1000]
         /\ pc'      = [pc EXCEPT ![t] =
              CASE op = "read"   -> "Read"
                [] op = "insert" -> "Insert"
                [] op = "update" -> "Update"
                [] op = "delete" -> "Delete"]
    /\ UNCHANGED <<disk, buffer_pool, delta_hm, timestamp, undo_buffer, txn_ts, count, read_set>>

MoveToDisk(victim) == 
    IF buffer_pool[victim].pageId = EMPTY
    THEN disk
    ELSE [disk EXCEPT ![buffer_pool[victim].pageId].values = buffer_pool[victim].values]

MoveDeltaHead(victim) ==
    IF buffer_pool[victim].pageId = EMPTY
    THEN delta_hm
    ELSE [delta_hm EXCEPT ![buffer_pool[victim].pageId] = buffer_pool[victim].delta]

MoveToPool(t, victim, val, delta) == [buffer_pool EXCEPT ![victim] = [
                                                pageId |-> page_id[t],
                                                values |-> val,
                                                delta  |-> delta]]

GetUndoRecordSlot(t) == Ptr(t,Len(undo_buffer[t]) + 1) 

AppendUndoRecord(t, op, delta, val) == LET rec    == [   
                            type      |-> op,
                            timestamp |-> UncommitedTs(txn_ts[t]),
                            pid       |-> page_id[t],
                            next      |-> delta,
                            value     |-> val ]   
                      IN [undo_buffer EXCEPT ![t] = Append(@, rec)]

HeadOf(pid)   == IF InPool(pid) THEN buffer_pool[SlotOf(pid)].delta  ELSE delta_hm[pid]

LatestOf(pid) == IF InPool(pid) THEN buffer_pool[SlotOf(pid)].values ELSE disk[pid].values

RECURSIVE ChainWalk(_, _, _)
ChainWalk(ptr, cur_value, ts) == 
    IF ptr = NULL_DELTA
    THEN cur_value
    ELSE LET rec == undo_buffer[ptr.thread][ptr.idx]
         IN IF rec.timestamp <= ts \/ (IsUncommitted(rec.timestamp) /\  TxnIdOf(rec.timestamp) = ts)
            THEN cur_value
            ELSE ChainWalk(rec.next, rec.value, ts)

VisibleValue(pid, ts) == [
    val |-> ChainWalk(HeadOf(pid), LatestOf(pid), ts),
    pid |-> pid
]

ReadValue(t, pid, ts) == 
    LET val == VisibleValue(pid, ts)
    IN [read_set EXCEPT ![t] = Append(@, val)]

EmptyValue(t) == 
    LET v == [val |-> EMPTY, pid |-> page_id[t]]
    IN [read_set EXCEPT ![t] = Append(@, v)]

Read(t) ==
    /\ pc[t] = "Read"
    /\ \/ /\ InPool(page_id[t])
          /\ read_set' = ReadValue(t, page_id[t], txn_ts[t])
          /\ UNCHANGED <<disk, buffer_pool, delta_hm>>
       \/ /\ ~InPool(page_id[t]) /\ InDisk(page_id[t])
          /\ \E victim \in 1..NUM_POOL_PAGES :
             /\ disk' = MoveToDisk(victim)
             /\ buffer_pool' = MoveToPool(t, victim, disk[page_id[t]].values, delta_hm[page_id[t]])
             /\ delta_hm' = MoveDeltaHead(victim)
             /\ read_set' = ReadValue(t, page_id[t], txn_ts[t])
       \/ /\ ~InPool(page_id[t]) /\ ~InDisk(page_id[t])
          /\ read_set' = EmptyValue(t)
          /\ UNCHANGED <<disk, buffer_pool, delta_hm>>
    /\ pc'        = [pc EXCEPT ![t] = "IncCount"]
    /\ UNCHANGED <<timestamp, undo_buffer, value, page_id, txn_ts, count>>

Insert(t) == 
    /\ pc[t] = "Insert"
    /\ \/ /\ ~InPool(page_id[t]) /\ ~InDisk(page_id[t])
          /\ \E victim \in 1..NUM_POOL_PAGES :
             /\ disk' = MoveToDisk(victim)
             /\ buffer_pool' = MoveToPool(t, victim, value[t], GetUndoRecordSlot(t))
             /\ delta_hm' = MoveDeltaHead(victim)
             /\ undo_buffer' = AppendUndoRecord(t, "insert", NULL_DELTA, EMPTY)
       \/ /\ (InPool(page_id[t]) \/ InDisk(page_id[t]))
          /\ UNCHANGED <<disk, buffer_pool, delta_hm, undo_buffer>> 
    /\ pc'        = [pc EXCEPT ![t] = "IncCount"]
    /\ UNCHANGED <<timestamp, value, page_id, txn_ts, count, read_set>>

CanWrite(pid, ts) ==
    LET h == HeadOf(pid) IN
        IF h = NULL_DELTA THEN
            TRUE
        ELSE
            LET rec == undo_buffer[h.thread][h.idx] IN
                TxnIdOf(rec.timestamp) = ts
                \/ (~IsUncommitted(rec.timestamp)
                    /\ rec.timestamp <= ts)

UpdateReal(t) == 
    /\ \/ /\ InPool(page_id[t])
          /\ buffer_pool' = MoveToPool(t, SlotOf(page_id[t]), value[t], GetUndoRecordSlot(t))
          /\ undo_buffer' = AppendUndoRecord(t, "update", buffer_pool[SlotOf(page_id[t])].delta, buffer_pool[SlotOf(page_id[t])].values)
          /\ UNCHANGED <<disk, delta_hm>>
       \/ /\ ~InPool(page_id[t]) /\ InDisk(page_id[t])
          /\ \E victim \in 1..NUM_POOL_PAGES :
             /\ disk' = MoveToDisk(victim)
             /\ buffer_pool' = MoveToPool(t, victim, value[t], GetUndoRecordSlot(t))
             /\ delta_hm' = MoveDeltaHead(victim)
             /\ undo_buffer' = AppendUndoRecord(t, "update", delta_hm[page_id[t]], disk[page_id[t]].values)
       \/ /\ ~InPool(page_id[t]) /\ ~InDisk(page_id[t])
          /\ UNCHANGED <<disk, delta_hm, undo_buffer, buffer_pool>>
    /\ UNCHANGED <<timestamp, value, page_id, txn_ts, count, read_set>>

Update(t) == 
    /\ pc[t] = "Update"
    /\ \/ /\ CanWrite(page_id[t], txn_ts[t])
          /\ UpdateReal(t)
          /\ pc' = [pc EXCEPT ![t] = "IncCount"]
       \/ /\ ~CanWrite(page_id[t], txn_ts[t])
          /\ pc' = [pc EXCEPT ![t] = "Abort"]          
          /\ UNCHANGED <<disk, buffer_pool, delta_hm, timestamp, undo_buffer, value, page_id, txn_ts, count, read_set>>
    
DeleteReal(t) == 
    /\ \/ /\ InPool(page_id[t])
          /\ buffer_pool' = MoveToPool(t, SlotOf(page_id[t]), TOMBSTONE, GetUndoRecordSlot(t))
          /\ undo_buffer' = AppendUndoRecord(t, "delete", buffer_pool[SlotOf(page_id[t])].delta, buffer_pool[SlotOf(page_id[t])].values) 
          /\ UNCHANGED <<disk, delta_hm>>
       \/ /\ ~InPool(page_id[t]) /\ InDisk(page_id[t])
          /\ \E victim \in 1..NUM_POOL_PAGES :
             /\ disk' = MoveToDisk(victim)
             /\ buffer_pool' = MoveToPool(t, victim, TOMBSTONE, GetUndoRecordSlot(t))
             /\ delta_hm' = MoveDeltaHead(victim)
             /\ undo_buffer' = AppendUndoRecord(t, "delete", delta_hm[page_id[t]], disk[page_id[t]].values) 
       \/ /\ ~InPool(page_id[t]) /\ ~InDisk(page_id[t])
          /\ UNCHANGED <<disk, delta_hm, undo_buffer, buffer_pool>>
    /\ UNCHANGED <<timestamp, value, page_id, txn_ts, count, read_set>>

Delete(t) == 
    /\ pc[t] = "Delete"
    /\ \/ /\ CanWrite(page_id[t], txn_ts[t])
          /\ DeleteReal(t)
          /\ pc' = [pc EXCEPT ![t] = "IncCount"]
       \/ /\ ~CanWrite(page_id[t], txn_ts[t])
          /\ pc' = [pc EXCEPT ![t] = "Abort"]          
          /\ UNCHANGED <<disk, buffer_pool, delta_hm, timestamp, undo_buffer, value, page_id, txn_ts, count, read_set>>

IncCount(t) ==
    /\ pc[t] = "IncCount"
    /\ count' = [count EXCEPT ![t] = count[t] + 1]
    /\ pc' = [pc EXCEPT ![t] = IF count[t] + 1 = OPERATIONS_COUNT THEN "Done" ELSE "Pick"]
    /\ UNCHANGED <<disk, buffer_pool, delta_hm, timestamp, undo_buffer, value, page_id, txn_ts, read_set>>

WrotePages(t) == { undo_buffer[t][i].pid : i \in 1..Len(undo_buffer[t]) }

FirstRec(t, p) == LET i == CHOOSE i \in 1..Len(undo_buffer[t]) :
                             /\ undo_buffer[t][i].pid = p
                             /\ \A j \in 1..i-1 : undo_buffer[t][j].pid /= p
                  IN undo_buffer[t][i]

Abort(t) ==
    /\ pc[t] = "Abort"
    /\ buffer_pool' = [s \in 1..NUM_POOL_PAGES |->
           IF buffer_pool[s].pageId \in WrotePages(t)
           THEN [buffer_pool[s] EXCEPT !.values = FirstRec(t, buffer_pool[s].pageId).value,
                                       !.delta  = FirstRec(t, buffer_pool[s].pageId).next]
           ELSE buffer_pool[s]]
    /\ disk' = [p \in 1..NUM_DISK_PAGES |->
           IF p \in WrotePages(t) /\ ~InPool(p)
           THEN [values |-> FirstRec(t, p).value]
           ELSE disk[p]]
    /\ delta_hm' = [p \in 1..NUM_DISK_PAGES |->
           IF p \in WrotePages(t) /\ ~InPool(p)
           THEN FirstRec(t, p).next
           ELSE delta_hm[p]]
    /\ undo_buffer' = [undo_buffer EXCEPT ![t] = <<>>]
    /\ read_set'    = [read_set    EXCEPT ![t] = <<>>]
    /\ pc' = [pc EXCEPT ![t] = "Done"]
    /\ UNCHANGED <<timestamp, value, page_id, txn_ts, count>>

Commit(t) ==
    /\ pc[t] = "Commit"
    /\ 
    /\ pc' = [pc EXCEPT ![t] = "Done"]
    /\ UNCHANGED <<timestamp, value, page_id, txn_ts, count>>

Done ==
    /\ \A t \in Threads : pc[t] = "Done"
    /\ UNCHANGED vars

Next ==
    \/ \E t \in Threads :
         \/ Start(t)  \/ Pick(t)    
         \/ Read(t)   \/ Insert(t) \/ Update(t) \/ Delete(t)  
         \/ IncCount(t) \/ Abort(t)
    \/ Done

Spec == Init /\ [][Next]_vars /\ WF_vars(Next)

Termination == <>(\A t \in Threads : pc[t] = "Done")

====
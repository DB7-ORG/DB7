---- MODULE mvcc ----

EXTENDS Integers, Sequences, FiniteSets, TLC

CONSTANTS NUM_THREADS, OPERATIONS_COUNT, NUM_DISK_PAGES, NUM_POOL_PAGES

UncommitedFlag == 1000000

InPool(pid) == \E s \in 1..NUM_POOL_PAGES : buffer_pool[s].pageId = pid

DeltaHMGet(pid) == delta_hm[pid]

DeltaHMSet(pid, delta) == delta_hm' = [delta_hm EXCEPT ![pid] = delta]

SlotOf(pid) == CHOOSE i \in 1..NUM_POOL_PAGES : buffer_pool[i].pageId = pid

BufferPoolGet(pid) ==
    IF InPool(pid)
    THEN UNCHANGED <<disk, buffer_pool>>          \* hit
    ELSE \E victim \in 1..NUM_POOL_PAGES :
            /\ DeltaHMSet(pid, disk[victim]) 
            /\ disk' = [disk EXCEPT
                          ![buffer_pool[victim].pageId].values = buffer_pool[victim].values]
            /\ buffer_pool' = [buffer_pool EXCEPT
                          ![victim] = [pageId |-> pid,
                                       values |-> disk[pid].values,
                                       delta  |-> DeltaHMGet(pid)]]      

BeginTransaction() == 
    /\ txn_timestamp' = timestamp
    /\ timestamp' = timestamp + 1

DeltaType == {"UPDATE", "INSERT", "DELETE"}

NULL_DELTA   == [thread |-> 0, idx |-> 0]
Ptr(t, i) == [thread |-> t, idx |-> i]

AppendUndo(t, rtype, ts, pid, val) ==
    LET newIdx == Len(undo_buffer[t]) + 1
        rec    == [ type      |-> rtype,
                    timestamp |-> ts,
                    pid       |-> pid,
                    next      |-> delta_hm[pid],
                    value     |-> val ]   \* old head becomes this record's next
    IN  undo_buffer' = [undo_buffer EXCEPT ![t] = Append(@, rec)]

(*--algorithm btree_index

variables
    disk = [p \in 1..NUM_DISK_PAGES |-> [pageId |-> p, values |-> 0]]
    buffer_pool = [p \in 1..NUM_POOL_PAGES |-> [pageId |-> p, values |-> 0, delta |-> NULL_DELTA]]
    delta_hm = [p \in 1..NUM_DISK_PAGES |-> NULL_DELTA]
    timestamp = 1
    undo_buffer = [p \in 1..NUM_THREADS |-> <<>>]
process Thread \in 1..NUM_THREADS
variables 
    value = 0;
    page_id = 0;
    txn_timestamp = 0;
    count = 0
    page  = []
begin
    Start:
        value := self;
        txn_timestamp := BeginTransaction();
        goto PickOperation;
        
    PickOperation:
        value := value + 1000; 
        with pid \in 1 .. NUM_DISK_PAGES do
            page_id := pid; 
            with op \in OPERATIONS do
                if op = "read" then
                    goto Read;
                elsif op = "insert" then
                    goto Insert;
                elsif op = "update" then
                    goto Update;
                elsif op = "delete" then
                    goto Delete;
                end if;
            end with;    
        end with; 

    Read:
        BufferPoolGet(page_id);
        goto IncCount;

    Insert:
        BufferPoolGet(page_id);
        if page.values /= 0 then 
            goto IncCount;
        else 
            AppendUndo(self, "INSERT", txn_timestamp, page_id, page.values);
            buffer_pool[SlotOf(page_id)].values := value;
            goto IncCount;
        end if;
        
    Update:

        page := BufferPoolGet(page_id);
        if page.values = 0 then 
            goto IncCount;
        else
            AppendUndo(self, "UPDATE", txn_timestamp, page_id, page.values);
            buffer_pool[SlotOf(page_id)].values := value;
            goto IncCount;
        end if;

    Delete:
        page := BufferPoolGet(page_id);
        if page.values = 0 then 
            goto IncCount;
        else
            AppendUndo(self, "DELETE", txn_timestamp, page_id, page.values);
            buffer_pool[SlotOf(page_id)].values := value;
            goto IncCount;
        end if;

    IncCount:
        count:=count+1;
        if count = OPERATIONS_COUNT then
            goto Done;
        else
            goto PickOperation;
        end if;

end process;

end algorithm; *)
\* BEGIN TRANSLATION (chksum(pcal) = "18366ab5" /\ chksum(tla) = "ea0576ae")
VARIABLES pc, nodes, txn

vars == << pc, nodes, txn >>

ProcSet == (1..NUM_THREADS)

Init == (* Global variables *)
        /\ nodes = {}
        (* Process Thread *)
        /\ txn = [self \in 1..NUM_THREADS |-> 1]
        /\ pc = [self \in ProcSet |-> "Start"]

Start(self) == /\ pc[self] = "Start"
               /\ pc' = [pc EXCEPT ![self] = "Done"]
               /\ UNCHANGED << nodes, txn >>

Thread(self) == Start(self)

(* Allow infinite stuttering to prevent deadlock on termination. *)
Terminating == /\ \A self \in ProcSet: pc[self] = "Done"
               /\ UNCHANGED vars

Next == (\E self \in 1..NUM_THREADS: Thread(self))
           \/ Terminating

Spec == Init /\ [][Next]_vars

Termination == <>(\A self \in ProcSet: pc[self] = "Done")

\* END TRANSLATION 

====

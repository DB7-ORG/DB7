---- MODULE buffer_pool ----
EXTENDS Integers, Sequences, FiniteSets, TLC

CONSTANTS NUM_THREADS, NUM_FRAMES, NUM_PAGES, NUM_PARTITIONS

(*--algorithm buffer_pool

variables
    hash_table = [par \in 0..NUM_PARTITIONS-1 |-> [p \in 1..NUM_PAGES |-> 0]],
    frames = [f \in 1..NUM_FRAMES |-> [
        page_id |-> 0,
        ref_count |-> 0,
        dirty |-> FALSE,
        io_in_progress |-> FALSE
    ]]
\* fair
process Thread \in 1..NUM_THREADS
variables wanted = 0, idx = 0, evict_page_id = 0;
begin

    Start:
        with p \in 1..NUM_PAGES do
            wanted := p;
        end with;

    Lookup:
        if hash_table[wanted % NUM_PARTITIONS][wanted] /= 0 then
            idx := hash_table[wanted % NUM_PARTITIONS][wanted];
            goto PageVisit;
        else
            goto FindVictim;
        end if;

    FindVictim:
        await (\E f \in 1..NUM_FRAMES : 
            frames[f].ref_count = 0 
            /\ frames[f].dirty = FALSE 
            /\ frames[f].io_in_progress = FALSE);
        with v \in {f \in 1..NUM_FRAMES : 
            frames[f].ref_count = 0 
            /\ frames[f].dirty = FALSE 
            /\ frames[f].io_in_progress = FALSE} do
            evict_page_id := frames[v].page_id;
            frames[v].ref_count := 1 || frames[v].page_id := wanted || frames[v].io_in_progress := TRUE;
            idx := v;
        end with;
        goto InsertVictimToTable;

    \* might want to get a pin while holding partition map lock
    \* then i can check right away and go to find victim i guess
    PageVisit:            
        if frames[idx].page_id = wanted then 
            frames[idx].ref_count := frames[idx].ref_count + 1;
            goto UsePage;
        else
            \* jump to lookup or find victim
            goto FindVictim;
        end if;

    UsePage:
        await frames[idx].io_in_progress = FALSE;
        frames[idx].ref_count := frames[idx].ref_count - 1;
        goto Done;
            
    InsertVictimToTable:
        if hash_table[wanted % NUM_PARTITIONS][wanted] = 0 then
            hash_table[wanted % NUM_PARTITIONS][wanted] := idx;
            goto FetchPage;
        else 
            goto UndoState;
        end if;
        
    UndoState:
        frames[idx].ref_count := frames[idx].ref_count - 1 || frames[idx].page_id := evict_page_id || frames[idx].io_in_progress := FALSE;
        evict_page_id:=0;
        idx:=0;
        goto Lookup;

    FetchPage:
        frames[idx].io_in_progress := FALSE;
        goto RemoveStaleEntry;
    
    RemoveStaleEntry:
        if evict_page_id = 0 then 
            goto UsePage;    
        elsif hash_table[evict_page_id % NUM_PARTITIONS][evict_page_id] = idx then
            hash_table[evict_page_id % NUM_PARTITIONS][evict_page_id] := 0;
            goto UsePage;
        else
            goto UsePage;      
        end if;
      
end process;

end algorithm; *)
\* BEGIN TRANSLATION (chksum(pcal) = "d9cc9037" /\ chksum(tla) = "d8831bbd")
VARIABLES pc, hash_table, frames, wanted, idx, evict_page_id

vars == << pc, hash_table, frames, wanted, idx, evict_page_id >>

ProcSet == ( 1 .. NUM_THREADS )

Init ==
  (* Global variables *)
  /\ hash_table =
       [par \in 0 .. NUM_PARTITIONS - 1 |-> [p \in 1 .. NUM_PAGES |-> 0]
       ]
  /\ frames =
       [f \in 1 .. NUM_FRAMES |->
         [ page_id |-> 0,
           ref_count |-> 0,
           dirty |-> FALSE,
           io_in_progress |-> FALSE
         ]
       ]
  (* Process Thread *)
  /\ wanted = [self \in 1 .. NUM_THREADS |-> 0]
  /\ idx = [self \in 1 .. NUM_THREADS |-> 0]
  /\ evict_page_id = [self \in 1 .. NUM_THREADS |-> 0]
  /\ pc = [self \in ProcSet |-> "Start"]

Start(self) ==
  /\ pc[self] = "Start"
  /\ \E p \in 1 .. NUM_PAGES: wanted' = [wanted EXCEPT ![self] = p]
  /\ pc' = [pc EXCEPT ![self] = "Lookup"]
  /\ UNCHANGED << hash_table, frames, idx, evict_page_id >>

Lookup(self) ==
  /\ pc[self] = "Lookup"
  /\ IF hash_table[wanted[self] % NUM_PARTITIONS][wanted[self]] /= 0
     THEN /\ idx' =
               [idx EXCEPT
               ![self] =
               hash_table[wanted[self] % NUM_PARTITIONS][wanted[self]]]
          /\ pc' = [pc EXCEPT ![self] = "PageVisit"]
     ELSE /\ pc' = [pc EXCEPT ![self] = "FindVictim"]
          /\ idx' = idx
  /\ UNCHANGED << hash_table, frames, wanted, evict_page_id >>

FindVictim(self) ==
  /\ pc[self] = "FindVictim"
  /\ ( \E f \in 1 .. NUM_FRAMES:
         frames[f].ref_count = 0 /\ frames[f].dirty = FALSE /\
           frames[f].io_in_progress = FALSE
     )
  /\ \E v \in
       {f \in 1 .. NUM_FRAMES:
           frames[f].ref_count = 0 /\ frames[f].dirty = FALSE /\
             frames[f].io_in_progress = FALSE
         }:
       /\ evict_page_id' = [evict_page_id EXCEPT ![self] = frames[v].page_id]
       /\ frames' =
            [frames EXCEPT
            ![v].ref_count =
            1,
            ![v].page_id =
            wanted[self],
            ![v].io_in_progress =
            TRUE]
       /\ idx' = [idx EXCEPT ![self] = v]
  /\ pc' = [pc EXCEPT ![self] = "InsertVictimToTable"]
  /\ UNCHANGED << hash_table, wanted >>

PageVisit(self) ==
  /\ pc[self] = "PageVisit"
  /\ IF frames[idx[self]].page_id = wanted[self]
     THEN /\ frames' =
               [frames EXCEPT
               ![idx[self]].ref_count =
               frames[idx[self]].ref_count + 1]
          /\ pc' = [pc EXCEPT ![self] = "UsePage"]
     ELSE /\ pc' = [pc EXCEPT ![self] = "FindVictim"]
          /\ UNCHANGED frames
  /\ UNCHANGED << hash_table, wanted, idx, evict_page_id >>

UsePage(self) ==
  /\ pc[self] = "UsePage"
  /\ frames[idx[self]].io_in_progress = FALSE
  /\ frames' =
       [frames EXCEPT ![idx[self]].ref_count = frames[idx[self]].ref_count - 1]
  /\ pc' = [pc EXCEPT ![self] = "Done"]
  /\ UNCHANGED << hash_table, wanted, idx, evict_page_id >>

InsertVictimToTable(self) ==
  /\ pc[self] = "InsertVictimToTable"
  /\ IF hash_table[wanted[self] % NUM_PARTITIONS][wanted[self]] = 0
     THEN /\ hash_table' =
               [hash_table EXCEPT
               ![wanted[self] % NUM_PARTITIONS][wanted[self]] =
               idx[self]]
          /\ pc' = [pc EXCEPT ![self] = "FetchPage"]
     ELSE /\ pc' = [pc EXCEPT ![self] = "UndoState"]
          /\ UNCHANGED hash_table
  /\ UNCHANGED << frames, wanted, idx, evict_page_id >>

UndoState(self) ==
  /\ pc[self] = "UndoState"
  /\ frames' =
       [frames EXCEPT
       ![idx[self]].ref_count =
       frames[idx[self]].ref_count - 1,
       ![idx[self]].page_id =
       evict_page_id[self],
       ![idx[self]].io_in_progress =
       FALSE]
  /\ evict_page_id' = [evict_page_id EXCEPT ![self] = 0]
  /\ idx' = [idx EXCEPT ![self] = 0]
  /\ pc' = [pc EXCEPT ![self] = "Lookup"]
  /\ UNCHANGED << hash_table, wanted >>

FetchPage(self) ==
  /\ pc[self] = "FetchPage"
  /\ frames' = [frames EXCEPT ![idx[self]].io_in_progress = FALSE]
  /\ pc' = [pc EXCEPT ![self] = "RemoveStaleEntry"]
  /\ UNCHANGED << hash_table, wanted, idx, evict_page_id >>

RemoveStaleEntry(self) ==
  /\ pc[self] = "RemoveStaleEntry"
  /\ IF evict_page_id[self] = 0
     THEN /\ pc' = [pc EXCEPT ![self] = "UsePage"]
          /\ UNCHANGED hash_table
     ELSE /\ IF hash_table[evict_page_id[self] % NUM_PARTITIONS][
                   evict_page_id[self]
                 ] =
                 idx[self]
             THEN /\ hash_table' =
                       [hash_table EXCEPT
                       ![evict_page_id[self] % NUM_PARTITIONS][evict_page_id[self]] =
                       0]
                  /\ pc' = [pc EXCEPT ![self] = "UsePage"]
             ELSE /\ pc' = [pc EXCEPT ![self] = "UsePage"]
                  /\ UNCHANGED hash_table
  /\ UNCHANGED << frames, wanted, idx, evict_page_id >>

Thread(self) ==
  Start(self) \/ Lookup(self) \/ FindVictim(self) \/ PageVisit(self) \/
            UsePage(self) \/
          InsertVictimToTable(self) \/
        UndoState(self) \/
      FetchPage(self) \/
    RemoveStaleEntry(self)

(* Allow infinite stuttering to prevent deadlock on termination. *)
Terminating ==
  /\ \A self \in ProcSet: pc[self] = "Done"
  /\ UNCHANGED vars

Next == ( \E self \in 1 .. NUM_THREADS: Thread(self) ) \/ Terminating

Spec == Init /\ [][Next]_vars

Termination == <>( \A self \in ProcSet: pc[self] = "Done" )

\* END TRANSLATION 
\* Checks at the end of alg that there are no duplicate pages
UniquePages ==
  ( \A t \in 1 .. NUM_THREADS: pc[t] = "Done" ) =>
    \A f1, f2 \in 1 .. NUM_FRAMES:
      f1 /= f2 /\ frames[f1].page_id /= 0 =>
        frames[f1].page_id /= frames[f2].page_id
\* Checks if any pins are leaked
NoLeakedFlags ==
  ( \A t \in 1 .. NUM_THREADS: pc[t] = "Done" ) =>
    \A f \in 1 .. NUM_FRAMES:
      frames[f].ref_count = 0 /\ frames[f].dirty = FALSE /\
        frames[f].io_in_progress = FALSE

\* Checks hash table points to correct page
HashTableConsistent ==
  ( \A t \in 1 .. NUM_THREADS: pc[t] = "Done" ) =>
    \A p \in 1 .. NUM_PAGES:
      hash_table[p % NUM_PARTITIONS][p] /= 0 =>
        frames[hash_table[p % NUM_PARTITIONS][p]].page_id = p

UsePageCorrect ==
  \A t \in 1 .. NUM_THREADS:
    pc[t] = "UsePage" => frames[idx[t]].page_id = wanted[t]

\* AllThreadsTerminate == <>( \A t \in 1 .. NUM_THREADS: pc[t] = "Done" )
====

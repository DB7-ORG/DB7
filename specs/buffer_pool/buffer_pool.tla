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
        io_in_progress |-> FALSE,
        write_in_progress |-> FALSE,
        \* this represents submitting something to the io_uring ring
        read_submitted |-> FALSE,
        write_submitted |-> FALSE,
        \* to check page versions are consistent on disk
        version |-> 0,          \* version of the page held in memory
        write_snapshot |-> 0    \* checks only: version when the write was submitted. Uses no memory
    ]],
    disk = [p \in 1..NUM_PAGES |-> 0],      \* version durable on disk
    latest = [p \in 1..NUM_PAGES |-> 0];    \* newest version ever produced (checks only)
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
            frames[idx].ref_count := frames[idx].ref_count + 1;
            goto PageVisit;
        else
            goto FindVictim;
        end if;

    FindVictim:
        await (\E f \in 1..NUM_FRAMES : 
            frames[f].ref_count = 0 
            /\ frames[f].io_in_progress = FALSE
            /\ frames[f].write_in_progress = FALSE);
        with v \in {f \in 1..NUM_FRAMES : 
            frames[f].ref_count = 0 
            /\ frames[f].io_in_progress = FALSE
            /\ frames[f].write_in_progress = FALSE} do

            if frames[v].dirty = TRUE then
                frames[v].dirty := FALSE || frames[v].write_in_progress := TRUE || 
                frames[v].write_submitted := TRUE || frames[v].write_snapshot := frames[v].version;
                goto FindVictim;
            else 
                evict_page_id := frames[v].page_id;
                frames[v].ref_count := 1 || frames[v].page_id := wanted || frames[v].io_in_progress := TRUE;
                idx := v;
                goto InsertVictimToTable;
            end if;
        end with;

    PageVisit:            
        if frames[idx].page_id = wanted then 
            goto UsePage;
        else
            frames[idx].ref_count := frames[idx].ref_count - 1;
            \* jump to lookup or find victim
            goto FindVictim;
        end if;

    UsePage:
        either
            await frames[idx].io_in_progress = FALSE;
            \* Just read the page no writing to it
            frames[idx].ref_count := frames[idx].ref_count - 1;
        or 
            await frames[idx].write_in_progress = FALSE /\ frames[idx].io_in_progress = FALSE;
            \* Write something to a page
            frames[idx].ref_count := frames[idx].ref_count - 1 ||
            frames[idx].dirty := TRUE || frames[idx].version := frames[idx].version + 1;
            latest[wanted] := latest[wanted] + 1;
        end either;
        goto Done;
            
    InsertVictimToTable:
        if hash_table[wanted % NUM_PARTITIONS][wanted] = 0 then
            hash_table[wanted % NUM_PARTITIONS][wanted] := idx;
            frames[idx].read_submitted := TRUE;
            goto RemoveStaleEntry;
        else 
            goto UndoState;
        end if;
        
    UndoState:
        frames[idx].ref_count := frames[idx].ref_count - 1 || frames[idx].page_id := evict_page_id || frames[idx].io_in_progress := FALSE;
        evict_page_id:=0;
        idx:=0;
        goto Lookup;
    
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

\* Stands in for the io_uring / AIO completion path.
process Completion = 0
begin
    Complete:
    if (\E t \in 1..NUM_THREADS : pc[t] /= "Done")
        \/ (\E g \in 1..NUM_FRAMES : frames[g].write_submitted) then
        either
            with f \in {g \in 1..NUM_FRAMES : frames[g].write_submitted} do
                disk[frames[f].page_id] := frames[f].version;
                frames[f].write_in_progress := FALSE ||
                frames[f].write_submitted := FALSE;
            end with;
        or
            with f \in {g \in 1..NUM_FRAMES : frames[g].read_submitted} do
                frames[f].version := disk[frames[f].page_id] ||
                frames[f].io_in_progress := FALSE ||
                frames[f].read_submitted := FALSE;
            end with;
        end either;
        goto Complete;
    else
        goto Done;
    end if;
end process;

end algorithm; *)
\* BEGIN TRANSLATION (chksum(pcal) = "4ac81533" /\ chksum(tla) = "d8f9ef32")
VARIABLES pc, hash_table, frames, disk, latest, wanted, idx, evict_page_id

vars == << pc, hash_table, frames, disk, latest, wanted, idx, evict_page_id
        >>

ProcSet == (1..NUM_THREADS) \cup {0}

Init == (* Global variables *)
        /\ hash_table = [par \in 0..NUM_PARTITIONS-1 |-> [p \in 1..NUM_PAGES |-> 0]]
        /\ frames =          [f \in 1..NUM_FRAMES |-> [
                        page_id |-> 0,
                        ref_count |-> 0,
                        dirty |-> FALSE,
                        io_in_progress |-> FALSE,
                        write_in_progress |-> FALSE,
                    
                        read_submitted |-> FALSE,
                        write_submitted |-> FALSE,
                    
                        version |-> 0,
                        write_snapshot |-> 0
                    ]]
        /\ disk = [p \in 1..NUM_PAGES |-> 0]
        /\ latest = [p \in 1..NUM_PAGES |-> 0]
        (* Process Thread *)
        /\ wanted = [self \in 1..NUM_THREADS |-> 0]
        /\ idx = [self \in 1..NUM_THREADS |-> 0]
        /\ evict_page_id = [self \in 1..NUM_THREADS |-> 0]
        /\ pc = [self \in ProcSet |-> CASE self \in 1..NUM_THREADS -> "Start"
                                        [] self = 0 -> "Complete"]

Start(self) == /\ pc[self] = "Start"
               /\ \E p \in 1..NUM_PAGES:
                    wanted' = [wanted EXCEPT ![self] = p]
               /\ pc' = [pc EXCEPT ![self] = "Lookup"]
               /\ UNCHANGED << hash_table, frames, disk, latest, idx, 
                               evict_page_id >>

Lookup(self) == /\ pc[self] = "Lookup"
                /\ IF hash_table[wanted[self] % NUM_PARTITIONS][wanted[self]] /= 0
                      THEN /\ idx' = [idx EXCEPT ![self] = hash_table[wanted[self] % NUM_PARTITIONS][wanted[self]]]
                           /\ frames' = [frames EXCEPT ![idx'[self]].ref_count = frames[idx'[self]].ref_count + 1]
                           /\ pc' = [pc EXCEPT ![self] = "PageVisit"]
                      ELSE /\ pc' = [pc EXCEPT ![self] = "FindVictim"]
                           /\ UNCHANGED << frames, idx >>
                /\ UNCHANGED << hash_table, disk, latest, wanted, 
                                evict_page_id >>

FindVictim(self) == /\ pc[self] = "FindVictim"
                    /\   (\E f \in 1..NUM_FRAMES :
                       frames[f].ref_count = 0
                       /\ frames[f].io_in_progress = FALSE
                       /\ frames[f].write_in_progress = FALSE)
                    /\ \E v \in        {f \in 1..NUM_FRAMES :
                                frames[f].ref_count = 0
                                /\ frames[f].io_in_progress = FALSE
                                /\ frames[f].write_in_progress = FALSE}:
                         IF frames[v].dirty = TRUE
                            THEN /\ frames' = [frames EXCEPT ![v].dirty = FALSE,
                                                             ![v].write_in_progress = TRUE,
                                                             ![v].write_submitted = TRUE,
                                                             ![v].write_snapshot = frames[v].version]
                                 /\ pc' = [pc EXCEPT ![self] = "FindVictim"]
                                 /\ UNCHANGED << idx, evict_page_id >>
                            ELSE /\ evict_page_id' = [evict_page_id EXCEPT ![self] = frames[v].page_id]
                                 /\ frames' = [frames EXCEPT ![v].ref_count = 1,
                                                             ![v].page_id = wanted[self],
                                                             ![v].io_in_progress = TRUE]
                                 /\ idx' = [idx EXCEPT ![self] = v]
                                 /\ pc' = [pc EXCEPT ![self] = "InsertVictimToTable"]
                    /\ UNCHANGED << hash_table, disk, latest, wanted >>

PageVisit(self) == /\ pc[self] = "PageVisit"
                   /\ IF frames[idx[self]].page_id = wanted[self]
                         THEN /\ pc' = [pc EXCEPT ![self] = "UsePage"]
                              /\ UNCHANGED frames
                         ELSE /\ frames' = [frames EXCEPT ![idx[self]].ref_count = frames[idx[self]].ref_count - 1]
                              /\ pc' = [pc EXCEPT ![self] = "FindVictim"]
                   /\ UNCHANGED << hash_table, disk, latest, wanted, idx, 
                                   evict_page_id >>

UsePage(self) == /\ pc[self] = "UsePage"
                 /\ \/ /\ frames[idx[self]].io_in_progress = FALSE
                       /\ frames' = [frames EXCEPT ![idx[self]].ref_count = frames[idx[self]].ref_count - 1]
                       /\ UNCHANGED latest
                    \/ /\ frames[idx[self]].write_in_progress = FALSE /\ frames[idx[self]].io_in_progress = FALSE
                       /\ frames' = [frames EXCEPT ![idx[self]].ref_count = frames[idx[self]].ref_count - 1,
                                                   ![idx[self]].dirty = TRUE,
                                                   ![idx[self]].version = frames[idx[self]].version + 1]
                       /\ latest' = [latest EXCEPT ![wanted[self]] = latest[wanted[self]] + 1]
                 /\ pc' = [pc EXCEPT ![self] = "Done"]
                 /\ UNCHANGED << hash_table, disk, wanted, idx, evict_page_id >>

InsertVictimToTable(self) == /\ pc[self] = "InsertVictimToTable"
                             /\ IF hash_table[wanted[self] % NUM_PARTITIONS][wanted[self]] = 0
                                   THEN /\ hash_table' = [hash_table EXCEPT ![wanted[self] % NUM_PARTITIONS][wanted[self]] = idx[self]]
                                        /\ frames' = [frames EXCEPT ![idx[self]].read_submitted = TRUE]
                                        /\ pc' = [pc EXCEPT ![self] = "RemoveStaleEntry"]
                                   ELSE /\ pc' = [pc EXCEPT ![self] = "UndoState"]
                                        /\ UNCHANGED << hash_table, frames >>
                             /\ UNCHANGED << disk, latest, wanted, idx, 
                                             evict_page_id >>

UndoState(self) == /\ pc[self] = "UndoState"
                   /\ frames' = [frames EXCEPT ![idx[self]].ref_count = frames[idx[self]].ref_count - 1,
                                               ![idx[self]].page_id = evict_page_id[self],
                                               ![idx[self]].io_in_progress = FALSE]
                   /\ evict_page_id' = [evict_page_id EXCEPT ![self] = 0]
                   /\ idx' = [idx EXCEPT ![self] = 0]
                   /\ pc' = [pc EXCEPT ![self] = "Lookup"]
                   /\ UNCHANGED << hash_table, disk, latest, wanted >>

RemoveStaleEntry(self) == /\ pc[self] = "RemoveStaleEntry"
                          /\ IF evict_page_id[self] = 0
                                THEN /\ pc' = [pc EXCEPT ![self] = "UsePage"]
                                     /\ UNCHANGED hash_table
                                ELSE /\ IF hash_table[evict_page_id[self] % NUM_PARTITIONS][evict_page_id[self]] = idx[self]
                                           THEN /\ hash_table' = [hash_table EXCEPT ![evict_page_id[self] % NUM_PARTITIONS][evict_page_id[self]] = 0]
                                                /\ pc' = [pc EXCEPT ![self] = "UsePage"]
                                           ELSE /\ pc' = [pc EXCEPT ![self] = "UsePage"]
                                                /\ UNCHANGED hash_table
                          /\ UNCHANGED << frames, disk, latest, wanted, idx, 
                                          evict_page_id >>

Thread(self) == Start(self) \/ Lookup(self) \/ FindVictim(self)
                   \/ PageVisit(self) \/ UsePage(self)
                   \/ InsertVictimToTable(self) \/ UndoState(self)
                   \/ RemoveStaleEntry(self)

Complete == /\ pc[0] = "Complete"
            /\ IF (\E t \in 1..NUM_THREADS : pc[t] /= "Done")
                   \/ (\E g \in 1..NUM_FRAMES : frames[g].write_submitted)
                  THEN /\ \/ /\ \E f \in {g \in 1..NUM_FRAMES : frames[g].write_submitted}:
                                  /\ disk' = [disk EXCEPT ![frames[f].page_id] = frames[f].version]
                                  /\ frames' = [frames EXCEPT ![f].write_in_progress = FALSE,
                                                              ![f].write_submitted = FALSE]
                          \/ /\ \E f \in {g \in 1..NUM_FRAMES : frames[g].read_submitted}:
                                  frames' = [frames EXCEPT ![f].version = disk[frames[f].page_id],
                                                           ![f].io_in_progress = FALSE,
                                                           ![f].read_submitted = FALSE]
                             /\ disk' = disk
                       /\ pc' = [pc EXCEPT ![0] = "Complete"]
                  ELSE /\ pc' = [pc EXCEPT ![0] = "Done"]
                       /\ UNCHANGED << frames, disk >>
            /\ UNCHANGED << hash_table, latest, wanted, idx, evict_page_id >>

Completion == Complete

(* Allow infinite stuttering to prevent deadlock on termination. *)
Terminating == /\ \A self \in ProcSet: pc[self] = "Done"
               /\ UNCHANGED vars

Next == Completion
           \/ (\E self \in 1..NUM_THREADS: Thread(self))
           \/ Terminating

Spec == Init /\ [][Next]_vars

Termination == <>(\A self \in ProcSet: pc[self] = "Done")

\* END TRANSLATION 
\* Checks at the end of alg that there are no duplicate pages
UniquePages ==
  ( \A t \in 1 .. NUM_THREADS: pc[t] = "Done" ) =>
    \A f1, f2 \in 1 .. NUM_FRAMES:
      f1 /= f2 /\ frames[f1].page_id /= 0 =>
        frames[f1].page_id /= frames[f2].page_id
\* Checks if any pins are leaked
NoLeakedFlags ==
  pc[0] = "Done" =>
    \A f \in 1 .. NUM_FRAMES:
      frames[f].ref_count = 0 /\ frames[f].write_in_progress = FALSE /\
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

\* A thread that can touch the page sees the newest version (no stale read, no lost update)
FreshPage ==
  \A t \in 1 .. NUM_THREADS:
    (pc[t] = "UsePage" /\ frames[idx[t]].page_id = wanted[t] /\ frames[idx[t]].io_in_progress = FALSE)
      => frames[idx[t]].version = latest[wanted[t]]

\* A clean frame with no write in flight really does match disk
CleanMeansDurable ==
  \A f \in 1 .. NUM_FRAMES:
    (frames[f].page_id /= 0 /\ frames[f].ref_count = 0 /\ ~frames[f].dirty
       /\ ~frames[f].write_in_progress /\ ~frames[f].io_in_progress)
      => disk[frames[f].page_id] = frames[f].version

\* The frame is what the kernel is reading from, so it must not change mid-write
StableDuringWrite ==
  \A f \in 1 .. NUM_FRAMES:
    frames[f].write_in_progress => frames[f].version = frames[f].write_snapshot

\* AllThreadsTerminate == <>( \A t \in 1 .. NUM_THREADS: pc[t] = "Done" )
====

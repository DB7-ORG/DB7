---- MODULE buffer_pool ----
EXTENDS Integers, Sequences, FiniteSets, TLC

CONSTANTS NUM_THREADS, NUM_FRAMES, NUM_PAGES

(*--algorithm buffer_pool

variables
    hash_table = [p \in 1..NUM_PAGES |-> 0],\* we assume only one partition because it should work the same on multiple partitions
    frames = [f \in 1..NUM_FRAMES |-> [
        page_id |-> 0,
        ref_count |-> 0
        \* dirty |-> FALSE,
        \* lock |-> FALSE
    ]]

process Thread \in 1..NUM_THREADS
variables wanted = 0, idx = 0, evict_page_id = 0;
begin
    Start:
        with p \in 1..NUM_PAGES do
            wanted := p;
        end with;

        goto Lookup;

    Lookup:
        if hash_table[wanted] /= 0 then
            idx := hash_table[wanted];
            goto Recheck;
        else
            goto SlowPath;
        end if;

    Recheck:
        if frames[idx].page_id /= wanted then
            goto SlowPath;
        else
            frames[idx].ref_count := frames[idx].ref_count + 1;
            goto End;
        end if;

    SlowPath:
        \* eviction logic goes here (picks a random page)
        with v \in {f \in 1..NUM_FRAMES : frames[f].ref_count = 0} do
            evict_page_id := frames[v].page_id;
            frames[v].ref_count := 1;
            idx := v;
        end with;
        goto SlowPathTableModify;

    SlowPathTableModify:
        if hash_table[wanted] = 0 then
            \* hash_table[evict_page_id] := 0;
            \* hash_table[wanted] := idx;
            hash_table := [hash_table EXCEPT ![evict_page_id] = 0, ![wanted] = idx];
            goto ReplaceFrame;
        \* elsif hash_table[wanted] /= evict_page_id then
        \*     idx := hash_table[wanted];
        \*     goto Recheck;
        else
            goto UndoState;
        end if;

    ReplaceFrame:
        if frames[idx].ref_count = 1 then
            frames[idx].page_id := wanted;
            goto End;
        else
            goto Lookup;
        end if; 

    UndoState:
        frames[idx].ref_count := frames[idx].ref_count - 1;
        idx := 0;
        evict_page_id := 0;
        goto Lookup;

    End:
        frames[idx].ref_count := frames[idx].ref_count - 1;
        idx := 0;
        evict_page_id := 0;
        goto Done;

        

end process;

end algorithm; *)
\* BEGIN TRANSLATION (chksum(pcal) = "7fd77693" /\ chksum(tla) = "fa750e01")
VARIABLES pc, hash_table, frames, wanted, idx, evict_page_id

vars == << pc, hash_table, frames, wanted, idx, evict_page_id >>

ProcSet == ( 1 .. NUM_THREADS )

Init ==
  (* Global variables *)
  /\ hash_table = [p \in 1 .. NUM_PAGES |-> 0]
  /\ frames = [f \in 1 .. NUM_FRAMES |-> [ page_id |-> 0, ref_count |-> 0 ]]
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
  /\ IF hash_table[wanted[self]] /= 0
     THEN /\ idx' = [idx EXCEPT ![self] = hash_table[wanted[self]]]
          /\ pc' = [pc EXCEPT ![self] = "Recheck"]
     ELSE /\ pc' = [pc EXCEPT ![self] = "SlowPath"]
          /\ idx' = idx
  /\ UNCHANGED << hash_table, frames, wanted, evict_page_id >>

Recheck(self) ==
  /\ pc[self] = "Recheck"
  /\ IF frames[idx[self]].page_id /= wanted[self]
     THEN /\ pc' = [pc EXCEPT ![self] = "SlowPath"]
          /\ UNCHANGED frames
     ELSE /\ frames' =
               [frames EXCEPT
               ![idx[self]].ref_count =
               frames[idx[self]].ref_count + 1]
          /\ pc' = [pc EXCEPT ![self] = "End"]
  /\ UNCHANGED << hash_table, wanted, idx, evict_page_id >>

SlowPath(self) ==
  /\ pc[self] = "SlowPath"
  /\ \E v \in {f \in 1 .. NUM_FRAMES: frames[f].ref_count = 0}:
       /\ evict_page_id' = [evict_page_id EXCEPT ![self] = frames[v].page_id]
       /\ frames' = [frames EXCEPT ![v].ref_count = 1]
       /\ idx' = [idx EXCEPT ![self] = v]
  /\ pc' = [pc EXCEPT ![self] = "SlowPathTableModify"]
  /\ UNCHANGED << hash_table, wanted >>

SlowPathTableModify(self) ==
  /\ pc[self] = "SlowPathTableModify"
  /\ IF hash_table[wanted[self]] = 0
     THEN /\ hash_table' =
               [hash_table EXCEPT
               ![evict_page_id[self]] =
               0,
               ![wanted[self]] =
               idx[self]]
          /\ pc' = [pc EXCEPT ![self] = "ReplaceFrame"]
     ELSE /\ pc' = [pc EXCEPT ![self] = "UndoState"]
          /\ UNCHANGED hash_table
  /\ UNCHANGED << frames, wanted, idx, evict_page_id >>

ReplaceFrame(self) ==
  /\ pc[self] = "ReplaceFrame"
  /\ IF frames[idx[self]].ref_count = 1
     THEN /\ frames' = [frames EXCEPT ![idx[self]].page_id = wanted[self]]
          /\ pc' = [pc EXCEPT ![self] = "End"]
     ELSE /\ pc' = [pc EXCEPT ![self] = "Lookup"]
          /\ UNCHANGED frames
  /\ UNCHANGED << hash_table, wanted, idx, evict_page_id >>

UndoState(self) ==
  /\ pc[self] = "UndoState"
  /\ frames' =
       [frames EXCEPT ![idx[self]].ref_count = frames[idx[self]].ref_count - 1]
  /\ idx' = [idx EXCEPT ![self] = 0]
  /\ evict_page_id' = [evict_page_id EXCEPT ![self] = 0]
  /\ pc' = [pc EXCEPT ![self] = "Lookup"]
  /\ UNCHANGED << hash_table, wanted >>

End(self) ==
  /\ pc[self] = "End"
  /\ frames' =
       [frames EXCEPT ![idx[self]].ref_count = frames[idx[self]].ref_count - 1]
  /\ idx' = [idx EXCEPT ![self] = 0]
  /\ evict_page_id' = [evict_page_id EXCEPT ![self] = 0]
  /\ pc' = [pc EXCEPT ![self] = "Done"]
  /\ UNCHANGED << hash_table, wanted >>

Thread(self) ==
  Start(self) \/ Lookup(self) \/ Recheck(self) \/ SlowPath(self) \/
          SlowPathTableModify(self) \/
        ReplaceFrame(self) \/
      UndoState(self) \/
    End(self)

(* Allow infinite stuttering to prevent deadlock on termination. *)
Terminating ==
  /\ \A self \in ProcSet: pc[self] = "Done"
  /\ UNCHANGED vars

Next == ( \E self \in 1 .. NUM_THREADS: Thread(self) ) \/ Terminating

Spec == Init /\ [][Next]_vars

Termination == <>( \A self \in ProcSet: pc[self] = "Done" )

\* END TRANSLATION 
\* Checks so that htable doesnt have 2 different pointers to same frame
HashTableNoDuplicates ==
  \A p1, p2 \in 1 .. NUM_PAGES:
    p1 /= p2 /\ hash_table[p1] /= 0 => hash_table[p1] /= hash_table[p2]

UniquePages ==
  ( \A t \in 1 .. NUM_THREADS: pc[t] = "Done" ) =>
    \A f1, f2 \in 1 .. NUM_FRAMES:
      f1 /= f2 /\ frames[f1].page_id /= 0 =>
        frames[f1].page_id /= frames[f2].page_id
====

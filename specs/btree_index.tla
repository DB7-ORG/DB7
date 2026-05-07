---- MODULE btree_index ----
EXTENDS Integers, Sequences, FiniteSets, TLC

CONSTANTS NUM_THREADS, VALUES


\* ============================================================
\* Pure TLA+ operators for B-tree data manipulation
\* ============================================================
MAX_NUM_KEYS == 3
UNDEFINED == 0

EmptyNode(id) ==
  [ keys |-> << 0, 0, 0 >>,
    children |-> << 0, 0, 0, 0 >>,
    is_leaf |-> TRUE,
    count |-> 0,
    rlink |-> 0,
    max_val |-> 0,
    idx |-> id
  ]

NewNode(keys, children, is_leaf, count, rlink, max_val, id) ==
  [ keys |-> keys,
    children |-> children,
    is_leaf |-> is_leaf,
    count |-> count,
    rlink |-> rlink,
    max_val |-> max_val,
    idx |-> id
  ]

\* Find first index where keys[i] >= key, or count+1
FindPosition(node, key) ==
  CHOOSE i \in 1 .. ( node.count + 1 ):
    /\ \A j \in 1 .. ( i - 1 ): node.keys[j] < key
    /\ ( i > node.count \/ node.keys[i] >= key )

\* Find insert position: first index where keys[i] >= key, or count+1
FindInsertPosition(node, key) ==
  IF node.count = 0 THEN 1 ELSE FindPosition(node, key)

\* Insert key at position idx, shifting keys right
ShiftRightInsert(seq, pos, key) ==
  [i \in 1 .. Len(seq) |->
    IF i < pos THEN seq[i] ELSE IF i = pos THEN key ELSE seq[i - 1]
  ]

\* ============================================================
\* PlusCal: only control flow and concurrency
\* ============================================================
(*--algorithm btree_index

variables
    nodes = [i \in 1..100 |-> EmptyNode(i)],
    root = UNDEFINED,
    next_free = 1,
    stack = <<>>;

process Thread \in 1..NUM_THREADS
variables 
    key = UNDEFINED,
    current = UNDEFINED,
    idx = UNDEFINED;

begin

    Start:
        with val \in 1..VALUES do
            key := val;
        end with;

        if root = UNDEFINED then
            root := EmptyNode(next_free);
            next_free := next_free+1;
        end if;
    
        current := root;

    DropToLeaf:
        if current.is_leaf = TRUE then 
            goto Insert;
        elsif current.max_val /= UNDEFINED /\ key >= current.max_val then
            current := nodes[current.rlink];
            goto DropToLeaf;
        else
            stack := Append(stack, current.idx);
            idx := FindPosition(current, key);
            current:= nodes[current.children[idx]];
            goto DropToLeaf;
        end if;
        
\* EmptyNode(id) ==
\*   [ keys |-> << 0, 0, 0 >>,
\*     children |-> << 0, 0, 0, 0 >>,
\*     is_leaf |-> TRUE,
\*     count |-> 0,
\*     rlink |-> 0,
\*     max_val |-> 0,
\*     idx |-> id
\*   ]
    Insert:
        if current.max_val /= UNDEFINED /\ key >= current.max_val then 
            current := nodes[current.rlink];
            goto Insert;
        elsif current.count /= MAX_NUM_KEYS then
            idx := FindInsertPosition(current, key);
            nodes[current.id] := 
            NewNode(
                ShiftRightInsert(current.keys, idx, key),
                \* usually this is ShiftRightInsert(current.children, insert_idx + 1, ref)
                << 0, 0, 0, 0 >>, 
                TRUE,
                current.count+1,
                current.rlink,
                current.max_val,
                current.id
            );
            goto Done;
        else
            \* split leaf
        end if;
        
end process;

end algorithm; *)
\* BEGIN TRANSLATION
VARIABLES pc, nodes, root, next_free, stack, key, current, idx

vars == << pc, nodes, root, next_free, stack, key, current, idx >>

ProcSet == ( 1 .. NUM_THREADS )

Init ==
  (* Global variables *)
  /\ nodes = [i \in 1 .. 100 |-> EmptyNode(i)]
  /\ root = UNDEFINED
  /\ next_free = 1
  /\ stack = <<>>
  (* Process Thread *)
  /\ key = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ current = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ idx = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ pc = [self \in ProcSet |-> "Start"]

Start(self) ==
  /\ pc[self] = "Start"
  /\ \E val \in 1 .. VALUES: key' = [key EXCEPT ![self] = val]
  /\ IF root = UNDEFINED
     THEN /\ root' = EmptyNode(next_free)
          /\ next_free' = next_free + 1
     ELSE /\ TRUE
          /\ UNCHANGED << root, next_free >>
  /\ current' = [current EXCEPT ![self] = root']
  /\ pc' = [pc EXCEPT ![self] = "DropToLeaf"]
  /\ UNCHANGED << nodes, stack, idx >>

DropToLeaf(self) ==
  /\ pc[self] = "DropToLeaf"
  /\ IF current[self].is_leaf = TRUE
     THEN /\ pc' = [pc EXCEPT ![self] = "Insert"]
          /\ UNCHANGED << stack, current, idx >>
     ELSE /\ IF current[self].max_val /= UNDEFINED /\
                 key[self] >= current[self].max_val
             THEN /\ current' =
                       [current EXCEPT ![self] = nodes[current[self].rlink]]
                  /\ pc' = [pc EXCEPT ![self] = "DropToLeaf"]
                  /\ UNCHANGED << stack, idx >>
             ELSE /\ stack' =
                       [stack EXCEPT ![self] = Append(stack, current[self].idx)]
                  /\ idx' =
                       [idx EXCEPT
                       ![self] =
                       FindPosition(current[self], key[self])]
                  /\ current' =
                       [current EXCEPT
                       ![self] =
                       nodes[current[self].children[idx'[self]]]]
                  /\ pc' = [pc EXCEPT ![self] = "DropToLeaf"]
  /\ UNCHANGED << nodes, root, next_free, key >>

Insert(self) ==
  /\ pc[self] = "Insert"
  /\ IF current[self].max_val /= UNDEFINED /\ key[self] >= current[self].max_val
     THEN /\ current' = [current EXCEPT ![self] = nodes[current[self].rlink]]
          /\ pc' = [pc EXCEPT ![self] = "Insert"]
          /\ UNCHANGED << nodes, idx >>
     ELSE /\ IF current[self].count /= MAX_NUM_KEYS
             THEN /\ idx' =
                       [idx EXCEPT
                       ![self] =
                       FindInsertPosition(current[self], key[self])]
                  /\ nodes' =
                       [nodes EXCEPT
                       ![current[self].id] =
                       NewNode(ShiftRightInsert(current[self].keys, idx'[self], key[self]), << 0, 0, 0, 0 >>, TRUE, current[
                             self
                           ].count +
                           1, current[
                           self
                         ].rlink, current[
                           self
                         ].max_val, current[
                           self
                         ].id)]
                  /\ pc' = [pc EXCEPT ![self] = "Done"]
             ELSE /\ pc' = [pc EXCEPT ![self] = "Done"]
                  /\ UNCHANGED << nodes, idx >>
          /\ UNCHANGED current
  /\ UNCHANGED << root, next_free, stack, key >>

Thread(self) == Start(self) \/ DropToLeaf(self) \/ Insert(self)

(* Allow infinite stuttering to prevent deadlock on termination. *)
Terminating ==
  /\ \A self \in ProcSet: pc[self] = "Done"
  /\ UNCHANGED vars

Next == ( \E self \in 1 .. NUM_THREADS: Thread(self) ) \/ Terminating

Spec == Init /\ [][Next]_vars

Termination == <>( \A self \in ProcSet: pc[self] = "Done" )

\* END TRANSLATION
====

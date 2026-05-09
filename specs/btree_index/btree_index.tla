---- MODULE btree_index ----
EXTENDS Integers, Sequences, FiniteSets, TLC

CONSTANTS NUM_THREADS


\* ============================================================
\* Pure TLA+ operators for B-tree data manipulation
\* ============================================================
MAX_NUM_KEYS == 3
UNDEFINED == 0

\* NOTE is_leaf is not needed since i have level
EmptyNode(id) ==
  [ keys |-> << 0, 0, 0 >>,
    children |-> << 0, 0, 0, 0 >>,
    is_leaf |-> TRUE,
    count |-> 0,
    rlink |-> UNDEFINED,
    max_val |-> UNDEFINED,
    idx |-> id,
    level |-> 0
  ]

NewNode(keys, children, is_leaf, count, rlink, max_val, id, lvl) ==
  [ keys |-> keys,
    children |-> children,
    is_leaf |-> is_leaf,
    count |-> count,
    rlink |-> rlink,
    max_val |-> max_val,
    idx |-> id,
    level |-> lvl
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

OverfullKeys(seq, key, pos) ==
  [i \in 1 .. ( Len(seq) + 1 ) |->
    IF i < pos THEN seq[i] ELSE IF i = pos THEN key ELSE seq[i - 1]
  ]

LeftHalfKeys(seq4) ==
  LET mid == ( Len(seq4) ) \div 2
  IN [i \in 1 .. ( Len(seq4) - 1 ) |-> IF i <= mid THEN seq4[i] ELSE 0
      ]

RightHalfKeys(seq4) ==
  LET mid == ( Len(seq4) ) \div 2
  IN [i \in 1 .. ( Len(seq4) - 1 ) |->
        IF i <= Len(seq4) - mid THEN seq4[i + mid] ELSE 0
      ]

ReloadCurrent(current_idx, nodes) == nodes[current_idx]

\* ============================================================
\* PlusCal: only control flow and concurrency
\* ============================================================
(*--algorithm btree_index

variables
    nodes = [i \in 1..10 |-> EmptyNode(i)],
    root_idx = UNDEFINED,
    next_free = 1;

process Thread \in 1..NUM_THREADS
variables 
    key = UNDEFINED,
    current_idx = UNDEFINED,
    current = UNDEFINED,
    idx = UNDEFINED,
    last_page_idx = UNDEFINED,
    drop_level = UNDEFINED,
    stack = UNDEFINED,
    sentinel = UNDEFINED,
    overfull = UNDEFINED,
    overfull2 = UNDEFINED;
begin

    Start:
        \* with val \in 1..VALUES do
        \*     key := val;
        \* end with;
        key := self;

        if root_idx = UNDEFINED then
            nodes[next_free] := EmptyNode(next_free);
            root_idx := next_free;
            next_free := next_free+1;
        end if;
    
        current_idx := root_idx;
        drop_level := 0;
        stack := <<>>;

    DropToLevel:
        current := ReloadCurrent(current_idx, nodes);

        if current_idx = root_idx /\  current.level = 0 then 
            goto HandleRoot;
        elsif current.level <= drop_level then
            goto Insert;
        elsif current.is_leaf = TRUE then 
            goto Insert;
        elsif current.max_val /= UNDEFINED /\ key >= current.max_val then
            current_idx := current.rlink;
            goto DropToLevel;
        else
            stack := Append(stack, current_idx);
            idx := FindPosition(current, key);
            current_idx := current.children[idx];
            goto DropToLevel;
        end if;

    Insert:
        current := ReloadCurrent(current_idx, nodes);

        if current.max_val /= UNDEFINED /\ key >= current.max_val then 
            current_idx := current.rlink;
            goto Insert;
        elsif current.count /= MAX_NUM_KEYS then
            idx := FindInsertPosition(current, key);
            nodes[current_idx] := 
            NewNode(
                ShiftRightInsert(current.keys, idx, key),
                ShiftRightInsert(current.children, idx + 1, last_page_idx),
                current.is_leaf,
                current.count+1,
                current.rlink,
                current.max_val,
                current_idx,
                current.level
            );
            goto Done;
        else
            \* split leaf
            \* The overfull sequence includes the key we're inserting
            idx := FindInsertPosition(current, key);
            overfull := OverfullKeys(current.keys, key, idx);
            overfull2 := OverfullKeys(current.children, last_page_idx, idx + 1);
            sentinel := overfull[(MAX_NUM_KEYS + 1) \div 2 + 1];

            nodes[next_free] :=
                NewNode(
                    RightHalfKeys(overfull),
                    RightHalfKeys(overfull2),
                    current.is_leaf,
                    (MAX_NUM_KEYS + 1) \div 2,
                    current.rlink,
                    current.max_val,
                    next_free,
                    current.level
                ) ||
            nodes[current_idx] :=
                NewNode(
                    LeftHalfKeys(overfull),
                    LeftHalfKeys(overfull2),
                    current.is_leaf,
                    (MAX_NUM_KEYS + 1) - ((MAX_NUM_KEYS + 1) \div 2),
                    next_free,
                    sentinel,
                    current_idx,
                    current.level
                );

            key := sentinel;  \* propagate the separator upward
            last_page_idx := next_free;

            next_free:=next_free+1;
            if Len(stack) = 0 then 
                drop_level := current.level + 1;
                current_idx := root_idx;
                goto DropToLevel;
            else
                goto PropagateInsert;
            end if;
            
        end if;

    PropagateInsert:
        current := ReloadCurrent(current_idx, nodes);

        if Len(stack) = 0 then 
            \* root is the leaf
            goto HandleRoot;
        else 
            \* regular case, not root
            \* last_page_idx := current.idx;
            current_idx := stack[Len(stack)];
            stack := SubSeq(stack, 1, Len(stack) - 1);
            goto Insert;
        end if;

    HandleRoot:
        \* handle root
        current := ReloadCurrent(current_idx, nodes);
        \* need to go right
        if current.max_val /= UNDEFINED /\ key >= current.max_val then 
            current_idx := current.rlink;
            goto HandleRoot;
        elsif current.count /= MAX_NUM_KEYS then
            idx := FindInsertPosition(current, key);
            nodes[current_idx] := 
            NewNode(
                ShiftRightInsert(current.keys, idx, key),
                ShiftRightInsert(current.children, idx + 1, last_page_idx), 
                current.is_leaf,
                current.count+1,
                current.rlink,
                current.max_val,
                current_idx,
                current.level
            );
            goto Done;
        else 
            \* we need to split root
            idx := FindInsertPosition(current, key);
            overfull := OverfullKeys(current.keys, key, idx);
            overfull2 := OverfullKeys(current.children, last_page_idx, idx + 1);
            sentinel := overfull[(MAX_NUM_KEYS + 1) \div 2 + 1];

            key := sentinel;  \* propagate the separator upward
            last_page_idx := next_free;

            if current_idx = root_idx then
                \* create new root no problem
                nodes[next_free] :=
                    NewNode(
                        RightHalfKeys(overfull),
                        RightHalfKeys(overfull2),
                        current.is_leaf,
                        (MAX_NUM_KEYS + 1) \div 2,
                        current.rlink,
                        current.max_val,
                        next_free,
                        current.level
                    ) ||
                nodes[current_idx] :=
                    NewNode(
                        LeftHalfKeys(overfull),
                        LeftHalfKeys(overfull2),
                        current.is_leaf,
                        (MAX_NUM_KEYS + 1) - ((MAX_NUM_KEYS + 1) \div 2),
                        next_free,
                        sentinel,
                        current_idx,
                        current.level
                    ) ||
                nodes[next_free + 1] := NewNode(
                    << key, 0, 0 >>,
                    << current_idx, last_page_idx, 0, 0 >>,
                    FALSE,
                    1,
                    UNDEFINED,
                    UNDEFINED,
                    next_free+1,
                    current.level + 1
                ); 
                root_idx := next_free + 1;

                next_free:=next_free+2;
                
                goto Done;

            else 
                \* traverse again to level above and proceed from there
                nodes[next_free] :=
                    NewNode(
                        RightHalfKeys(overfull),
                        RightHalfKeys(overfull2),
                        current.is_leaf,
                        (MAX_NUM_KEYS + 1) \div 2,
                        current.rlink,
                        current.max_val,
                        next_free,
                        current.level
                    ) ||
                nodes[current_idx] :=
                    NewNode(
                        LeftHalfKeys(overfull),
                        LeftHalfKeys(overfull2),
                        current.is_leaf,
                        (MAX_NUM_KEYS + 1) - ((MAX_NUM_KEYS + 1) \div 2),
                        next_free,
                        sentinel,
                        current_idx,
                        current.level
                    );

                next_free := next_free + 1;
                drop_level := current.level + 1;
                current_idx := root_idx;
                goto DropToLevel;
            end if;
            
        end if; 
     
end process;

end algorithm; *)
\* BEGIN TRANSLATION
VARIABLES pc,
          nodes,
          root_idx,
          next_free,
          key,
          current_idx,
          current,
          idx,
          last_page_idx,
          drop_level,
          stack,
          sentinel,
          overfull,
          overfull2

vars ==
  << pc,
     nodes,
     root_idx,
     next_free,
     key,
     current_idx,
     current,
     idx,
     last_page_idx,
     drop_level,
     stack,
     sentinel,
     overfull,
     overfull2
  >>

ProcSet == ( 1 .. NUM_THREADS )

Init ==
  (* Global variables *)
  /\ nodes = [i \in 1 .. 10 |-> EmptyNode(i)]
  /\ root_idx = UNDEFINED
  /\ next_free = 1
  (* Process Thread *)
  /\ key = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ current_idx = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ current = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ idx = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ last_page_idx = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ drop_level = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ stack = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ sentinel = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ overfull = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ overfull2 = [self \in 1 .. NUM_THREADS |-> UNDEFINED]
  /\ pc = [self \in ProcSet |-> "Start"]

Start(self) ==
  /\ pc[self] = "Start"
  /\ key' = [key EXCEPT ![self] = self]
  /\ IF root_idx = UNDEFINED
     THEN /\ nodes' = [nodes EXCEPT ![next_free] = EmptyNode(next_free)]
          /\ root_idx' = next_free
          /\ next_free' = next_free + 1
     ELSE /\ TRUE
          /\ UNCHANGED << nodes, root_idx, next_free >>
  /\ current_idx' = [current_idx EXCEPT ![self] = root_idx']
  /\ drop_level' = [drop_level EXCEPT ![self] = 0]
  /\ stack' = [stack EXCEPT ![self] = <<>>]
  /\ pc' = [pc EXCEPT ![self] = "DropToLevel"]
  /\ UNCHANGED << current, idx, last_page_idx, sentinel, overfull, overfull2 >>

DropToLevel(self) ==
  /\ pc[self] = "DropToLevel"
  /\ current' =
       [current EXCEPT ![self] = ReloadCurrent(current_idx[self], nodes)]
  /\ IF current_idx[self] = root_idx /\ current'[self].level = 0
     THEN /\ pc' = [pc EXCEPT ![self] = "HandleRoot"]
          /\ UNCHANGED << current_idx, idx, stack >>
     ELSE /\ IF current'[self].level <= drop_level[self]
             THEN /\ pc' = [pc EXCEPT ![self] = "Insert"]
                  /\ UNCHANGED << current_idx, idx, stack >>
             ELSE /\ IF current'[self].is_leaf = TRUE
                     THEN /\ pc' = [pc EXCEPT ![self] = "Insert"]
                          /\ UNCHANGED << current_idx, idx, stack >>
                     ELSE /\ IF current'[self].max_val /= UNDEFINED /\
                                 key[self] >= current'[self].max_val
                             THEN /\ current_idx' =
                                       [current_idx EXCEPT
                                       ![self] =
                                       current'[self].rlink]
                                  /\ pc' = [pc EXCEPT ![self] = "DropToLevel"]
                                  /\ UNCHANGED << idx, stack >>
                             ELSE /\ stack' =
                                       [stack EXCEPT
                                       ![self] =
                                       Append(stack[self], current_idx[self])]
                                  /\ idx' =
                                       [idx EXCEPT
                                       ![self] =
                                       FindPosition(current'[self], key[self])]
                                  /\ current_idx' =
                                       [current_idx EXCEPT
                                       ![self] =
                                       current'[self].children[idx'[self]]]
                                  /\ pc' = [pc EXCEPT ![self] = "DropToLevel"]
  /\ UNCHANGED << nodes,
        root_idx,
        next_free,
        key,
        last_page_idx,
        drop_level,
        sentinel,
        overfull,
        overfull2
     >>

Insert(self) ==
  /\ pc[self] = "Insert"
  /\ current' =
       [current EXCEPT ![self] = ReloadCurrent(current_idx[self], nodes)]
  /\ IF current'[self].max_val /= UNDEFINED /\
         key[self] >= current'[self].max_val
     THEN /\ current_idx' = [current_idx EXCEPT ![self] = current'[self].rlink]
          /\ pc' = [pc EXCEPT ![self] = "Insert"]
          /\ UNCHANGED << nodes,
                next_free,
                key,
                idx,
                last_page_idx,
                drop_level,
                sentinel,
                overfull,
                overfull2
             >>
     ELSE /\ IF current'[self].count /= MAX_NUM_KEYS
             THEN /\ idx' =
                       [idx EXCEPT
                       ![self] =
                       FindInsertPosition(current'[self], key[self])]
                  /\ nodes' =
                       [nodes EXCEPT
                       ![current_idx[self]] =
                       NewNode(ShiftRightInsert(current'[self].keys, idx'[self], key[self]), ShiftRightInsert(current'[
                             self
                           ].children, idx'[
                               self
                             ] +
                             1, last_page_idx[
                             self
                           ]), current'[
                           self
                         ].is_leaf, current'[
                             self
                           ].count +
                           1, current'[
                           self
                         ].rlink, current'[
                           self
                         ].max_val, current_idx[
                           self
                         ], current'[
                           self
                         ].level)]
                  /\ pc' = [pc EXCEPT ![self] = "Done"]
                  /\ UNCHANGED << next_free,
                        key,
                        current_idx,
                        last_page_idx,
                        drop_level,
                        sentinel,
                        overfull,
                        overfull2
                     >>
             ELSE /\ idx' =
                       [idx EXCEPT
                       ![self] =
                       FindInsertPosition(current'[self], key[self])]
                  /\ overfull' =
                       [overfull EXCEPT
                       ![self] =
                       OverfullKeys(current'[self].keys, key[self], idx'[self])]
                  /\ overfull2' =
                       [overfull2 EXCEPT
                       ![self] =
                       OverfullKeys(current'[self].children, last_page_idx[self], idx'[self] + 1)]
                  /\ sentinel' =
                       [sentinel EXCEPT
                       ![self] =
                       overfull'[self][( MAX_NUM_KEYS + 1 ) \div 2 + 1]]
                  /\ nodes' =
                       [nodes EXCEPT
                       ![next_free] =
                       NewNode(RightHalfKeys(overfull'[self]), RightHalfKeys(overfull2'[self]), current'[self].is_leaf, ( MAX_NUM_KEYS +
                               1
                           ) \div
                           2, current'[
                           self
                         ].rlink, current'[
                           self
                         ].max_val, next_free, current'[
                           self
                         ].level),
                       ![current_idx[self]] =
                       NewNode(LeftHalfKeys(overfull'[self]), LeftHalfKeys(overfull2'[self]), current'[self].is_leaf, ( MAX_NUM_KEYS +
                               1
                           ) -
                           ( ( MAX_NUM_KEYS + 1 ) \div 2 ), next_free, sentinel'[
                           self
                         ], current_idx[
                           self
                         ], current'[
                           self
                         ].level)]
                  /\ key' = [key EXCEPT ![self] = sentinel'[self]]
                  /\ last_page_idx' = [last_page_idx EXCEPT ![self] = next_free]
                  /\ next_free' = next_free + 1
                  /\ IF Len(stack[self]) = 0
                     THEN /\ drop_level' =
                               [drop_level EXCEPT
                               ![self] =
                               current'[self].level + 1]
                          /\ current_idx' =
                               [current_idx EXCEPT ![self] = root_idx]
                          /\ pc' = [pc EXCEPT ![self] = "DropToLevel"]
                     ELSE /\ pc' = [pc EXCEPT ![self] = "PropagateInsert"]
                          /\ UNCHANGED << current_idx, drop_level >>
  /\ UNCHANGED << root_idx, stack >>

PropagateInsert(self) ==
  /\ pc[self] = "PropagateInsert"
  /\ current' =
       [current EXCEPT ![self] = ReloadCurrent(current_idx[self], nodes)]
  /\ IF Len(stack[self]) = 0
     THEN /\ pc' = [pc EXCEPT ![self] = "HandleRoot"]
          /\ UNCHANGED << current_idx, stack >>
     ELSE /\ current_idx' =
               [current_idx EXCEPT ![self] = stack[self][Len(stack[self])]]
          /\ stack' =
               [stack EXCEPT
               ![self] =
               SubSeq(stack[self], 1, Len(stack[self]) - 1)]
          /\ pc' = [pc EXCEPT ![self] = "Insert"]
  /\ UNCHANGED << nodes,
        root_idx,
        next_free,
        key,
        idx,
        last_page_idx,
        drop_level,
        sentinel,
        overfull,
        overfull2
     >>

HandleRoot(self) ==
  /\ pc[self] = "HandleRoot"
  /\ current' =
       [current EXCEPT ![self] = ReloadCurrent(current_idx[self], nodes)]
  /\ IF current'[self].max_val /= UNDEFINED /\
         key[self] >= current'[self].max_val
     THEN /\ current_idx' = [current_idx EXCEPT ![self] = current'[self].rlink]
          /\ pc' = [pc EXCEPT ![self] = "HandleRoot"]
          /\ UNCHANGED << nodes,
                root_idx,
                next_free,
                key,
                idx,
                last_page_idx,
                drop_level,
                sentinel,
                overfull,
                overfull2
             >>
     ELSE /\ IF current'[self].count /= MAX_NUM_KEYS
             THEN /\ idx' =
                       [idx EXCEPT
                       ![self] =
                       FindInsertPosition(current'[self], key[self])]
                  /\ nodes' =
                       [nodes EXCEPT
                       ![current_idx[self]] =
                       NewNode(ShiftRightInsert(current'[self].keys, idx'[self], key[self]), ShiftRightInsert(current'[
                             self
                           ].children, idx'[
                               self
                             ] +
                             1, last_page_idx[
                             self
                           ]), current'[
                           self
                         ].is_leaf, current'[
                             self
                           ].count +
                           1, current'[
                           self
                         ].rlink, current'[
                           self
                         ].max_val, current_idx[
                           self
                         ], current'[
                           self
                         ].level)]
                  /\ pc' = [pc EXCEPT ![self] = "Done"]
                  /\ UNCHANGED << root_idx,
                        next_free,
                        key,
                        current_idx,
                        last_page_idx,
                        drop_level,
                        sentinel,
                        overfull,
                        overfull2
                     >>
             ELSE /\ idx' =
                       [idx EXCEPT
                       ![self] =
                       FindInsertPosition(current'[self], key[self])]
                  /\ overfull' =
                       [overfull EXCEPT
                       ![self] =
                       OverfullKeys(current'[self].keys, key[self], idx'[self])]
                  /\ overfull2' =
                       [overfull2 EXCEPT
                       ![self] =
                       OverfullKeys(current'[self].children, last_page_idx[self], idx'[self] + 1)]
                  /\ sentinel' =
                       [sentinel EXCEPT
                       ![self] =
                       overfull'[self][( MAX_NUM_KEYS + 1 ) \div 2 + 1]]
                  /\ key' = [key EXCEPT ![self] = sentinel'[self]]
                  /\ last_page_idx' = [last_page_idx EXCEPT ![self] = next_free]
                  /\ IF current_idx[self] = root_idx
                     THEN /\ nodes' =
                               [nodes EXCEPT
                               ![next_free] =
                               NewNode(RightHalfKeys(overfull'[self]), RightHalfKeys(overfull2'[
                                     self
                                   ]), current'[
                                   self
                                 ].is_leaf, ( MAX_NUM_KEYS +
                                       1
                                   ) \div
                                   2, current'[
                                   self
                                 ].rlink, current'[
                                   self
                                 ].max_val, next_free, current'[
                                   self
                                 ].level),
                               ![current_idx[self]] =
                               NewNode(LeftHalfKeys(overfull'[self]), LeftHalfKeys(overfull2'[
                                     self
                                   ]), current'[
                                   self
                                 ].is_leaf, ( MAX_NUM_KEYS +
                                       1
                                   ) -
                                   ( ( MAX_NUM_KEYS + 1 ) \div 2 ), next_free, sentinel'[
                                   self
                                 ], current_idx[
                                   self
                                 ], current'[
                                   self
                                 ].level),
                               ![next_free + 1] =
                               NewNode(<< key'[self], 0, 0 >>, << current_idx[self],
                                    last_page_idx'[self],
                                    0,
                                    0
                                 >>, FALSE, 1, UNDEFINED, UNDEFINED, next_free +
                                   1, current'[
                                     self
                                   ].level +
                                   1)]
                          /\ root_idx' = next_free + 1
                          /\ next_free' = next_free + 2
                          /\ pc' = [pc EXCEPT ![self] = "Done"]
                          /\ UNCHANGED << current_idx, drop_level >>
                     ELSE /\ nodes' =
                               [nodes EXCEPT
                               ![next_free] =
                               NewNode(RightHalfKeys(overfull'[self]), RightHalfKeys(overfull2'[
                                     self
                                   ]), current'[
                                   self
                                 ].is_leaf, ( MAX_NUM_KEYS +
                                       1
                                   ) \div
                                   2, current'[
                                   self
                                 ].rlink, current'[
                                   self
                                 ].max_val, next_free, current'[
                                   self
                                 ].level),
                               ![current_idx[self]] =
                               NewNode(LeftHalfKeys(overfull'[self]), LeftHalfKeys(overfull2'[
                                     self
                                   ]), current'[
                                   self
                                 ].is_leaf, ( MAX_NUM_KEYS +
                                       1
                                   ) -
                                   ( ( MAX_NUM_KEYS + 1 ) \div 2 ), next_free, sentinel'[
                                   self
                                 ], current_idx[
                                   self
                                 ], current'[
                                   self
                                 ].level)]
                          /\ next_free' = next_free + 1
                          /\ drop_level' =
                               [drop_level EXCEPT
                               ![self] =
                               current'[self].level + 1]
                          /\ current_idx' =
                               [current_idx EXCEPT ![self] = root_idx]
                          /\ pc' = [pc EXCEPT ![self] = "DropToLevel"]
                          /\ UNCHANGED root_idx
  /\ stack' = stack

Thread(self) ==
  Start(self) \/ DropToLevel(self) \/ Insert(self) \/ PropagateInsert(self) \/
    HandleRoot(self)

(* Allow infinite stuttering to prevent deadlock on termination. *)
Terminating ==
  /\ \A self \in ProcSet: pc[self] = "Done"
  /\ UNCHANGED vars

Next == ( \E self \in 1 .. NUM_THREADS: Thread(self) ) \/ Terminating

Spec == Init /\ [][Next]_vars

Termination == <>( \A self \in ProcSet: pc[self] = "Done" )

\* END TRANSLATION
AllDone == \A self \in ProcSet: pc[self] = "Done"

NotZeroCount(x) == Len(SelectSeq(nodes[x].keys, LAMBDA k:k # 0))

TotalLeafKeys ==
  LET RECURSIVE SumRec(_)
      SumRec(s) ==
        IF s = {}
        THEN 0
        ELSE LET x == CHOOSE x \in s: TRUE
          IN ( IF nodes[x].level = 0 /\ NotZeroCount(x) = nodes[x].count
                  THEN nodes[x].count
                  ELSE 0
                ) +
                SumRec(s \ { x })
  IN SumRec(1 .. next_free - 1)

CountCheck == AllDone => TotalLeafKeys = NUM_THREADS

IsSorted(seq) == \A i, j \in 1 .. Len(seq): i < j => seq[i] <= seq[j]

AllNodesSorted ==
  \A n \in 1 .. ( next_free - 1 ):
    IsSorted(SubSeq(nodes[n].keys, 1, nodes[n].count))

CheckMaxValue(max, node_idx) ==
  \A i \in 1 .. nodes[node_idx].count: nodes[node_idx].keys[i] < max

CheckMinValue(min, node_idx) ==
  \A i \in 1 .. nodes[node_idx].count: nodes[node_idx].keys[i] >= min

AreChildrenKeysSortedByParent ==
  \A n \in 1 .. ( next_free - 1 ):
    nodes[n].level > 0 =>
      \A j \in 1 .. nodes[n].count:
        LET k == nodes[n].keys[j]
            p1 == nodes[n].children[j]
            p2 == nodes[n].children[j + 1]
        IN CheckMaxValue(k, p1) /\ CheckMinValue(k, p2)

SortedCheck == AllDone => AllNodesSorted /\ AreChildrenKeysSortedByParent
====

---- MODULE btree_index ----
EXTENDS Integers, Sequences, FiniteSets, TLC

CONSTANTS NUM_THREADS, VALUES


\* ============================================================
\* Pure TLA+ operators for B-tree data manipulation
\* ============================================================
EmptyNode(id) ==
  [ keys |-> << 0, 0, 0 >>,
    children |-> << 0, 0, 0, 0 >>,
    is_leaf |-> TRUE,
    count |-> 0,
    rlink |-> 0,
    max_val |-> 0,
    idx |-> id
  ]

\* Find insert position: first index where keys[i] >= key, or count+1
FindChild(node, key) ==
  IF node.count = 0
  THEN 1
  ELSE CHOOSE i \in 1 .. ( node.count + 1 ):
      /\ \A j \in 1 .. ( i - 1 ): node.keys[j] < key
      /\ ( i > node.count \/ node.keys[i] >= key )

\* Insert key at position idx, shifting keys right
InsertKey(node, idx, key) ==
  [node EXCEPT
  !.keys =
  [k \in 1 .. 3 |->
    IF k < idx
    THEN node.keys[k]
    ELSE IF k = idx
      THEN key
      ELSE IF k <= node.count + 1 THEN node.keys[k - 1] ELSE 0
  ],
  !.count =
  node.count + 1]

\* Insert child ref at position idx, shifting children right
InsertChild(node, idx, ref) ==
  [node EXCEPT
  !.children =
  [k \in 1 .. 4 |->
    IF k < idx
    THEN node.children[k]
    ELSE IF k = idx
      THEN ref
      ELSE IF k <= node.count + 2 THEN node.children[k - 1] ELSE 0
  ]]

\* Split a node: returns [left, right] where right gets upper half
SplitNode(node, idx, key, prev) ==
  LET mid == node.count \div 2 + 1
      old_count == node.count
      left_count == old_count \div 2
      right_count == old_count - ( mid - 1 )
      right ==
        [ keys |->
            [i \in 1 .. 3 |->
              IF i <= right_count THEN node.keys[i + mid - 1] ELSE 0
            ],
          children |->
            [i \in 1 .. 4 |->
              IF i <= right_count + 1 THEN node.children[i + mid - 1] ELSE 0
            ],
          is_leaf |-> node.is_leaf,
          count |-> right_count,
          rlink |-> node.rlink,
          max_val |-> 0,
          idx |-> 0
        \* caller sets this
        ]
      left == [node EXCEPT !.count = left_count, !.rlink = 0\* caller sets this
        ]
      \* Insert into correct half
      inserted_left ==
        IF idx <= mid
        THEN InsertChild(InsertKey(left, idx, key), idx + 1, prev)
        ELSE left
      inserted_right ==
        IF idx > mid
        THEN InsertChild(InsertKey(right, idx - left_count, key), idx - left_count + 1, prev)
        ELSE right
  IN [ left |-> inserted_left,
        right |-> inserted_right,
        sep |-> inserted_right.keys[1]
      ]





\* ============================================================
\* PlusCal: only control flow and concurrency
\* ============================================================
(*--algorithm btree_index

variables
    nodes = [i \in 1..100 |-> EmptyNode(i)],
    num_keys = 3,
    root = 0,
    next_free = 1;

process Thread \in 1..NUM_THREADS
variables 
    key = 0, cur = nodes[1], node_stack = <<>>, 
    separator = 0, prev_node = 0, idx = 0, ptr = 0,
    new_node = nodes[1], new_root = nodes[1], 
    top = 0, result = [left |-> nodes[1], right |-> nodes[1], sep |-> 0];

begin

    Start:
        with val \in 1..VALUES do
            key := val;
        end with;
    
    GetRoot:
        if root = 0 then
            root := next_free;
            next_free := next_free + 1;
        end if;
        cur := nodes[root];

    FetchPage:
        skip;

    Insert:
        if cur.max_val /= 0 /\ cur.max_val <= key then
            cur := nodes[cur.rlink];
            goto FetchPage;
        end if;

    ScanInsert:
        idx := FindChild(cur, key);
        ptr := cur.children[idx];

        if ~cur.is_leaf then
            node_stack := Append(node_stack, cur.idx);
            cur := nodes[ptr];
            goto FetchPage;
        end if;

    LeafInsert:
        if cur.count < num_keys then
            cur := InsertKey(cur, idx, key);
            nodes[cur.idx] := cur;
            goto Done;
        end if;

    LeafSplit:
        result := SplitNode(cur, idx, key, prev_node);

        \* set up new node
        new_node := result.right;
        new_node.idx := next_free;
        new_node.rlink := cur.rlink;
        next_free := next_free + 1;

    LeafSplitFinish:
        \* update left node
        cur := result.left;
        cur.rlink := new_node.idx;

        separator := result.sep;
        prev_node := new_node.idx;

        nodes[cur.idx] := cur;
        nodes[new_node.idx] := new_node;
        goto Backtrack;

    Backtrack:
        if Len(node_stack) = 0 then
            \* create new root
            new_root := EmptyNode(next_free);
            new_root.is_leaf := FALSE;
            next_free := next_free + 1;
            goto NewRoot;
        end if;

        top := node_stack[Len(node_stack)];
        cur := nodes[top];
        node_stack := SubSeq(node_stack, 1, Len(node_stack) - 1);
        goto InsertInter;

    NewRoot:
        new_root.keys[1] := separator;
        new_root.count := 1;
        new_root.children[1] := root;
        new_root.children[2] := prev_node;
        nodes[new_root.idx] := new_root;
        root := new_root.idx;
        goto Done;

    InsertInter:
        if cur.max_val /= 0 /\ cur.max_val <= key then
            cur := nodes[cur.rlink];
            goto InsertInter;
        end if;

    ScanInter:
        idx := FindChild(cur, separator);

        if cur.count < num_keys then
            cur := InsertKey(cur, idx, separator);
            cur := InsertChild(cur, idx + 1, prev_node);
            nodes[cur.idx] := cur;
            goto Done;
        end if;

    InterSplit:
        result := SplitNode(cur, idx, separator, prev_node);

        new_node := result.right;
        new_node.idx := next_free;
        new_node.rlink := cur.rlink;
        next_free := next_free + 1;

    InterSplitFinish:
        cur := result.left;
        cur.rlink := new_node.idx;

        separator := result.sep;
        prev_node := new_node.idx;

        nodes[cur.idx] := cur;
        nodes[new_node.idx] := new_node;
        goto Backtrack;
      
end process;

end algorithm; *)
\* BEGIN TRANSLATION
\* END TRANSLATION
====

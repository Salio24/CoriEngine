# GDB pretty printers

| File | Covers |
| --- | --- |
| `spscring_printer.py` | `Cori::Threading::SPSCRing<T>` |
| `tbb_printers.py` | `tbb::concurrent_vector<T>`, `tbb::concurrent_unordered_map<K, V>`, `tbb::concurrent_unordered_multimap<K, V>` |

## Setup

Source them from `~/.gdbinit`:

```gdb
source /home/salio/Projects/CoriEngine/Tools/gdb/spscring_printer.py
source /home/salio/Projects/CoriEngine/Tools/gdb/tbb_printers.py
```

Or per-session, `source Tools/gdb/tbb_printers.py`. Re-sourcing an already
loaded file is safe. Nothing needs to be built or linked — the printers read
member layout out of the debug info.

## oneTBB containers

oneTBB ships no printers, so without these a `concurrent_vector` shows as a
segment table of `std::atomic<T*>` and a `concurrent_unordered_map` as a
split-ordered intrusive list. With them:

```
(gdb) p m_AssetStatuses
$1 = tbb::concurrent_vector of length 5 = {std::atomic<AssetStatus> = { AssetStatus::Loaded }, ...}

(gdb) p m_AssetDatabase
$2 = tbb::concurrent_unordered_map with 3 elements = {[160] = {slot = 0, ...}, ...}
```

Both respect `set print elements`, so a 100k-element vector will not flood the
terminal.

### `tbb-unwrap-atomics`

The asset manager stores nearly every parallel-array column as
`concurrent_vector<std::atomic<X>>`, where the per-element `std::atomic<X> = {}`
wrapper buries the data. Off by default; turn it on to print just the value:

```
(gdb) set tbb-unwrap-atomics on
(gdb) p m_RefCounts
$3 = tbb::concurrent_vector of length 4 = {1, 0, 3, 1}
```

It applies to vector elements and to mapped values, not to keys.

### Reading a container while other threads run

These print a snapshot assembled with plain loads, so on a container another
thread is actively mutating:

- `concurrent_vector` size comes from `my_size`, which a `grow_by` bumps
  *before* constructing the elements. Trailing entries can be raw memory.
- A `concurrent_unordered_map` walk can miss a concurrent insert or see a node
  mid-unlink.

Both bound their walks, so a torn or corrupt container truncates instead of
looping forever. Values are trustworthy when the world is stopped and no
container operation was in flight.

## Version coupling

`tbb_printers.py` reads oneTBB-internal members (`my_segment_table`,
`my_first_block`, `my_head`, `my_order_key`) and was verified against the
vendored **oneTBB v2023.1.0** in `Engine/thirdparty/oneTBB`. The container
internals are documented at the top of the file; if a oneTBB bump makes a
printer return nothing, re-check the layout there first.

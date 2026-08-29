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

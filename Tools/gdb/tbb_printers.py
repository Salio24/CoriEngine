# GDB pretty printers for oneTBB concurrent containers.
#
# oneTBB ships no printers of its own, so tbb::concurrent_vector and
# tbb::concurrent_unordered_map print as raw segment tables / split-ordered
# lists. These reconstruct the logical contents.
#
# Verified against the vendored oneTBB v2023.1.0 (Engine/thirdparty/oneTBB).
#
# concurrent_vector -- derives from detail::d1::segment_table:
#   atomic<atomic<T*>*>  my_segment_table  -> active table, embedded or heap
#   atomic<T*>           my_embedded_table[pointers_per_embedded_table]
#   atomic<size_type>    my_first_block    -> segments sharing one allocation
#   atomic<size_type>    my_size
#   Element i lives at my_segment_table[segment_index_of(i)][i], where
#   segment_index_of(i) == log2(i|1). Segments past the first block store a
#   base-adjusted pointer (alloc - segment_base(seg)) so that the *global*
#   index subscripts correctly; first-block segments all point at one
#   contiguous allocation starting at global index 0, so the same rule holds.
#   A segment pointer of 1 is segment_allocation_failure_tag.
#
# concurrent_unordered_map -- derives from detail::d2::concurrent_unordered_base:
#   atomic<size_type>  my_size
#   atomic<size_type>  my_bucket_count
#   list_node_type     my_head     -> sentinel of the split-ordered list
#   Every element is on one intrusive singly-linked list threaded through
#   my_next. Nodes with an even my_order_key are bucket dummies and carry no
#   value; odd keys are value_nodes holding the pair. So iteration is just a
#   list walk that skips dummies -- bucket structure can be ignored entirely.
#
# Usage: see Tools/gdb/README.md

import gdb
import gdb.printing

# Segments in the inline table before a heap table is allocated
# (embedded_table_num_segments in concurrent_vector.h). Read from debug info
# when available; this is only the fallback.
EMBEDDED_TABLE_NUM_SEGMENTS = 3

# pointers_per_long_table == sizeof(size_type) * 8
POINTERS_PER_LONG_TABLE = 64

# segment_table::segment_allocation_failure_tag
SEGMENT_ALLOCATION_FAILURE_TAG = 1


def _atomic_value(field):
    """Read the stored value out of a libstdc++ std::atomic<...> as a gdb.Value."""
    try:
        return field['_M_i']
    except gdb.error:
        pass
    try:
        # std::atomic<T*> stores through __atomic_base<T*>::_M_p
        return field['_M_b']['_M_p']
    except gdb.error:
        return field


def _atomic_int(field):
    return int(_atomic_value(field))


def _member(val, name):
    """Fetch a member, falling back to an explicit base-class cast.

    concurrent_vector inherits from segment_table privately; GDB normally
    resolves that anyway, but a cast keeps this working if it ever does not.
    """
    try:
        return val[name]
    except gdb.error:
        pass
    for f in val.type.strip_typedefs().fields():
        if f.is_base_class:
            try:
                return val.cast(f.type)[name]
            except gdb.error:
                continue
    raise gdb.error('no member %s' % name)


def _base_type_name(val):
    for f in val.type.strip_typedefs().fields():
        if f.is_base_class:
            return f.type.name or str(f.type)
    return None


def _segment_index_of(index):
    # log2(index | 1)
    return int(index | 1).bit_length() - 1


class UnwrapAtomics(gdb.Parameter):
    """Print std::atomic elements of TBB containers as their bare value.

    Off by default. CoriEngine stores nearly every parallel-array column as
    concurrent_vector<std::atomic<X>>, where the per-element
    'std::atomic<X> = { v }' wrapper drowns out the actual data; turn this on
    to show just v."""

    def __init__(self):
        super(UnwrapAtomics, self).__init__(
            'tbb-unwrap-atomics', gdb.COMMAND_DATA, gdb.PARAM_BOOLEAN)
        self.value = False

    def get_set_string(self):
        return 'TBB atomic element unwrapping is %s.' % ('on' if self.value else 'off')

    def get_show_string(self, svalue):
        return 'TBB atomic element unwrapping is %s.' % svalue


try:
    _unwrap_atomics = UnwrapAtomics()
except RuntimeError:
    # Already registered by an earlier source of this file.
    _unwrap_atomics = None


def _maybe_unwrap(value):
    if _unwrap_atomics is None or not _unwrap_atomics.value:
        return value
    name = value.type.strip_typedefs().name or ''
    if name.startswith('std::atomic<') or name.startswith('std::__atomic_base<'):
        return _atomic_value(value)
    return value


class ConcurrentVectorPrinter:
    def __init__(self, val):
        self.val = val

    def _size(self):
        return _atomic_int(_member(self.val, 'my_size'))

    def _table(self):
        """(table pointer, number of segments it holds), or (None, 0)."""
        table = _atomic_value(_member(self.val, 'my_segment_table'))
        if int(table) == 0:
            return None, 0

        embedded = _member(self.val, 'my_embedded_table')
        try:
            embedded_count = int(self.val['pointers_per_embedded_table'])
        except (gdb.error, RuntimeError):
            embedded_count = EMBEDDED_TABLE_NUM_SEGMENTS

        if int(table) == int(embedded[0].address):
            return table, embedded_count
        return table, POINTERS_PER_LONG_TABLE

    def to_string(self):
        try:
            size = self._size()
        except gdb.error:
            return 'tbb::concurrent_vector <uninitialized>'
        return 'tbb::concurrent_vector of length %d' % size

    def children(self):
        try:
            size = self._size()
            table, num_segments = self._table()
        except gdb.error:
            return
        if table is None:
            return

        for i in range(size):
            seg = _segment_index_of(i)
            if seg >= num_segments:
                # my_size ran ahead of the table (a grow_by in flight on
                # another thread, or corruption). Stop rather than read past.
                return
            base = _atomic_value(table[seg])
            addr = int(base)
            if addr == 0 or addr == SEGMENT_ALLOCATION_FAILURE_TAG:
                return
            yield ('[%d]' % i, _maybe_unwrap((base + i).dereference()))

    def display_hint(self):
        return 'array'


class ConcurrentUnorderedMapPrinter:
    def __init__(self, val, name):
        self.val = val
        self.name = name

    def _size(self):
        return _atomic_int(_member(self.val, 'my_size'))

    def _value_node_type(self):
        base = _base_type_name(self.val)
        if base is None:
            return None
        try:
            return gdb.lookup_type(base + '::value_node_type').strip_typedefs()
        except gdb.error:
            return None

    def to_string(self):
        try:
            size = self._size()
        except gdb.error:
            return '%s <uninitialized>' % self.name
        return '%s with %d elements' % (self.name, size)

    def _node_value(self, node):
        """The pair stored in a value_node, which lives in an anonymous union."""
        try:
            return node['my_value']
        except gdb.error:
            pass
        for f in node.type.strip_typedefs().fields():
            if f.name is None and not f.is_base_class:
                try:
                    return node[f]['my_value']
                except gdb.error:
                    continue
        raise gdb.error('cannot read value_node storage')

    def children(self):
        try:
            size = self._size()
            head = _member(self.val, 'my_head')
            bucket_count = _atomic_int(_member(self.val, 'my_bucket_count'))
        except gdb.error:
            return
        value_node_type = self._value_node_type()
        if value_node_type is None:
            return
        value_node_ptr = value_node_type.pointer()

        # Dummy nodes are bounded by the bucket count and value nodes by size,
        # so this bounds a corrupt list without truncating a healthy one.
        max_nodes = 2 * (size + bucket_count) + 64

        node = _atomic_value(head['my_next'])
        n = 0
        visited = 0
        while int(node) != 0 and visited < max_nodes:
            visited += 1
            deref = node.dereference()
            # Even order key == bucket dummy, carries no value.
            if int(deref['my_order_key']) & 1:
                value = self._node_value(node.cast(value_node_ptr).dereference())
                try:
                    yield ('[%d]' % n, value['first'])
                    yield ('[%d]' % n, _maybe_unwrap(value['second']))
                except gdb.error:
                    yield ('[%d]' % n, value)
                n += 1
            node = _atomic_value(deref['my_next'])

    def display_hint(self):
        return 'map'


def _make_map_printer(val):
    return ConcurrentUnorderedMapPrinter(val, 'tbb::concurrent_unordered_map')


def _make_multimap_printer(val):
    return ConcurrentUnorderedMapPrinter(val, 'tbb::concurrent_unordered_multimap')


def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter('oneTBB')
    # Containers live in tbb::detail::dN:: and are pulled into tbb:: by a
    # using-declaration, so match both the qualified and inline forms.
    pp.add_printer('concurrent_vector',
                   '^tbb::(detail::d[0-9]+::)?concurrent_vector<.*>$',
                   ConcurrentVectorPrinter)
    pp.add_printer('concurrent_unordered_map',
                   '^tbb::(detail::d[0-9]+::)?concurrent_unordered_map<.*>$',
                   _make_map_printer)
    pp.add_printer('concurrent_unordered_multimap',
                   '^tbb::(detail::d[0-9]+::)?concurrent_unordered_multimap<.*>$',
                   _make_multimap_printer)
    return pp


def register(objfile=None):
    gdb.printing.register_pretty_printer(
        objfile if objfile is not None else gdb.current_objfile(),
        build_pretty_printer(),
        replace=True)


# Auto-register when sourced directly (e.g. from ~/.gdbinit or `source ...`).
register()

# GDB pretty printer for Cori::Threading::SPSCRing<T>
#
# See SPSCRing.hpp. Layout recap:
#   T*                   m_Storage      -> backing buffer, (m_Capacity + 2*s_SlotPadding) slots
#   uint64_t             m_Capacity     -> internal capacity (usable capacity == m_Capacity - 1)
#   atomic<uint64_t>     m_Head         -> next write index, in [0, m_Capacity)
#   uint64_t             m_HeadCache
#   atomic<uint64_t>     m_Tail         -> next read (Front) index, in [0, m_Capacity)
#   uint64_t             m_TailCache
#   static s_SlotPadding -> leading padding slots; element i lives at m_Storage[i + s_SlotPadding]
#
# Live elements are the indices [tail, head) walked with wraparound at m_Capacity.
#
# Usage: see Tools/gdb/README.md

import gdb
import gdb.printing

# std::hardware_destructive_interference_size as used to compute s_SlotPadding.
# GCC on x86-64/AArch64 uses 64. Change here if you target a platform that differs.
HW_DESTRUCTIVE_INTERFERENCE_SIZE = 64


def _atomic_load(field):
    """Read the stored integer out of a libstdc++ std::atomic<...>."""
    try:
        return int(field['_M_i'])
    except gdb.error:
        # Fallback: some builds let you convert the atomic directly.
        return int(field)


class SPSCRingPrinter:
    def __init__(self, val):
        self.val = val

    def _elem_type(self):
        t = self.val.type
        if t.code == gdb.TYPE_CODE_REF:
            t = t.target()
        return t.strip_typedefs().template_argument(0)

    def _slot_padding(self, elem_type):
        # Prefer the real static constexpr value if the debug info exposes it,
        # otherwise recompute it: (HW_INTERFERENCE - 1) / sizeof(T) + 1.
        try:
            return int(self.val['s_SlotPadding'])
        except (gdb.error, RuntimeError):
            pass
        tsize = elem_type.sizeof or 1
        return (HW_DESTRUCTIVE_INTERFERENCE_SIZE - 1) // tsize + 1

    def _head_tail_cap(self):
        head = _atomic_load(self.val['m_Head'])
        tail = _atomic_load(self.val['m_Tail'])
        cap = int(self.val['m_Capacity'])
        return head, tail, cap

    def to_string(self):
        try:
            head, tail, cap = self._head_tail_cap()
        except gdb.error:
            return 'Cori::Threading::SPSCRing <uninitialized>'
        size = head - tail
        if size < 0:
            size += cap
        return 'Cori::Threading::SPSCRing of length %d, capacity %d' % (size, cap - 1)

    def children(self):
        storage = self.val['m_Storage']
        if int(storage) == 0:
            return
        head, tail, cap = self._head_tail_cap()
        padding = self._slot_padding(self._elem_type())

        idx = tail
        n = 0
        # Bound the walk by cap so corrupt head/tail can't loop forever.
        while idx != head and n < cap:
            yield ('[%d]' % n, (storage + (padding + idx)).dereference())
            n += 1
            idx += 1
            if idx == cap:
                idx = 0

    def display_hint(self):
        return 'array'


def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter('CoriEngine')
    pp.add_printer('SPSCRing',
                   '^Cori::Threading::SPSCRing<.*>$',
                   SPSCRingPrinter)
    return pp


def register(objfile=None):
    gdb.printing.register_pretty_printer(
        objfile if objfile is not None else gdb.current_objfile(),
        build_pretty_printer(),
        replace=True)


# Auto-register when sourced directly (e.g. from ~/.gdbinit or `source ...`).
register()

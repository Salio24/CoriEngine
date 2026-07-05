#include "AssetHandleAllocator.hpp"

Cori::Core::AssetHandleAllocator<uint64_t, [](Cori::Core::Handle<uint64_t> handle, uint64_t id){}> test;
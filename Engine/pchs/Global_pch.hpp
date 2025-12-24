#ifndef GLOBAL_PCH
#define GLOBAL_PCH

#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <algorithm>
#include <random>
#include <map>
#include <chrono>
#include <iostream>
#include <system_error>
#include <filesystem>
#include <string_view>
#include <cstdint>
#include <atomic>
#include <compare>
#include <stdexcept>
#include <system_error>
#include <expected>
#include <typeinfo> 
#include <utility> 
#include <typeindex>
#include <type_traits>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <ostream>
#include <thread>
#include <fstream>
#include <concepts>
#include <numeric>
#include <numbers>
#include <bit>
#include <any>
#include <variant>
#include <ranges>
#include <limits>
#include <print>
#include <bitset>
#include <queue>
#include <future>
#include <condition_variable>
#include <source_location>

#ifdef CORI_USE_SMID
	#define GLM_FORCE_INTRINSICS
#endif

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtx/string_cast.hpp>
#include <imgui.h>
#include "../src/Utility/Macros.hpp"
#include "../src/Core/Logger.hpp"
#include "../src/Profiling/Profiler.hpp"
#include "../src/Core/Uuid.hpp"
#include "../src/Core/Error.hpp"
#include "../src/GlobalDefines.hpp"
#include "../src/Core/ByteType.hpp"
#include "../src/Math/Alignment.hpp"

#endif

#pragma once

namespace Cori {
#define CORI_BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1)

}
#pragma once

#include <stdexec/execution.hpp>

namespace allio::detail {
namespace execution_namespaces {

namespace ex = stdexec;
namespace fex = exec;

} // namespace execution_namespaces

using namespace execution_namespaces;

} // namespace allio::detail

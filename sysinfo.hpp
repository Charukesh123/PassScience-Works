#pragma once
#include <string>
namespace wb {
struct SysInfo { std::string cpu, compiler, flags; int logical = 1, physical = 1; double ramGiB = 0; };
SysInfo querySysInfo();
}

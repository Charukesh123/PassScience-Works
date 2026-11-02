#include "sysinfo.hpp"
#include <algorithm>
#include <thread>
#include <fstream>
#include <set>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifndef WB_FLAGS
#define WB_FLAGS "unknown (set WB_FLAGS in CMake)"
#endif
namespace wb {
SysInfo querySysInfo() {
    SysInfo s; s.logical = (int)std::max(1u, std::thread::hardware_concurrency()); s.physical = s.logical; s.flags = WB_FLAGS;
#if defined(_MSC_VER)
    s.compiler = "MSVC " + std::to_string(_MSC_VER);
#else
    s.compiler = std::string("GCC/Clang ") + __VERSION__;
#endif
#ifdef _WIN32
    MEMORYSTATUSEX m{}; m.dwLength = sizeof m; if (GlobalMemoryStatusEx(&m)) s.ramGiB = m.ullTotalPhys / 1073741824.0;
    char buf[256]; DWORD n = sizeof buf;
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString", RRF_RT_REG_SZ, nullptr, buf, &n) == ERROR_SUCCESS) s.cpu = buf;
    DWORD len = 0; GetLogicalProcessorInformation(nullptr, &len);
    if (len) { std::string raw(len, 0); auto* p = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)raw.data();
        if (GetLogicalProcessorInformation(p, &len)) { int c = 0; for (DWORD i = 0; i < len / sizeof *p; ++i) if (p[i].Relationship == RelationProcessorCore) ++c; if (c) s.physical = c; } }
#else
    std::ifstream f("/proc/cpuinfo"); std::string ln; std::set<std::string> cores;  std::string phys, core;
    while (std::getline(f, ln)) {
        auto c = ln.find(':'); if (c == std::string::npos) continue; std::string k = ln.substr(0, c), v = ln.substr(std::min(ln.size(), c + 2));  // value may be empty
        if (k.rfind("model name", 0) == 0 && s.cpu.empty()) s.cpu = v;
        if (k.rfind("physical id", 0) == 0) phys = v;
        if (k.rfind("core id", 0) == 0) { core = v; cores.insert(phys + "/" + core); }
    }
    if (!cores.empty()) s.physical = (int)cores.size();
    std::ifstream mi("/proc/meminfo"); long kb = 0; std::string k; if (mi >> k >> kb) s.ramGiB = kb / 1048576.0;
#endif
    if (s.cpu.empty()) s.cpu = "unknown";
    return s;
}
}

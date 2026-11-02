// Tiny JSON emit helpers (transport layer only; no numerics here).
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
namespace wbj {
inline std::string num(double v) { if (!std::isfinite(v)) return "null"; char b[40]; std::snprintf(b, sizeof b, "%.12g", v); return b; }
inline std::string str(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) { if (c == '"' || c == '\\') { o += '\\'; o += (char)c; } else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; } else o += (char)c; }
    return o + "\"";
}
template <class T> std::string arr(const std::vector<T>& v) { std::string o = "["; for (size_t i = 0; i < v.size(); ++i) { if (i) o += ','; o += num((double)v[i]); } return o + "]"; }
}

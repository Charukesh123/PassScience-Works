#include "devices.hpp"
#include <cstdio>
namespace wb {
MockBench::MockBench() {
    dev_["mcu0"]  = {"mcu0", "SIM-MCU (Cortex-M class, 3.3 V)", "Microcontroller board (application/control)", "USB-UART (CDC) to host", DevType::MCU,  {"uart-telemetry", "gpio-3v3", "swd-debug-port"}, "disconnected"};
    dev_["fpga0"] = {"fpga0", "SIM-FPGA (3.3 V I/O, 1.0 V core)", "FPGA board (logic/accelerator, configured via JTAG)", "USB-UART (CDC) to host; JTAG for configuration", DevType::FPGA, {"uart-telemetry", "jtag-config-target", "status-pins"}, "disconnected"};
    dev_["jtag0"] = {"jtag0", "SIM-JTAG adapter (TCK/TMS/TDI/TDO, VTref)", "Debug/programming interface (NOT a compute board)", "USB to host; 4-wire JTAG + VTref to targets", DevType::JTAG, {"jtag-chain-scan", "tck-config", "vtref-sense"}, "disconnected"};
}
std::vector<DeviceInfo> MockBench::discover() const { std::vector<DeviceInfo> v; for (auto& p : dev_) v.push_back(p.second); return v; }
OpResult MockBench::connect(const std::string& id) {
    auto it = dev_.find(id); if (it == dev_.end()) return {false, "UNKNOWN_DEVICE", "No such device: " + id};
    int a = ++attempts_[id];
    if (id == "fpga0" && a == 1) { note("[SIM] fpga0 connect attempt 1: timeout after 2000 ms (injected)"); return {false, "TIMEOUT", "SIMULATED timeout (2000 ms) waiting for FPGA UART banner. Recoverable: check power/boot jumper, then retry."}; }
    it->second.state = "connected"; note("[SIM] " + id + " connected (attempt " + std::to_string(a) + ")");
    return {true, "OK", "SIMULATED connection established."};
}
OpResult MockBench::disconnect(const std::string& id) {
    auto it = dev_.find(id); if (it == dev_.end()) return {false, "UNKNOWN_DEVICE", "No such device: " + id};
    it->second.state = "disconnected"; note("[SIM] " + id + " disconnected"); return {true, "OK", "SIMULATED disconnect."};
}
OpResult MockBench::telemetry(const std::string& id, Telemetry& t) {
    auto it = dev_.find(id); if (it == dev_.end()) return {false, "UNKNOWN_DEVICE", "No such device: " + id};
    if (it->second.state != "connected") return {false, "NOT_CONNECTED", "Connect " + id + " first."};
    int s = ++seq_[id]; t.device = id; t.seq = s; char b[64];
    auto f = [&](const char* k, const char* fmt, double v) { std::snprintf(b, sizeof b, fmt, v); t.fields.push_back({k, b}); };
    if (it->second.type == DevType::MCU) { f("vdd_V", "%.2f", 3.30); f("die_temp_C", "%.1f", 31.0 + 0.5 * (s % 4)); f("uptime_s", "%.0f", 10.0 * s); }
    else if (it->second.type == DevType::FPGA) { f("vccint_V", "%.2f", 1.00); f("vccio_V", "%.2f", 3.30); t.fields.push_back({"done_pin", "1"}); f("die_temp_C", "%.1f", 40.0 + 0.5 * (s % 4)); }
    else { f("vtref_V", "%.2f", 3.30); f("tck_kHz", "%.0f", 1000.0); t.fields.push_back({"idcode_chain", "0x4BA00477 (SIM)"}); }
    return {true, "OK", "SIMULATED telemetry sample."};
}
}

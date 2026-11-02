// Task E: SIMULATED device adapters (no real hardware is touched).
#pragma once
#include <string>
#include <vector>
#include <map>
namespace wb {
enum class DevType { MCU, FPGA, JTAG };
struct DeviceInfo {
    std::string id, name, role, interfaceDesc;   // role: what this device is for
    DevType type; std::vector<std::string> capabilities;
    std::string state;                           // disconnected | connected
};
struct OpResult { bool ok; std::string code, message; };   // code: OK | TIMEOUT | NOT_CONNECTED | UNKNOWN_DEVICE
struct Telemetry { std::string device; long seq; std::vector<std::pair<std::string, std::string>> fields; };

class MockBench {
public:
    MockBench();
    std::vector<DeviceInfo> discover() const;
    OpResult connect(const std::string& id);          // FPGA adapter times out on its 1st attempt (injected, recoverable)
    OpResult disconnect(const std::string& id);
    OpResult telemetry(const std::string& id, Telemetry& out);
    std::string log() const { return log_; }
private:
    std::map<std::string, DeviceInfo> dev_;
    std::map<std::string, int> attempts_, seq_;
    std::string log_;
    void note(const std::string& s) { log_ += s + "\n"; }
};
}

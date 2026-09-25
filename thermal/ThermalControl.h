// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <aidl/vendor/lineage/thermal/BnThermalControl.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace aidl::vendor::lineage::thermal {

class ThermalControl : public BnThermalControl {
  public:
    ThermalControl();

    ndk::ScopedAStatus isSupported(bool* _aidl_return) override;
    ndk::ScopedAStatus getCpuTemperature(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getCpuLimit(int32_t* _aidl_return) override;
    ndk::ScopedAStatus setCpuLimit(int32_t millidegC) override;
    ndk::ScopedAStatus clearCpuLimit() override;

  private:
    struct Policy {
        std::string maxPath;
        std::vector<int32_t> freqs;
        int32_t vendorMax;
        int32_t ourMax;
    };

    void discover();
    void monitor();
    void apply();
    void restore();
    int32_t pick(const Policy& policy);
    int32_t readTemperature();
    int32_t readInt(const std::string& path);
    std::string readString(const std::string& path);
    bool writeInt(const std::string& path, int32_t value);

    std::vector<std::string> mZones;
    std::vector<Policy> mPolicies;
    std::mutex mLock;
    std::atomic<int32_t> mLimit{0};
    std::atomic<bool> mApplied{false};
    std::thread mThread;
};

}

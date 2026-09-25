// SPDX-License-Identifier: Apache-2.0

#include "ThermalControl.h"

#include <android/binder_status.h>

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <utility>

namespace aidl::vendor::lineage::thermal {

using namespace std::chrono_literals;

ThermalControl::ThermalControl() {
    discover();
    mThread = std::thread(&ThermalControl::monitor, this);
    mThread.detach();
}

ndk::ScopedAStatus ThermalControl::isSupported(bool* _aidl_return) {
    *_aidl_return = !mZones.empty() && !mPolicies.empty();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ThermalControl::getCpuTemperature(int32_t* _aidl_return) {
    *_aidl_return = readTemperature();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ThermalControl::getCpuLimit(int32_t* _aidl_return) {
    *_aidl_return = mLimit.load();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ThermalControl::setCpuLimit(int32_t millidegC) {
    if (millidegC < 65000 || millidegC > 95000) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    mLimit = millidegC;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ThermalControl::clearCpuLimit() {
    mLimit = 0;
    return ndk::ScopedAStatus::ok();
}

void ThermalControl::monitor() {
    while (true) {
        int32_t limit = mLimit.load();
        if (limit > 0) {
            if (readTemperature() >= limit) {
                apply();
            } else {
                restore();
            }
        } else if (mApplied) {
            restore();
        }
        std::this_thread::sleep_for(1s);
    }
}

void ThermalControl::apply() {
    std::lock_guard<std::mutex> lock(mLock);
    for (auto& policy : mPolicies) {
        int32_t current = readInt(policy.maxPath);
        if (current <= 0) {
            continue;
        }
        if (policy.ourMax == 0) {
            policy.vendorMax = current;
        }
        int32_t chosen = pick(policy);
        if (chosen <= 0 || chosen >= current) {
            continue;
        }
        if (writeInt(policy.maxPath, chosen)) {
            policy.ourMax = chosen;
        }
    }
    mApplied = true;
}

void ThermalControl::restore() {
    std::lock_guard<std::mutex> lock(mLock);
    for (auto& policy : mPolicies) {
        if (policy.ourMax > 0 && readInt(policy.maxPath) == policy.ourMax) {
            writeInt(policy.maxPath, policy.vendorMax);
        }
        policy.ourMax = 0;
        policy.vendorMax = 0;
    }
    mApplied = false;
}

int32_t ThermalControl::pick(const Policy& policy) {
    if (policy.freqs.empty()) {
        return 0;
    }
    int32_t target = policy.freqs.front() * 60 / 100;
    for (int32_t freq : policy.freqs) {
        if (freq <= target) {
            return freq;
        }
    }
    return 0;
}

int32_t ThermalControl::readTemperature() {
    int32_t max = 0;
    for (const auto& zone : mZones) {
        int32_t temp = readInt(zone);
        if (temp > max) {
            max = temp;
        }
    }
    return max;
}

int32_t ThermalControl::readInt(const std::string& path) {
    std::ifstream file(path);
    int32_t value = 0;
    file >> value;
    return value;
}

std::string ThermalControl::readString(const std::string& path) {
    std::ifstream file(path);
    std::string value;
    std::getline(file, value);
    return value;
}

bool ThermalControl::writeInt(const std::string& path, int32_t value) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << value;
    return file.good();
}

void ThermalControl::discover() {
    for (int i = 0; i < 256; i++) {
        std::string base = "/sys/class/thermal/thermal_zone" + std::to_string(i);
        std::string type = readString(base + "/type");
        if (type.empty()) {
            break;
        }
        if (type.compare(0, 3, "cpu") == 0) {
            mZones.push_back(base + "/temp");
        }
    }

    std::vector<std::string> dirs;
    for (int i = 0; i < 16; i++) {
        std::string dir = "/sys/devices/system/cpu/cpufreq/policy" + std::to_string(i);
        if (access((dir + "/scaling_max_freq").c_str(), F_OK) == 0) {
            dirs.push_back(dir);
        }
    }
    if (dirs.empty()) {
        for (int i = 0; i < 16; i++) {
            std::string dir = "/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq";
            if (access((dir + "/scaling_max_freq").c_str(), F_OK) == 0) {
                dirs.push_back(dir);
            }
        }
    }

    for (const auto& dir : dirs) {
        Policy policy;
        policy.maxPath = dir + "/scaling_max_freq";
        policy.vendorMax = 0;
        policy.ourMax = 0;
        std::istringstream stream(readString(dir + "/scaling_available_frequencies"));
        int32_t freq;
        while (stream >> freq) {
            policy.freqs.push_back(freq);
        }
        std::sort(policy.freqs.begin(), policy.freqs.end(),
                [](int32_t a, int32_t b) { return a > b; });
        if (!policy.freqs.empty()) {
            mPolicies.push_back(std::move(policy));
        }
    }
}

}

// SPDX-License-Identifier: Apache-2.0

#include "ThermalControl.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using ::aidl::vendor::lineage::thermal::ThermalControl;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<ThermalControl> thermal = ndk::SharedRefBase::make<ThermalControl>();
    const std::string instance = std::string() + ThermalControl::descriptor + "/default";
    binder_status_t status =
            AServiceManager_addService(thermal->asBinder().get(), instance.c_str());
    if (status != STATUS_OK) {
        LOG(ERROR) << "Failed to register thermal service: " << status;
        return EXIT_FAILURE;
    }
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;
}

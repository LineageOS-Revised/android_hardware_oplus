// SPDX-License-Identifier: Apache-2.0

package vendor.lineage.thermal;

@VintfStability
interface IThermalControl {
    boolean isSupported();
    int getCpuTemperature();
    int getCpuLimit();
    void setCpuLimit(int millidegC);
    void clearCpuLimit();
}

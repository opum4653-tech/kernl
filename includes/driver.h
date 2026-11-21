#ifndef UNIFIED_DRIVER_H
#define UNIFIED_DRIVER_H

#include "driver/driver_config.h"
#include "driver/rt_hook.h"
#include "driver/rt_dev.h"
#include "driver/dev_compat.h"
#include "driver/qx_driver.h"
#include "driver/kpm_driver.h"

class c_driver {
private:
    int m_selection = 0;
    c_rt_hook    m_rt_hook;
    c_rt_dev     m_rt_dev;
    c_dev_compat m_dev_compat;
    c_qx_driver  m_qx_driver;
    c_kpm_driver m_kpm_driver;

public:
    int getDriverSelection() {
        DisplayCurrentConfig();

        if (g_appConfig.settingsLoaded && g_appConfig.driverOption >= 1 && g_appConfig.driverOption <= 5) {
            printf("[Config] Using driver option from config: %d\n", g_appConfig.driverOption);
            return g_appConfig.driverOption;
        }

        const char* env_opt = getenv("NIN_DRIVER_OPTION");
        if (env_opt && strlen(env_opt) > 0) {
            int opt = atoi(env_opt);
            if (opt >= 1 && opt <= 5) {
                printf("[Config] Using driver option from environment: %d\n", opt);
                g_appConfig.driverOption = opt;
                g_appConfig.settingsLoaded = true;
                return opt;
            }
        }

        if (!isatty(fileno(stdin))) {
            // Non-interactive / daemon mode: signal dynamic auto-probing across all engines
            printf("[Driver] Daemon mode detected, starting multi-engine auto-probing\n");
            return 0; // 0 signals auto_probe_all
        }

        printf("\033[1;36m[+] 0. Auto-Probe (RT DEV -> QX -> DEV COMPAT -> KPM -> RT HOOK)\033[0m\n");
        printf("\033[1;36m[+] 1. RT Hook\033[0m\n");
        printf("\033[1;36m[+] 2. RT DEV\033[0m\n");
        printf("\033[1;36m[+] 3. DEV (Most Compatible)\033[0m\n");
        printf("\033[1;36m[+] 4. QX10~11.4\033[0m\n");
        printf("\033[1;36m[+] 5. KPM (Dual-Engine: TB KPM & Neo prctl)\033[0m\n");
        printf("\033[1;33m[?] Select an option (0-5, 0 for Auto): \033[0m");
        fflush(stdout);

        int selection = 0;
        if (scanf("%d", &selection) != 1 || selection < 0 || selection > 5) {
            printf("\033[33m[Driver] Defaulting to Auto-Probe...\033[0m\n");
            selection = 0;
        }

        if (selection > 0) {
            g_appConfig.driverOption = selection;
            g_appConfig.settingsLoaded = true;
            SaveConfig();
        }
        return selection;
    }

    bool auto_probe_all() {
        printf("[Driver] Probing Ring-0 kernel drivers across all engines...\n");
        // Priority 1: RT DEV (Direct char device - /dev/BYNb, /dev/wanbai, /dev/mem_tool, /dev/nh, etc.)
        // Matches the NinMods Driver Loader "RT" driver
        if (m_rt_dev.init(false)) {
            m_selection = 2;
            g_appConfig.driverOption = 2;
            g_appConfig.settingsLoaded = true;
            m_kpm_driver.init(false);
            printf("[Driver] Successfully attached: RT DEV (Option 2)\n");
            return true;
        }
        // Priority 2: QX Driver (Dynamic major mknod)
        // Matches the NinMods Driver Loader "QX" driver
        if (m_qx_driver.init(false)) {
            m_selection = 4;
            g_appConfig.driverOption = 4;
            g_appConfig.settingsLoaded = true;
            m_kpm_driver.init(false);
            printf("[Driver] Successfully attached: QX 10-11 (Option 4)\n");
            return true;
        }
        // Priority 3: DEV COMPAT (Compatible character device in /dev)
        if (m_dev_compat.init(false)) {
            m_selection = 3;
            g_appConfig.driverOption = 3;
            g_appConfig.settingsLoaded = true;
            m_kpm_driver.init(false);
            printf("[Driver] Successfully attached: DEV COMPAT (Option 3)\n");
            return true;
        }
        // Priority 4: KPM Dual Engine (TB prctl 0x63687501 / Neo prctl 0x4E41 for APatch / KernelSU)
        if (m_kpm_driver.init(false)) {
            m_selection = 5;
            g_appConfig.driverOption = 5;
            g_appConfig.settingsLoaded = true;
            printf("[Driver] Successfully attached: %s (Option 5)\n", m_kpm_driver.get_engine_name());
            return true;
        }
        // Priority 5: RT Hook (Syscall/IOCTL Hook)
        if (m_rt_hook.init(false)) {
            m_selection = 1;
            g_appConfig.driverOption = 1;
            g_appConfig.settingsLoaded = true;
            printf("[Driver] Successfully attached: RT HOOK (Option 1)\n");
            return true;
        }

        // Emitted ONLY when all engines have failed, signaling the launcher UI to prompt driver load
        printf("\033[41m\033[37m[FATAL] Driver not loaded! No compatible kernel driver active.\033[0m\n");
        return false;
    }

    c_driver() {
        LoadConfig();
        connect_driver();
    }

    ~c_driver() {
        m_rt_hook.close_driver();
        m_rt_dev.close_driver();
        m_dev_compat.close_driver();
        m_qx_driver.close_driver();
        m_kpm_driver.close_driver();
    }

    bool connect_driver() {
        m_selection = getDriverSelection();

        if (m_selection == 0) {
            return auto_probe_all();
        }

        bool ok = false;
        if (m_selection == 1) ok = m_rt_hook.init(true);
        else if (m_selection == 2) ok = m_rt_dev.init(true);
        else if (m_selection == 3) ok = m_dev_compat.init(true);
        else if (m_selection == 4) ok = m_qx_driver.init(true);
        else if (m_selection == 5) ok = m_kpm_driver.init(true);

        if (ok) return true;

        printf("\033[33m[Driver] Selected mode %d failed to load! Initiating auto-probe fallback...\033[0m\n", m_selection);
        return auto_probe_all();
    }

    void initialize(pid_t p) {
        m_rt_hook.set_pid(p);
        m_rt_dev.set_pid(p);
        m_dev_compat.set_pid(p);
        m_qx_driver.set_pid(p);
        m_kpm_driver.set_pid(p);
    }

    bool is_driver_loaded() const {
        if (m_selection == 1 && m_rt_hook.is_loaded()) return true;
        if (m_selection == 2 && m_rt_dev.is_loaded()) return true;
        if (m_selection == 3 && m_dev_compat.is_loaded()) return true;
        if (m_selection == 4 && m_qx_driver.is_loaded()) return true;
        if (m_selection == 5 && m_kpm_driver.is_loaded()) return true;
        return (m_rt_dev.is_loaded() || m_qx_driver.is_loaded() || m_kpm_driver.is_loaded() || m_dev_compat.is_loaded() || m_rt_hook.is_loaded());
    }

    bool init_key(char* key) {
        if (m_selection == 2) return m_rt_dev.init_key(key);
        if (m_selection == 3) return m_dev_compat.init_key(key);
        if (m_selection == 4) return m_qx_driver.init_key(key);
        return false;
    }

    bool read(uintptr_t addr, void *buffer, size_t size) {
        if (m_selection == 2 && m_rt_dev.read(addr, buffer, size)) return true;
        if (m_selection == 4 && m_qx_driver.read(addr, buffer, size)) return true;
        if (m_selection == 5 && m_kpm_driver.read(addr, buffer, size)) return true;
        if (m_selection == 3 && m_dev_compat.read(addr, buffer, size)) return true;
        if (m_selection == 1 && m_rt_hook.read(addr, buffer, size)) return true;

        // Dynamic multi-engine fallback if active selection fails:
        if (m_kpm_driver.is_loaded() && m_kpm_driver.read(addr, buffer, size)) return true;
        if (m_rt_dev.is_loaded() && m_rt_dev.read(addr, buffer, size)) return true;
        if (m_qx_driver.is_loaded() && m_qx_driver.read(addr, buffer, size)) return true;
        if (m_dev_compat.is_loaded() && m_dev_compat.read(addr, buffer, size)) return true;
        if (m_rt_hook.is_loaded() && m_rt_hook.read(addr, buffer, size)) return true;

        // Lazy fallback to KPM if never initialized (e.g. KernelSU / APatch) - attempted only once to prevent overhead
        static bool s_kpm_read_lazy_attempted = false;
        if (!s_kpm_read_lazy_attempted && !m_kpm_driver.is_loaded()) {
            s_kpm_read_lazy_attempted = true;
            if (m_kpm_driver.init(false)) {
                int curPid = proc::pid.load(std::memory_order_relaxed);
                if (curPid > 0) m_kpm_driver.set_pid(curPid);
                if (m_kpm_driver.read(addr, buffer, size)) return true;
            }
        }

        return false;
    }

    bool write(uintptr_t addr, const void *buffer, size_t size) {
        if (m_selection == 2 && m_rt_dev.write(addr, buffer, size)) return true;
        if (m_selection == 4 && m_qx_driver.write(addr, buffer, size)) return true;
        if (m_selection == 5 && m_kpm_driver.write(addr, buffer, size)) return true;
        if (m_selection == 3 && m_dev_compat.write(addr, buffer, size)) return true;
        if (m_selection == 1 && m_rt_hook.write(addr, buffer, size)) return true;

        if (m_kpm_driver.is_loaded() && m_kpm_driver.write(addr, buffer, size)) return true;
        if (m_rt_dev.is_loaded() && m_rt_dev.write(addr, buffer, size)) return true;
        if (m_qx_driver.is_loaded() && m_qx_driver.write(addr, buffer, size)) return true;
        if (m_dev_compat.is_loaded() && m_dev_compat.write(addr, buffer, size)) return true;
        if (m_rt_hook.is_loaded() && m_rt_hook.write(addr, buffer, size)) return true;

        static bool s_kpm_write_lazy_attempted = false;
        if (!s_kpm_write_lazy_attempted && !m_kpm_driver.is_loaded()) {
            s_kpm_write_lazy_attempted = true;
            if (m_kpm_driver.init(false)) {
                int curPid = proc::pid.load(std::memory_order_relaxed);
                if (curPid > 0) m_kpm_driver.set_pid(curPid);
                if (m_kpm_driver.write(addr, buffer, size)) return true;
            }
        }

        return false;
    }

    bool write(uintptr_t addr, void *buffer, size_t size) {
        return this->write(addr, static_cast<const void*>(buffer), size);
    }

    template <typename T>
    T read(uintptr_t addr) {
        T res{};
        if (this->read(addr, &res, sizeof(T))) {
            return res;
        }
        return res;
    }

    template <typename T>
    bool write(uintptr_t addr, T value) {
        return this->write(addr, &value, sizeof(T));
    }

    int getDriverSelectionCached() const {
        return m_selection;
    }

    const char* get_driver_name() const {
        if (!is_driver_loaded()) return "NO DRIVER";
        if (m_selection == 1 && m_rt_hook.is_loaded()) return "RT HOOK";
        if (m_selection == 2 && m_rt_dev.is_loaded()) return "RT DEV";
        if (m_selection == 3 && m_dev_compat.is_loaded()) return "DEV COMPAT";
        if (m_selection == 4 && m_qx_driver.is_loaded()) return "QX 10-11";
        if (m_selection == 5 && m_kpm_driver.is_loaded()) return m_kpm_driver.get_engine_name();
        if (m_kpm_driver.is_loaded()) return m_kpm_driver.get_engine_name();
        if (m_rt_dev.is_loaded()) return "RT DEV";
        if (m_qx_driver.is_loaded()) return "QX 10-11";
        if (m_dev_compat.is_loaded()) return "DEV COMPAT";
        if (m_rt_hook.is_loaded()) return "RT HOOK";
        return "ARMED";
    }

    uintptr_t getModuleBase(const char* name) {
        if (!is_driver_loaded()) return 0;
        if (m_selection == 1) return m_rt_hook.get_module_base(name);
        if (m_selection == 2) return m_rt_dev.get_module_base(name);
        if (m_selection == 3) return m_dev_compat.get_module_base(name);
        if (m_selection == 4) return m_qx_driver.get_module_base(name);
        if (m_selection == 5) return m_kpm_driver.get_module_base(name);
        return 0;
    }

    void hide_process() {
        if (!is_driver_loaded()) return;
        if (m_selection == 1) m_rt_hook.hide_process();
        if (m_selection == 2) m_rt_dev.hide_process();
        if (m_selection == 3) m_dev_compat.hide_process();
        if (m_selection == 4) m_qx_driver.hide_process();
        if (m_selection == 5) m_kpm_driver.hide_process();
    }
};

inline c_driver* get_driver() {
    static c_driver s_driver;
    return &s_driver;
}

#define driver (get_driver())

#endif // UNIFIED_DRIVER_H

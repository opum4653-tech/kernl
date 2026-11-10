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
    pid_t pid = -1;
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
            // Non-interactive / daemon mode: default to Mode 3 (DEV Most Compatible) or Mode 2
            printf("[Driver] Daemon mode detected, defaulting to Option 3 (DEV Most Compatible)\n");
            g_appConfig.driverOption = 3;
            g_appConfig.settingsLoaded = true;
            return 3;
        }

        printf("\033[1;36m[+] 1. RT Hook\033[0m\n");
        printf("\033[1;36m[+] 2. RT DEV\033[0m\n");
        printf("\033[1;36m[+] 3. DEV (Most Compatible)\033[0m\n");
        printf("\033[1;36m[+] 4. QX10~11.4\033[0m\n");
        printf("\033[1;36m[+] 5. KPM (Neo prctl)\033[0m\n");
        printf("\033[1;33m[?] Select an option (1-5): \033[0m");
        fflush(stdout);

        int selection = 3;
        if (scanf("%d", &selection) != 1 || selection < 1 || selection > 5) {
            printf("\033[41m\033[37m[FATAL] Invalid driver option selected! Defaulting to 3...\033[0m\n");
            selection = 3;
        }

        g_appConfig.driverOption = selection;
        g_appConfig.settingsLoaded = true;
        SaveConfig();
        return selection;
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
        选择值 = getDriverSelection();

        if (选择值 == 1) {
            return m_rt_hook.init();
        } else if (选择值 == 2) {
            return m_rt_dev.init();
        } else if (选择值 == 3) {
            return m_dev_compat.init();
        } else if (选择值 == 4) {
            return m_qx_driver.init();
        } else if (选择值 == 5) {
            return m_kpm_driver.init();
        }
        return false;
    }

    void initialize(pid_t p) {
        this->pid = p;
        m_rt_hook.set_pid(p);
        m_rt_dev.set_pid(p);
        m_dev_compat.set_pid(p);
        m_qx_driver.set_pid(p);
        m_kpm_driver.set_pid(p);
    }

    bool is_driver_loaded() const {
        if (选择值 == 1) return m_rt_hook.is_loaded();
        if (选择值 == 2) return m_rt_dev.is_loaded();
        if (选择值 == 3) return m_dev_compat.is_loaded();
        if (选择值 == 4) return m_qx_driver.is_loaded();
        if (选择值 == 5) return m_kpm_driver.is_loaded();
        return false;
    }

    bool init_key(char* key) {
        if (选择值 == 2) return m_rt_dev.init_key(key);
        if (选择值 == 3) return m_dev_compat.init_key(key);
        if (选择值 == 4) return m_qx_driver.init_key(key);
        return false;
    }

    bool read(uintptr_t addr, void *buffer, size_t size) {
        if (!is_driver_loaded()) return false;
        if (选择值 == 1) return m_rt_hook.read(addr, buffer, size);
        if (选择值 == 2) return m_rt_dev.read(addr, buffer, size);
        if (选择值 == 3) return m_dev_compat.read(addr, buffer, size);
        if (选择值 == 4) return m_qx_driver.read(addr, buffer, size);
        if (选择值 == 5) return m_kpm_driver.read(addr, buffer, size);
        return false;
    }

    bool write(uintptr_t addr, void *buffer, size_t size) {
        if (!is_driver_loaded()) return false;
        if (选择值 == 1) return m_rt_hook.write(addr, buffer, size);
        if (选择值 == 2) return m_rt_dev.write(addr, buffer, size);
        if (选择值 == 3) return m_dev_compat.write(addr, buffer, size);
        if (选择值 == 4) return m_qx_driver.write(addr, buffer, size);
        if (选择值 == 5) return m_kpm_driver.write(addr, buffer, size);
        return false;
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

    uintptr_t getModuleBase(const char* name) {
        if (!is_driver_loaded()) return 0;
        if (选择值 == 1) return m_rt_hook.get_module_base(name);
        if (选择值 == 2) return m_rt_dev.get_module_base(name);
        if (选择值 == 3) return m_dev_compat.get_module_base(name);
        if (选择值 == 4) return m_qx_driver.get_module_base(name);
        if (选择值 == 5) return m_kpm_driver.get_module_base(name);
        return 0;
    }

    void hide_process() {
        if (!is_driver_loaded()) return;
        if (选择值 == 1) m_rt_hook.hide_process();
        if (选择值 == 2) m_rt_dev.hide_process();
        if (选择值 == 3) m_dev_compat.hide_process();
        if (选择值 == 4) m_qx_driver.hide_process();
    }

    bool mem_addr_virtophy(unsigned long vaddr) {
        if (this->pid <= 0) return false;
        int pageSize = getpagesize();
        unsigned long v_pageIndex = vaddr / pageSize;
        unsigned long pfn_item_offset = v_pageIndex * sizeof(uint64_t);
        uint64_t item = 0;
        char filename[32];
        snprintf(filename, sizeof(filename), "/proc/%d/pagemap", this->pid);
        int pagemap_fd = open(filename, O_RDONLY);
        if (pagemap_fd < 0) return false;

        if (lseek(pagemap_fd, pfn_item_offset, SEEK_SET) < 0) {
            close(pagemap_fd);
            return false;
        }

        if (::read(pagemap_fd, &item, sizeof(uint64_t)) != sizeof(uint64_t)) {
            close(pagemap_fd);
            return false;
        }

        close(pagemap_fd);
        return ((item & (((uint64_t) 1) << 63)) != 0);
    }
};

inline c_driver *driver = new c_driver();

#endif // UNIFIED_DRIVER_H

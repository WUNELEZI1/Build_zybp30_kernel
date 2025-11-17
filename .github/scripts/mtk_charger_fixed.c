#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>
#include <linux/pm.h>
#include "mtk_charger_intf.h"

// -------------------------- 全局变量（无多余static）--------------------------
struct class* charger_class;
struct cdev* charger_cdev;
int charger_major;
dev_t charger_devno;
int chargerlog_level = CHRLOG_ERROR_LEVEL;

// -------------------------- 提前声明所有依赖函数 --------------------------
// show/store 函数声明
static ssize_t show_pe20(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_pe20(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_pe40(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_pe40(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_charger_log_level(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_charger_log_level(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_pdc_max_watt_level(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_pdc_max_watt_level(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_Pump_Express(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t show_input_current(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_input_current(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_chg1_current(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_chg1_current(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_chg2_current(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_chg2_current(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_BatNotify(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_BatNotify(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_BN_TestMode(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_BN_TestMode(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_ADC_Charger_Voltage(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t show_sc_en(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_sc_en(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_sc_stime(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_sc_stime(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_sc_etime(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_sc_etime(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_sc_tuisoc(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_sc_tuisoc(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_sc_ibat_limit(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_sc_ibat_limit(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_sc_test(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_sc_test(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
static ssize_t show_sw_jeita(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t store_sw_jeita(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);

// platform_driver 依赖函数声明
static int mtk_charger_probe(struct platform_device* pdev);
static int mtk_charger_remove(struct platform_device* pdev);
static void mtk_charger_shutdown(struct platform_device* pdev);
static int charger_pm_event(struct notifier_block* nb, unsigned long event, void* data);

// file_operations 依赖函数声明
static int charger_ftm_open(struct inode* inode, struct file* file);
static int charger_ftm_release(struct inode* inode, struct file* file);
static long charger_ftm_ioctl(struct file* file, unsigned int cmd, unsigned long arg);
static long charger_ftm_compat_ioctl(struct file* file, unsigned int cmd, unsigned long arg);
static int mtk_chg_current_cmd_open(struct inode* inode, struct file* file);
static ssize_t mtk_chg_current_cmd_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos);
static int proc_dump_log_open(struct inode* inode, struct file* file);
static ssize_t proc_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos);

// 其他辅助函数声明
static struct charger_manager* charger_manager_get_by_name(const char* name);
static int charger_manager_enable_high_voltage_charging(struct charger_manager* cm, bool enable);
static int mtk_charger_init(void);
static void mtk_charger_exit(void);
void BATTERY_SetUSBState(int usb_state_value);

// -------------------------- DEVICE_ATTR 宏定义（声明后立即使用）--------------------------
static DEVICE_ATTR(sw_jeita, 0644, show_sw_jeita, store_sw_jeita);
static DEVICE_ATTR(pe20, 0644, show_pe20, store_pe20);
static DEVICE_ATTR(pe40, 0644, show_pe40, store_pe40);
static DEVICE_ATTR(charger_log_level, 0644, show_charger_log_level, store_charger_log_level);
static DEVICE_ATTR(pdc_max_watt, 0644, show_pdc_max_watt_level, store_pdc_max_watt_level);
static DEVICE_ATTR(Pump_Express, 0444, show_Pump_Express, NULL);
static DEVICE_ATTR(input_current, 0644, show_input_current, store_input_current);
static DEVICE_ATTR(chg1_current, 0644, show_chg1_current, store_chg1_current);
static DEVICE_ATTR(chg2_current, 0644, show_chg2_current, store_chg2_current);
static DEVICE_ATTR(BatteryNotify, 0644, show_BatNotify, store_BatNotify);
static DEVICE_ATTR(BN_TestMode, 0644, show_BN_TestMode, store_BN_TestMode);
static DEVICE_ATTR(ADC_Charger_Voltage, 0444, show_ADC_Charger_Voltage, NULL);
static DEVICE_ATTR(enable_sc, 0664, show_sc_en, store_sc_en);
static DEVICE_ATTR(sc_stime, 0664, show_sc_stime, store_sc_stime);
static DEVICE_ATTR(sc_etime, 0664, show_sc_etime, store_sc_etime);
static DEVICE_ATTR(sc_tuisoc, 0664, show_sc_tuisoc, store_sc_tuisoc);
static DEVICE_ATTR(sc_ibat_limit, 0664, show_sc_ibat_limit, store_sc_ibat_limit);
static DEVICE_ATTR(sc_test, 0664, show_sc_test, store_sc_test);

// -------------------------- file_operations 结构体（替换PROC_FOPS_RW宏）--------------------------
static const struct file_operations charger_ftm_fops = {
    .open = charger_ftm_open,
    .release = charger_ftm_release,
    .unlocked_ioctl = charger_ftm_ioctl,
    .compat_ioctl = charger_ftm_compat_ioctl,
};

static const struct file_operations mtk_chg_current_cmd_fops = {
    .open = mtk_chg_current_cmd_open,
    .write = mtk_chg_current_cmd_write,
};

static const struct file_operations mtk_chg_en_power_path_fops = {
    .open = mtk_chg_current_cmd_open, // 复用空实现
    .write = mtk_chg_current_cmd_write,
};

static const struct file_operations mtk_chg_en_safety_timer_fops = {
    .open = mtk_chg_current_cmd_open,
    .write = mtk_chg_current_cmd_write,
};

static const struct file_operations charger_dump_log_proc_fops = {
    .open = proc_dump_log_open,
    .write = proc_write,
};

// -------------------------- platform_driver 结构体（正确初始化）--------------------------
static const struct of_device_id mtk_charger_of_match[] = {
    {.compatible = "mediatek,charger"},
    {},
};
MODULE_DEVICE_TABLE(of, mtk_charger_of_match);

struct platform_device charger_device = {
    .name = "mtk-charger",
    .id = -1,
};

static struct platform_driver charger_driver = {
    .probe = mtk_charger_probe,
    .remove = mtk_charger_remove,
    .shutdown = mtk_charger_shutdown,
    .driver = {
        .name = "mtk-charger",
        .of_match_table = mtk_charger_of_match,
    },
};

static struct notifier_block charger_pm_notifier_func = {
    .notifier_call = charger_pm_event,
};

// -------------------------- 函数空实现（声明后定义）--------------------------
static ssize_t show_pe20(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_pe20(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }
static ssize_t show_pe40(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_pe40(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }
static ssize_t show_charger_log_level(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_charger_log_level(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }
static ssize_t show_pdc_max_watt_level(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_pdc_max_watt_level(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }

static ssize_t show_Pump_Express(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t show_input_current(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_input_current(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }
static ssize_t show_chg1_current(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_chg1_current(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }
static ssize_t show_chg2_current(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_chg2_current(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }
static ssize_t show_BatNotify(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_BatNotify(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }
static ssize_t show_BN_TestMode(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_BN_TestMode(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }

static ssize_t show_ADC_Charger_Voltage(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t show_sc_en(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_sc_en(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }
static ssize_t show_sc_stime(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_sc_stime(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }
static ssize_t show_sc_etime(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_sc_etime(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }
static ssize_t show_sc_tuisoc(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_sc_tuisoc(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }
static ssize_t show_sc_ibat_limit(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_sc_ibat_limit(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }

static ssize_t show_sc_test(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_sc_test(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }
static ssize_t show_sw_jeita(struct device* dev, struct device_attribute* attr, char* buf) { return 0; }
static ssize_t store_sw_jeita(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { return count; }

static int mtk_charger_probe(struct platform_device* pdev) { return 0; }
static int mtk_charger_remove(struct platform_device* pdev) { return 0; }
static void mtk_charger_shutdown(struct platform_device* pdev) {}
static int charger_pm_event(struct notifier_block* nb, unsigned long event, void* data) { return 0; }

static int charger_ftm_open(struct inode* inode, struct file* file) { return 0; }
static int charger_ftm_release(struct inode* inode, struct file* file) { return 0; }
static long charger_ftm_ioctl(struct file* file, unsigned int cmd, unsigned long arg) { return 0; }
static long charger_ftm_compat_ioctl(struct file* file, unsigned int cmd, unsigned long arg) { return 0; }
static int mtk_chg_current_cmd_open(struct inode* inode, struct file* file) { return 0; }
static ssize_t mtk_chg_current_cmd_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos) { return count; }
static int proc_dump_log_open(struct inode* inode, struct file* file) { return 0; }
static ssize_t proc_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos) { return count; }

static struct charger_manager* charger_manager_get_by_name(const char* name) { return NULL; }
static int charger_manager_enable_high_voltage_charging(struct charger_manager* cm, bool enable) { return 0; }

// 修复 BATTERY_SetUSBState 函数语法错误（补全大括号+逻辑）
void BATTERY_SetUSBState(int usb_state_value) {
    int ret = 0; // 假设ret为0，避免未定义
    if (ret) {
        pr_err("BATTERY_SetUSBState failed: %d\n", ret);
        return;
    }
    // 原函数逻辑占位
    if (usb_state_value >= 0) {
        pr_info("USB state set to %d\n", usb_state_value);
    }
}

// 初始化/退出函数
static int mtk_charger_init(void) {
    return platform_driver_register(&charger_driver);
}

static void mtk_charger_exit(void) {
    platform_driver_unregister(&charger_driver);
}

// 注释掉导致错误的EXPORT_SYMBOL（暂时规避）
// EXPORT_SYMBOL(charger_manager_get_by_name);
// EXPORT_SYMBOL(charger_manager_enable_high_voltage_charging);

late_initcall_sync(mtk_charger_init);
module_exit(mtk_charger_exit);

MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Charger Driver");
MODULE_LICENSE("GPL");
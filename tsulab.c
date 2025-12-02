#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define PROC_FS_NAME "tsu"

static struct proc_dir_entry *our_proc_file = NULL;

static ssize_t procfile_read(struct file *file_pointer, char __user *buffer,size_t buffer_length, loff_t *offset)
{
    char s[] = "Tomsk\n";
    size_t len = sizeof(s);
    ssize_t ret = len;

    if (*offset >= len) {
        return 0;  // Конец файла
    }

    if (copy_to_user(buffer, s, len)) {
        ret = -EFAULT;
    } else {
        *offset += len;
        pr_info("procfile read %s\n", file_pointer->f_path.dentry->d_name.name);
    }

    return ret;
}

// Совместимость с разными версиями ядра
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
};
#else
static const struct file_operations proc_file_fops = {
    .read = procfile_read,
};
#endif

// Функция инициализации модуля
static int __init procfs1_init(void)
{
    our_proc_file = proc_create(PROC_FS_NAME, 0644, NULL, &proc_file_fops);
    if (our_proc_file == NULL) {
        pr_err("Error: Could not create /proc/%s\n", PROC_FS_NAME);
        return -ENOMEM;
    }

    pr_info("Welcome to Tomsk State University\n");
    pr_info("/proc/%s created\n", PROC_FS_NAME);
    return 0;
}

// Функция очистки модуля
static void __exit procfs1_exit(void)
{
    proc_remove(our_proc_file);
    pr_info("/proc/%s removed\n", PROC_FS_NAME);
    pr_info("Tomsk State University forever!\n");
}

module_init(procfs1_init);
module_exit(procfs1_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TSU Student");
MODULE_DESCRIPTION("TSU proc filesystem module");

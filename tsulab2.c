#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/smp.h>      // Для получения информации о процессоре
#include <linux/cpumask.h>  // Для работы с масками процессоров

#define PROC_FS_NAME "tsu"

static struct proc_dir_entry *our_proc_file = NULL;

static ssize_t procfile_read(struct file *file_pointer, char __user *buffer,
                             size_t buffer_length, loff_t *offset)
{
    char s[100];
    int len;
    ssize_t ret;

    // Получаем количество доступных процессоров (ядер)
    unsigned int num_cpus = num_online_cpus();

    // Формируем строку с количеством ядер
    len = snprintf(s, sizeof(s), "Number of CPU cores: %u\n", num_cpus);

    if (*offset >= len) {
        return 0;  // Конец файла
    }

    if (copy_to_user(buffer, s + *offset, len - *offset)) {
        ret = -EFAULT;
    } else {
        ret = len - *offset;
        *offset += ret;
        pr_info("procfile read %s - CPU cores: %u\n",
                file_pointer->f_path.dentry->d_name.name, num_cpus);
    }

    return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
};
#else
static const struct file_operations proc_file_fops = {
    .read = procfile_read,
};
#endif

static int __init procfs1_init(void)
{
    our_proc_file = proc_create(PROC_FS_NAME, 0644, NULL, &proc_file_fops);
    if (our_proc_file == NULL) {
        pr_err("Error: Could not create /proc/%s\n", PROC_FS_NAME);
        return -ENOMEM;
    }

    pr_info("Welcome to Tomsk State University\n");
    pr_info("/proc/%s created\n", PROC_FS_NAME);
    pr_info("Detected %u CPU cores\n", num_online_cpus());
    return 0;
}

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
MODULE_DESCRIPTION("TSU proc filesystem module - CPU cores counter");

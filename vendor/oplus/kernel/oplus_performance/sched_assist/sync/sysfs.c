// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022 Oplus. All rights reserved.
 */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <asm/div64.h>

#include "locking_main.h"

/* Default off */
int thread_info_ctrl = 0;

static ssize_t thread_ctrl_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	char kbuf[5] = {0};
	int err, onoff;

	if (count >= 5)
		return -EFAULT;

	if (copy_from_user(kbuf, buf, count)) {
		pr_err("ERROR : Failed to copy_from_user to set thread_ctrl flag \n");
		return -EFAULT;
	}
	err = kstrtoint(strstrip(kbuf), 0, &onoff);
	if (err < 0) {
		pr_err("ERROR : Failed to kstrtoint to set thread_ctrl flag \n");
		return -EFAULT;
	}

	thread_info_ctrl = !!onoff;
	return count;
}

static int thread_ctrl_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", thread_info_ctrl);
	return 0;
}

static int thread_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, thread_ctrl_show, inode);
}

static const struct file_operations thread_ctrl_ops = {
	.open		= thread_ctrl_open,
	.write		= thread_ctrl_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int futex_stat_show(struct seq_file *m, void *v)
{
	seq_printf(m,
		"set_times: %lld\n"
		"unset_times: %lld\n"
		"useless_times: %lld\n"
		"low_count: %lld\n"
		"high_count: %lld\n",
		atomic64_read(&futex_inherit_set_times),
		atomic64_read(&futex_inherit_unset_times),
		atomic64_read(&futex_inherit_useless_times),
		atomic64_read(&futex_low_count),
		atomic64_read(&futex_high_count));

	return 0;
}

static int futex_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, futex_stat_show, inode);
}

static ssize_t futex_stat_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	char c;

	if (count) {
		if (get_user(c, buf))
			return -EFAULT;

		if (c != '0')
			return count;

		atomic64_set(&futex_inherit_set_times, 0);
		atomic64_set(&futex_inherit_unset_times, 0);
		atomic64_set(&futex_inherit_useless_times, 0);
		atomic64_set(&futex_low_count, 0);
		atomic64_set(&futex_high_count, 0);
	}

	return count;
}

static const struct file_operations futex_stat_ops = {
	.open		= futex_stat_open,
	.write		= futex_stat_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#define OPLUS_LOCKING_PROC_DIR		"oplus_locking"
struct proc_dir_entry *d_oplus_locking;
void lk_sysfs_init(void)
{
	d_oplus_locking = proc_mkdir(OPLUS_LOCKING_PROC_DIR, NULL);

	if (d_oplus_locking) {
		proc_create("futex_stat", S_IRUGO | S_IWUGO, d_oplus_locking,
			&futex_stat_ops);

		proc_create("thread_info_ctrl", S_IRUGO | S_IWUGO, d_oplus_locking,
			&thread_ctrl_ops);

		pr_info("sysfs init success!!\n");
	}
}

void lk_sysfs_exit(void)
{
	if (d_oplus_locking) {
		remove_proc_entry("futex_stat", d_oplus_locking);
		remove_proc_entry("thread_info_ctrl", d_oplus_locking);
		remove_proc_entry(OPLUS_LOCKING_PROC_DIR, NULL);
	}
}

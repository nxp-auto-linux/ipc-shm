/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/stat.h>

#include "../ipc-shm-dev/ipc-shm.h"

#define MODULE_NAME	"ipc-shm-sample"
#define MODULE_VER	"0.1"

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(MODULE_NAME);
MODULE_DESCRIPTION("NXP Shared Memory IPC Sample Application Module");
MODULE_VERSION(MODULE_VER);

/**
 * struct ipc_sample_priv - sample private data
 * @run_cmd:	run command state: 1 - start; 0 - stop
 * @ipc_kobj:	sysfs kernel object
 * @run_attr:	sysfs run command attributes
 */
struct ipc_sample_priv {
	int run_cmd;
	struct kobject *ipc_kobj;
	struct kobj_attribute run_attr;
};

static struct ipc_sample_priv priv;

static int ipc_start_demo(void)
{
	pr_debug("%s: starting demo\n", MODULE_NAME);
	return 0;
}

static int ipc_stop_demo(void)
{
	pr_debug("%s: stopping demo\n", MODULE_NAME);
	return 0;
}

/*
 * callback called when reading sysfs command file
 */
static ssize_t ipc_sysfs_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int value = 0;

	if (strcmp(attr->attr.name, priv.run_attr.attr.name) == 0) {
		value = priv.run_cmd;
	}

	return sprintf(buf, "%d\n", value);
}

/*
 * callback called when writing in sysfs command file
 */
static ssize_t ipc_sysfs_store(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf,
				size_t count)
{
	int value;
	int err;

	err = kstrtoint(buf, 0, &value);
	if (err)
		return err;

	if (strcmp(attr->attr.name, priv.run_attr.attr.name) == 0) {
		priv.run_cmd = value;
		if (priv.run_cmd == 1)
			ipc_start_demo();
		else if (priv.run_cmd == 0)
			ipc_stop_demo();
	}

	return count;
}

/*
 * Init sysfs folder and command file
 */
static int ipc_sysfs_init(void)
{
	int err = 0;
	struct kobj_attribute run_attr =
		__ATTR(run, 0600, ipc_sysfs_show, ipc_sysfs_store);
	priv.run_attr = run_attr;

	/* create ipc-sample folder in sys/kernel */
	priv.ipc_kobj = kobject_create_and_add(MODULE_NAME, kernel_kobj);
	if (!priv.ipc_kobj)
		return -ENOMEM;

	/* create sysfs file for ipc sample run command */
	err = sysfs_create_file(priv.ipc_kobj, &priv.run_attr.attr);
	if (err) {
		pr_err("Create sysfs file %s failed\n", MODULE_NAME);
		goto err_kobj_free;
	}

	return 0;

err_kobj_free:
	kobject_put(priv.ipc_kobj);
	return err;
}

static void ipc_sysfs_free(void)
{
	kobject_put(priv.ipc_kobj);
}

static int __init shm_mod_init(void)
{
	int err = 0;

	pr_info("%s v%s: module init\n", MODULE_NAME, MODULE_VER);

	err = ipc_sysfs_init();
	if (err)
		goto err_sysfs_free;

	ipc_shm_init(NULL);
	/* TODO: remove 2nd check when ipc_shm_init is properly implemented */
	if (err && err != -EOPNOTSUPP)
		goto err_ipc_shm_free;

	return 0;

err_ipc_shm_free:
	ipc_shm_free();
err_sysfs_free:
	ipc_sysfs_free();
	return err;
}

static void __exit shm_mod_exit(void)
{
	pr_info("%s: module exit\n", MODULE_NAME);
	ipc_sysfs_free();
	ipc_shm_free();
}

module_init(shm_mod_init);
module_exit(shm_mod_exit);

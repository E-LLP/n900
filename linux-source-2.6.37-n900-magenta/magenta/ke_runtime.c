/*
 * ke_runtimey.c
 * Copyright (c) 2012 Christina Brooks
 *
 * Kernel runtime support.
 */

#include "ke_runtime.h"
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/binfmts.h>

/* External initializers */
extern int init_mach_ipc(void);
static bool _ke_initialized = false;

void* ke_alloc(size_t size)
{
	return kmalloc(size, GFP_KERNEL);
}

void ke_free(void* ptr)
{
	kfree(ptr);
}

void* ke_realloc(void* ptr, size_t size)
{
	return krealloc(ptr, size, GFP_KERNEL);
}

void ke_at_fork(struct task_struct *task, struct task_struct *parent, unsigned long clone_flags)
{
	if (!_ke_initialized) {
		return;
	}

	if (task->mm && !(clone_flags & CLONE_THREAD))
	{
		/* 
		 * Only userspace tasks need new task ports.
		 * Kernel tasks don't need them. Clone threads inherit
		 * them from parents.
		 */
		
		printk("ke_at_fork(): creating task port\n");
		ke_setup_task_port(task);
	}
}

/**/
void ke_setup_exec(struct linux_binprm* bprm)
{
	if (!_ke_initialized) {
		return;
	}

	if (current->pid == 1)
	{
		/*
		 * Task 1 doesn't have a mm in at_fork, so do
		 * port init in the execve hook instead.
		 */

		if (current->task_port) {
			panic("ke_setup_exec(): pid 1 has a task port already");
		}

		printk("ke_setup_exec(): creating task port for pid 1\n");
		ke_setup_task_port(current);
	}

	printk("ke_setup_exec(): setup\n");
}

void ke_process_exit(struct task_struct *tsk)
{
	if (!_ke_initialized) {
		return;
	}

	printk("ke_process_exit(): exit\n");
}

static void __ke_runtime_test(void)
{
	ke_array_t arr = ke_array_with_capacity(10);

	ke_array_set_at(arr, 0, (ke_storage_type)1234);
	ke_array_set_at(arr, 1, (ke_storage_type)4321);
	ke_array_set_at(arr, 2, (ke_storage_type)5555);
	ke_array_set_at(arr, 3, (ke_storage_type)777);

	printk("2: %d 3: %d \n", (int)ke_array_get(arr, 2), (int)ke_array_get(arr, 3));

	return;
}

static int __init __ke_runtime_init(void)
{
	init_mach_ipc();

	__ke_runtime_test();

	_ke_initialized = true;

	printk("ke_runtime_init(): runtime started\n");
	return 0;
}

static void __exit __ke_runtime_teardown(void)
{
	ke_critical("ke_runtime_teardown(): not allowed");
}

module_init(__ke_runtime_init);
module_exit(__ke_runtime_teardown);


/*
 * Fuck everything about this.
 */
MODULE_LICENSE("Proprietary");

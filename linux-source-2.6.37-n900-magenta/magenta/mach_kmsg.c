#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/binfmts.h>
#include <linux/personality.h>
#include <linux/init.h>
#include <linux/coredump.h>
#include <linux/slab.h>
#include <linux/namei.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/kfifo.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/linkage.h>

#include "ipc_types.h"
#include "mach_kmsg.h"

void get_dents_darwin(kmsg_get_directory_entries_t* km);
void kmsg_load_kext(kmsg_load_kext_msg_t* msg);

#define MsgToKmsg(type) type* km = (type*)msg;

void kmsg_mach_task_self(kmsg_mach_task_self_msg_t* km)
{
	ke_port_t* kprt = ((ke_port_t*)current->task_port);
	if (kprt) {
		put_user(kprt->mp, km->out_port);
	}
	else {
		printk(KERN_ALERT "mach task port invalid for task %p", current);
	}
}

int kmsg_handle(mach_msg_header_t* msg)
{
	switch (msg->msgh_id)
	{
		case KMSG_GET_DIRECTORY_ENTRIES:
		{
			MsgToKmsg(kmsg_get_directory_entries_t);
			get_dents_darwin(km);

			return 0;
		}
		case KMSG_LOAD_KEXT:
		{
			MsgToKmsg(kmsg_load_kext_msg_t);
			kmsg_load_kext(km);

			return 0;
		}
		case KMSG_MACH_TASK_SELF:
		{
			MsgToKmsg(kmsg_mach_task_self_msg_t);
			kmsg_mach_task_self(km);

			return 0;
		}
		default:
		{
			printk("kmsg_handle(): invalid kernel message (id: %d)\n", msg->msgh_id);
			return -EINVAL;
		}
	}
}
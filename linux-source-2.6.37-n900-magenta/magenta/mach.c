/*
 * mach.c
 * Copyright (c) 2012 Christina Brooks
 *
 * Mach routines.
 */

#include <linux/module.h>

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

/*
	Port list.
*/
static ke_port_t ports[MAX_PORT_COUNT];
static DECLARE_RWSEM(ports_sem);

/* Handler for kernel messages */
extern int kmsg_handle(mach_msg_header_t* msg);

/*
	Mutex for kmsgs.
*/
DEFINE_MUTEX(kmsg_exec_mutex);

/*
 * Returns a new kernel port.
 *
 * XXX
 * This port allocator is temporary. I should really use a pid map with
 * a linked list of slabs to store the ports.
 */
ke_port_t* ke_port_allocate(uint16_t type)
{
	ke_port_t* prt = NULL;
	int i = 0;

	down_write(&ports_sem);
	while (i < MAX_PORT_COUNT) {
		if (ports[i].type == KE_PORT_TYPE_FREE) {
			prt = &ports[i];
			prt->type = type;
			break;
		}

		i++;
	}
	up_write(&ports_sem);

	return prt;
}

/*
 * Finds a port by its mach port name.
 */
ke_port_t* ke_port_find_named(mach_port_t name)
{
	ke_port_t* prt = NULL;
	int i = 0;

	down_read(&ports_sem);
	while (i < MAX_PORT_COUNT) {
		if (ports[i].mp == name) {
			prt = &ports[i];
			break;
		}

		i++;
	}
	up_read(&ports_sem);

	return prt;
}

void ke_setup_task_port(struct task_struct* task)
{
	ke_port_t* kprt = NULL;

	kprt = ke_port_allocate(KE_PORT_TYPE_TASK);
	if (!kprt) {
		panic("ke_setup_task_port(): unable to creat task port for %p", task);
	}

	/* Set the task descriptor */
	kprt->c.tp.task = task;

	/* And set the port */
	task->task_port = (void*)kprt;

	printk("ke_setup_task_port(): task %p got port %d\n", task, kprt->mp);
}

ke_port_t* ipc_port_allocate(struct task_struct* task) {
	ke_port_t* kprt = NULL;
	ipc_port* prt = NULL;
	
	kprt = ke_port_allocate(KE_PORT_TYPE_IPC);
	if (!kprt) {
		return NULL;
	}

	prt = &(kprt->c.ipc);

	/* create a message queue */
	if(kfifo_alloc(&(prt->queue), PAGE_SIZE, GFP_KERNEL)) {
		panic("allocate_ipc_port: baaaaad queue alloc");
	}

	/* 
		create a completion variable to hang on if the
		queue is empty 
	*/
	init_completion(&(prt->wait_for_enqueued_data));

	return kprt;
}

static void dump_mach_msg_hdr(mach_msg_header_t* head) {
	return;

	printk(KERN_WARNING "Mach Message:\n"
	"\tbits: %p\n\tsize: %d\n\tremote: %d\n\tlocal: %d\n\tid : %d\n"
	,(void*)head->msgh_bits, head->msgh_size, head->msgh_remote_port, head->msgh_local_port, head->msgh_id);
}

SYSCALL_DEFINE1(mach_msg_trap, struct mach_msg_trap_data __user *, usr_data)
{
	mach_msg_trap_data_t trap_data;
	mach_msg_header_t tmsg;
	mach_msg_header_t* msg;
	ipc_message* im;
	ipc_message* rm;
	ke_port_t* remote;
	ke_port_t* local;
	int retval = 0;
	//boolean_t internal_message = 0;
	mach_port_t sswp; /* for swaps */

	if (!current) {
		panic("mach_msg_trap(): used without user context");
	}

	/* read in the trap data */
	if (copy_from_user(&trap_data, usr_data, sizeof(mach_msg_trap_data_t)))
		return -EFAULT;
	/* read in the temp message header */	
	if (copy_from_user(&tmsg, trap_data.msg, sizeof(mach_msg_header_t)))
		return -EFAULT;

	/*
		Read in the entire inline message. We leave an empty space
		at the end so we can place the message trailer there.

		XXX: Needs some sort of a bounds check for kalloc.
	*/
	msg = (mach_msg_header_t*)kmalloc(tmsg.msgh_size + LARGEST_TRAILER_SIZE, GFP_KERNEL);
	if (copy_from_user(msg, trap_data.msg, tmsg.msgh_size))
		return -EFAULT;

	dump_mach_msg_hdr(msg);


	/*
		*** KMSG message. ***
	*/
	if (msg->msgh_remote_port == 0 &&
		trap_data.option & MACH_SEND_MSG) {
		/*
			This is a kmsg, so handle it.

			Kmsgs (kernel messages) are special mach messages that
			are not enqueued. They are handled immediatedly. They are
			sent to remote port 0 with the MACH_SEND_MSG flag.
		*/

		mutex_lock(&kmsg_exec_mutex);
		if (msg->msgh_id == KMSG_MACH_PORT_ALLOCATE) {
			/*
				Allocate a new mach port. The only type of ports
				the userland may allocate via this function are IPC ports.
			*/
			kmsg_mach_port_allocate_msg_t* km = (kmsg_mach_port_allocate_msg_t*)msg;
			ke_port_t* prt = ipc_port_allocate(current);
			mach_port_t mp = prt->mp;

			if (copy_to_user(km->port_out, &mp, sizeof(mach_port_t)))
				retval = -EFAULT;
		}
		else {
			/* This is not an IPC related message, so offload it */
			retval = kmsg_handle(msg);
		}
		mutex_unlock(&kmsg_exec_mutex);

		/* This is it, destroy the message. */
		kfree(msg);
		return retval;
	}

	if (tmsg.msgh_bits & MACH_MSGH_BITS_COMPLEX)
	{
		/* Complex messages are not supported yet */
		printk("mach_msg(): complex messages not yet supported\n");
		return -EINVAL;
	}

	/*
		*** IPC message send. ***
	*/
	if (trap_data.option & MACH_SEND_MSG &&
		tmsg.msgh_remote_port != 0) 
	{
		/*
			Caller wants to send a mach message.
		*/

		/* 1). Find out where it wants to send it to. */
		remote = ke_port_find_named(tmsg.msgh_remote_port);
		if (!remote) {
			 printk("mach_msg(): nonexistent remote port\n");
			/* baaad port */
			kfree(msg);
			return -EINVAL;
		}

		/* Check the port type */
		if (remote->type == KE_PORT_TYPE_TASK)
		{
			/* Task port, do special handling */

			kfree(msg);
			return 0;
		}
		else if (remote->type == KE_PORT_TYPE_IPC)
		{
			/* Do nothing for now */
		}
		else 
		{
			/* Can't send to this port type */
			printk("mach_msg(): invalid remote port type\n");
			kfree(msg);
			return -EINVAL;
		}

		/* 2). Prepare the message. */
		im = (ipc_message*)kmalloc(sizeof(ipc_message), GFP_KERNEL);
		im->sender = current;
		im->msg = msg;
		im->head = tmsg; /* inline header */
		im->received = 0;
		init_completion(&(im->send_block)); /* block */

		/* 3). Enqueue a pointer to the message. */
		kfifo_in(&(remote->c.ipc.queue), &im, sizeof(im));
		printk("enqueued message at %p\n", im);

		/* 
			4). If the receiver is waiting for queue writes, let it know.
				that a new message just came in.
		*/
		complete(&(remote->c.ipc.wait_for_enqueued_data));
	}


	/*
		*** IPC message receive. ***
	*/
	if (trap_data.option & MACH_RCV_MSG &&
		tmsg.msgh_local_port != 0)
	{
		/*
			Caller wants to receive a mach message.
		*/
		local = ke_port_find_named(tmsg.msgh_local_port);
		if (!local || local->type != KE_PORT_TYPE_IPC) {
			printk("mach_msg(): invalid local port\n");
			/* baaad port */
			retval = -EINVAL;
			goto out;
		}

		if (kfifo_is_empty(&(local->c.ipc.queue))) {
			/*
				1). If the queue is empty, wait until something writes to it.
			*/
			wait_for_completion(&(local->c.ipc.wait_for_enqueued_data));
			//printk("completion lock lifted\n", im);
			if (kfifo_is_empty(&(local->c.ipc.queue))) {
				/*
					This should never happen.
					
					(someone just told us that a message was sent but
					 there are no messages in the queue)
				*/
				panic("MACH_RCV_MSG: queue empty after completion");
			}
		}

		/* 2). Dequeue the message pointer. */
		kfifo_out(&(local->c.ipc.queue), &rm, sizeof(rm));
		printk("dequeued message at %p\n", rm);

		/* 
			3). Check if this is an internal (on the same thread) message.
				If it is, don't wait for completion at the end.
		*/
		if (im == rm) {
			//internal_message = 1;
		}

		/* 
			4). Fixup the message.
				This involves reversing the ports and adding a trailer.
		*/

		sswp = rm->msg->msgh_local_port;
		rm->msg->msgh_local_port = rm->msg->msgh_remote_port;
		rm->msg->msgh_remote_port = sswp;
		/* XXX: trailer */
		/* grow by the trailer size */
		//rm->msg->msgh_size += LARGEST_TRAILER_SIZE;

		/* 5). Copy the message into the userspace. */
		if (copy_to_user(trap_data.msg, rm->msg, rm->msg->msgh_size))
		{
			printk("can't write message %p\n", rm);
			retval = -EFAULT;
			goto out;
		}

		rm->received = 1;

		/* 6). If the receiver is waiting for completion, let it know that we're done */
		complete(&(rm->send_block));
	}

	retval = 0;
out:
	if (trap_data.option & MACH_SEND_MSG &&
		tmsg.msgh_local_port != 0)
	{
		if (!im->received) {
			/* Block this thread until the sent message is dequeued */
			wait_for_completion(&(im->send_block));
		}

		/* Destroy the copied message buffer */
		kfree(im->msg);

		/* Destroy the ipc_message */
		kfree(im);
	}

	return retval;
}

int init_mach_ipc(void)
{
	/*
	 * Initialize all slots for valid port names.
	 * The port names are going to start at 20.
	 */
	int vl = 20;
	int i = 0;


	down_write(&ports_sem);
	while (i < MAX_PORT_COUNT) {
		ports[i].mp = (mach_port_t)vl;

		vl++;
		i++;
	}
	up_write(&ports_sem);

	printk("init_mach_ipc(): started mach ipc subsystem {max_ports=%d}\n", MAX_PORT_COUNT);

	return 0;
}

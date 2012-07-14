#ifndef _H_MG_MACH_PORT_TYPES_
#define _H_MG_MACH_PORT_TYPES_

#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/list.h>

#include <DarwinTypes.h>
#include <MachO.h>

#define FALSE 0
#define TRUE 1

#define MAX_PORT_COUNT 4096

typedef unsigned int natural_t;
typedef int integer_t;
typedef int boolean_t;

typedef natural_t mach_port_t;
typedef natural_t mach_port_right_t;
typedef int mach_port_delta_t;

#define MACH_PORT_RIGHT_SEND            ((mach_port_right_t) 0)
#define MACH_PORT_RIGHT_RECEIVE         ((mach_port_right_t) 1)
#define MACH_PORT_RIGHT_SEND_ONCE       ((mach_port_right_t) 2)
#define MACH_PORT_RIGHT_PORT_SET        ((mach_port_right_t) 3)
#define MACH_PORT_RIGHT_DEAD_NAME       ((mach_port_right_t) 4)
#define MACH_PORT_RIGHT_NUMBER          ((mach_port_right_t) 5)

#define KE_PORT_TYPE_FREE 0
#define KE_PORT_TYPE_TASK 1
#define KE_PORT_TYPE_IPC 2

typedef struct __ke_port_t ke_port_t;

typedef enum {
    kMachPortRightSend = 0x1,
    kMachPortRightReceive = 0x2,
    kMachPortRightSendOnce = 0x4,
    /*STYLE4 = 0x8,
    STYLE5 = 0x10,
    STYLE6 = 0x20,
    STYLE7 = 0x40,
    STYLE8 = 0x80*/
} ke_right_type_t;

typedef struct 
{
	struct task_struct *task; /* task which owns the port */
	
	struct completion wait_for_enqueued_data;
	struct kfifo queue; /* queue */

	boolean_t allocated; 
} ipc_port;

typedef struct
{
	ke_port_t* port;
	ke_right_type_t rights;
	int urefs;

	struct list_head list;
} ke_port_right_t;

/*
 * This structure represents a task port as well
 * the task's IPC space and other stuff.
 */
typedef struct 
{
	struct task_struct *task; /* task which owns the port */
	ke_port_right_t* port_rights; /* list of port rights for this task */
} task_port_t;

typedef struct __ke_port_t
{
	mach_port_t	mp; /* port name */
	uint16_t type; /* port type */

	union {
		ipc_port ipc;
		task_port_t tp;
	} c;
} ke_port_t;

#endif 
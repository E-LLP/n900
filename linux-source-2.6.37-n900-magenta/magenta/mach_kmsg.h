/*
 * mach_kmsg.h
 * Copyright (c) 2012 Christina Brooks
 *
 * Special mach messages that get interpreted by the
 * kernel.
 */

#ifndef _H_MG_MACH_KMSG_
#define _H_MG_MACH_KMSG_

#include "ipc_types.h"

#define KMSG_MACH_PORT_ALLOCATE 2000
#define KMSG_LOAD_KEXT 2001
#define KMSG_GET_DIRECTORY_ENTRIES 2002

typedef struct 
{
	mach_msg_header_t head;
	mach_port_right_t rights;
	mach_port_t* port_out;
} kmsg_mach_port_allocate_msg_t;

typedef struct 
{
	mach_msg_header_t head;
	void* buffer;
	unsigned int buffer_len;
} kmsg_load_kext_msg_t;

typedef struct 
{
	mach_msg_header_t head;
	int fd;
	int* out_error;
	void* buffer;
	unsigned int buffer_len;
} kmsg_get_directory_entries_t;


/*
 * 2100: Mach routines
 */
#define KMSG_MACH_TASK_SELF 2100

typedef struct 
{
	mach_msg_header_t head;
	mach_port_t* out_port;
} kmsg_mach_task_self_msg_t;

#endif
/*
 * ipc_types.h
 * Copyright (c) 2012 Christina Brooks
 *
 * Kernel Mach IPC layer.
 *
 * And a lot of other unrelated stuff that needs to be
 * moved out of here.
 */

#ifndef _H_MG_IPC_TYPES_
#define _H_MG_IPC_TYPES_

#include "mach_port_types.h"

#define FALSE 0
#define TRUE 1

#define MACH_MSG_OPTION_NONE	0x00000000

#define	MACH_SEND_MSG		0x00000001
#define	MACH_RCV_MSG		0x00000002
#define MACH_RCV_LARGE		0x00000004

#define MACH_MSGH_BITS_ZERO		0x00000000
#define MACH_MSGH_BITS_REMOTE_MASK	0x000000ff
#define MACH_MSGH_BITS_LOCAL_MASK	0x0000ff00
#define MACH_MSGH_BITS_COMPLEX		0x80000000U
#define MACH_MSGH_BITS_USER             0x8000ffffU

#define	MACH_MSGH_BITS_CIRCULAR		0x40000000	/* internal use only */
#define	MACH_MSGH_BITS_USED		0xc000ffffU

#define	MACH_MSGH_BITS_PORTS_MASK				\
		(MACH_MSGH_BITS_REMOTE_MASK|MACH_MSGH_BITS_LOCAL_MASK)

#define MACH_MSGH_BITS(remote, local)				\
		((remote) | ((local) << 8))
#define	MACH_MSGH_BITS_REMOTE(bits)				\
		((bits) & MACH_MSGH_BITS_REMOTE_MASK)
#define	MACH_MSGH_BITS_LOCAL(bits)				\
		(((bits) & MACH_MSGH_BITS_LOCAL_MASK) >> 8)
#define	MACH_MSGH_BITS_PORTS(bits)				\
		((bits) & MACH_MSGH_BITS_PORTS_MASK)
#define	MACH_MSGH_BITS_OTHER(bits)				\
		((bits) &~ MACH_MSGH_BITS_PORTS_MASK)

typedef integer_t mach_msg_option_t;
typedef unsigned int mach_msg_bits_t;
typedef	natural_t mach_msg_size_t;
typedef integer_t mach_msg_id_t;
typedef natural_t mach_msg_timeout_t;
typedef natural_t mach_port_right_t;
typedef	natural_t		vm_offset_t;

typedef	unsigned int mach_msg_trailer_type_t;
typedef	unsigned int mach_msg_trailer_size_t;
typedef natural_t mach_port_seqno_t;
typedef vm_offset_t mach_port_context_t;

typedef struct 
{
  mach_msg_trailer_type_t	msgh_trailer_type;
  mach_msg_trailer_size_t	msgh_trailer_size;
} mach_msg_trailer_t;

typedef struct
{
  mach_msg_trailer_type_t       msgh_trailer_type;
  mach_msg_trailer_size_t       msgh_trailer_size;
  mach_port_seqno_t             msgh_seqno;
} mach_msg_seqno_trailer_t;

typedef struct
{
  unsigned int			val[2];
} security_token_t;

typedef struct 
{
  mach_msg_trailer_type_t	msgh_trailer_type;
  mach_msg_trailer_size_t	msgh_trailer_size;
  mach_port_seqno_t		msgh_seqno;
  security_token_t		msgh_sender;
} mach_msg_security_trailer_t;

typedef struct
{
  unsigned int			val[8];
} audit_token_t;

typedef struct 
{
  mach_msg_trailer_type_t	msgh_trailer_type;
  mach_msg_trailer_size_t	msgh_trailer_size;
  mach_port_seqno_t		msgh_seqno;
  security_token_t		msgh_sender;
  audit_token_t			msgh_audit;
} mach_msg_audit_trailer_t;

typedef struct 
{
  mach_msg_trailer_type_t	msgh_trailer_type;
  mach_msg_trailer_size_t	msgh_trailer_size;
  mach_port_seqno_t		msgh_seqno;
  security_token_t		msgh_sender;
  audit_token_t			msgh_audit;
  mach_port_context_t		msgh_context;
} mach_msg_context_trailer_t; /* This is the biggest simple trailer */

#define LARGEST_TRAILER_SIZE sizeof(mach_msg_context_trailer_t)

typedef struct 
{
	mach_msg_bits_t	msgh_bits;
	mach_msg_size_t	msgh_size;
	mach_port_t		msgh_remote_port;
	mach_port_t		msgh_local_port;
	mach_msg_size_t msgh_reserved;
	mach_msg_id_t	msgh_id;
} mach_msg_header_t;



struct mach_msg_trap_data {
	mach_msg_header_t* msg;
	mach_msg_option_t option;
	mach_msg_size_t send_size;
	mach_msg_size_t receive_limit;
	mach_port_t receive_name;
	mach_msg_timeout_t timeout;
	mach_port_t notify;
};

typedef struct 
{
	mach_msg_header_t head; /* just the header, for routing */
	mach_msg_header_t* msg; /* pointer to the message in the sender's space */
	struct task_struct* sender; /* sender */
	
	struct completion send_block; /* blocking the sender while the message is enqueued */
	boolean_t received;
} ipc_message;


typedef int ipc_port_index;
typedef struct mach_msg_trap_data mach_msg_trap_data_t;

#endif

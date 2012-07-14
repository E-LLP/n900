/*
 * Multimedia device API
 *
 * Copyright (C) 2010 Nokia Corporation
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *	     Sakari Ailus <sakari.ailus@maxwell.research.nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_MEDIA_H
#define __LINUX_MEDIA_H

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/version.h>

#define MEDIA_API_VERSION	KERNEL_VERSION(0, 1, 0)

struct media_device_info {
	char driver[16];
	char model[32];
	char serial[40];
	char bus_info[32];
	__u32 media_version;
	__u32 hw_revision;
	__u32 driver_version;
	__u32 reserved[31];
};

#define MEDIA_ENTITY_ID_FLAG_NEXT		(1 << 31)

#define MEDIA_ENTITY_TYPE_SHIFT			16
#define MEDIA_ENTITY_TYPE_MASK			0x00ff0000
#define MEDIA_ENTITY_SUBTYPE_MASK		0x0000ffff

#define MEDIA_ENTITY_TYPE_DEVNODE		(1 << MEDIA_ENTITY_TYPE_SHIFT)
#define MEDIA_ENTITY_TYPE_DEVNODE_V4L		(MEDIA_ENTITY_TYPE_DEVNODE + 1)
#define MEDIA_ENTITY_TYPE_DEVNODE_FB		(MEDIA_ENTITY_TYPE_DEVNODE + 2)
#define MEDIA_ENTITY_TYPE_DEVNODE_ALSA		(MEDIA_ENTITY_TYPE_DEVNODE + 3)
#define MEDIA_ENTITY_TYPE_DEVNODE_DVB		(MEDIA_ENTITY_TYPE_DEVNODE + 4)

#define MEDIA_ENTITY_TYPE_V4L2_SUBDEV		(2 << MEDIA_ENTITY_TYPE_SHIFT)
#define MEDIA_ENTITY_TYPE_V4L2_SUBDEV_SENSOR	(MEDIA_ENTITY_TYPE_V4L2_SUBDEV + 1)
#define MEDIA_ENTITY_TYPE_V4L2_SUBDEV_FLASH	(MEDIA_ENTITY_TYPE_V4L2_SUBDEV + 2)
#define MEDIA_ENTITY_TYPE_V4L2_SUBDEV_LENS	(MEDIA_ENTITY_TYPE_V4L2_SUBDEV + 3)

#define MEDIA_ENTITY_FLAG_DEFAULT		(1 << 0)

struct media_entity_desc {
	__u32 id;
	char name[32];
	__u32 type;
	__u32 revision;
	__u32 flags;
	__u32 group_id;
	__u16 pads;
	__u16 links;

	__u32 reserved[4];

	union {
		/* Node specifications */
		struct {
			__u32 major;
			__u32 minor;
		} v4l;
		struct {
			__u32 major;
			__u32 minor;
		} fb;
		struct {
			__u32 card;
			__u32 device;
			__u32 subdevice;
		} alsa;
		int dvb;

		/* Sub-device specifications */
		/* Nothing needed yet */
		__u8 raw[184];
	};
};

#define MEDIA_PAD_FLAG_INPUT			(1 << 0)
#define MEDIA_PAD_FLAG_OUTPUT			(1 << 1)

struct media_pad_desc {
	__u32 entity;		/* entity ID */
	__u16 index;		/* pad index */
	__u32 flags;		/* pad flags */
	__u32 reserved[2];
};

#define MEDIA_LINK_FLAG_ENABLED			(1 << 0)
#define MEDIA_LINK_FLAG_IMMUTABLE		(1 << 1)
#define MEDIA_LINK_FLAG_DYNAMIC			(1 << 2)

struct media_link_desc {
	struct media_pad_desc source;
	struct media_pad_desc sink;
	__u32 flags;
	__u32 reserved[2];
};

struct media_links_enum {
	__u32 entity;
	/* Should have enough room for pads elements */
	struct media_pad_desc __user *pads;
	/* Should have enough room for links elements */
	struct media_link_desc __user *links;
	__u32 reserved[4];
};

#define MEDIA_IOC_DEVICE_INFO		_IOWR('M', 1, struct media_device_info)
#define MEDIA_IOC_ENUM_ENTITIES		_IOWR('M', 2, struct media_entity_desc)
#define MEDIA_IOC_ENUM_LINKS		_IOWR('M', 3, struct media_links_enum)
#define MEDIA_IOC_SETUP_LINK		_IOWR('M', 4, struct media_link_desc)

#endif /* __LINUX_MEDIA_H */
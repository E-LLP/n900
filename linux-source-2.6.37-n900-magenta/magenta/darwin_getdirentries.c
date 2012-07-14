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

#pragma pack()
#define __DARWIN_MAXPATHLEN	1024

typedef struct __darwin_dent {
	uint64_t  d_ino;      /* file number of entry */
	uint64_t  d_seekoff;  /* seek offset (optional, used by servers) */
	uint16_t  d_reclen;   /* length of this record */
	uint16_t  d_namlen;   /* length of string in d_name */
	uint8_t   d_type;     /* file type, see below */
	char      d_name[__DARWIN_MAXPATHLEN]; /* entry name (up to MAXPATHLEN bytes) */
} darwin_dirent_t;

struct getdents_callback64 {
	darwin_dirent_t* current_dir;
	darwin_dirent_t* previous;
	int count;
	int error;
};

static int filldir64(void * __buf,
	const char * name,
	int namlen,
	loff_t offset,
	u64 ino,
	unsigned int d_type)
{
	darwin_dirent_t __user *dirent;
	struct getdents_callback64 * buf = (struct getdents_callback64 *) __buf;

	int reclen = ALIGN(offsetof(darwin_dirent_t, d_name) + namlen + 1, sizeof(u64));

	//int reclen = ALIGN(sizeof(darwin_dirent_t), sizeof(u64));

	buf->error = -EINVAL;	/* only used if we fail.. */
	if (reclen > buf->count) {
		return -EINVAL;
	}
	dirent = buf->previous;

	if (dirent) {
		if (__put_user(offset, &dirent->d_seekoff)) {
			goto efault;
		}
	}

	dirent = buf->current_dir;

	if (__put_user(ino, &dirent->d_ino)) {
		goto efault;
	}
	if (__put_user(0, &dirent->d_seekoff)) {
		goto efault;
	}
	if (__put_user(reclen, &dirent->d_reclen)) {
		goto efault;
	}
	if (__put_user(d_type, &dirent->d_type)) {
		goto efault;
	}
	if (__put_user(namlen, &dirent->d_namlen)) {
		/* BRING THE BSD PAIN */
		goto efault;
	}
	if (copy_to_user(&dirent->d_name, name, namlen)) {
		goto efault;
	}

	char* thing = ((char*)(&dirent->d_name)) + (namlen);
	if (__put_user(0, thing)) {
		goto efault;
	}

	buf->previous = dirent;
	dirent = (void __user *)dirent + reclen;
	buf->current_dir = dirent;
	buf->count -= reclen;
	return 0;
efault:
	buf->error = -EFAULT;
	return -EFAULT;
}

void get_dents_darwin(kmsg_get_directory_entries_t* km)
{
	unsigned int fd = km->fd;
	void* dirent = km->buffer;
	unsigned int count = km->buffer_len;

	struct file * file;
	darwin_dirent_t __user * lastdirent;
	struct getdents_callback64 buf;
	int error;

	error = -EFAULT;
	if (!access_ok(VERIFY_WRITE, dirent, count))
		goto out;

	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	buf.current_dir = dirent;
	buf.previous = NULL;
	buf.count = count;
	buf.error = 0;

	error = vfs_readdir(file, filldir64, &buf);
	if (error >= 0) {
		error = buf.error;
	}

	lastdirent = buf.previous;
	if (lastdirent) {
		typeof(lastdirent->d_seekoff) d_off = file->f_pos;
		if (__put_user(d_off, &lastdirent->d_seekoff))
			error = -EFAULT;
		else
			error = count - buf.count;
	}
	fput(file);
out:
	//printk(KERN_WARNING "get_dents_darwin(%d, %p, %d) = %d", km->fd, km->buffer, km->buffer_len, error);

	__put_user(error, km->out_error);
}
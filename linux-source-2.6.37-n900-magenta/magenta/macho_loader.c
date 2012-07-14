/*
 * MachO Binary Format Support
 * Copyright (c) 2012 Christina Brooks
 *
 * A standalone kernel module responsible for loading MachO binaries
 * into the kernel. Right now this only supports ARM binaries.
 */

/*
 * Incl.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/binfmts.h>
#include <linux/string.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/personality.h>
#include <linux/elfcore.h>
#include <linux/init.h>
#include <linux/highuid.h>
#include <linux/compiler.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/security.h>
#include <linux/random.h>
#include <linux/elf.h>
#include <linux/utsname.h>
#include <linux/coredump.h>
#include <asm/uaccess.h>
#include <asm/param.h>
#include <asm/page.h>

#include <DarwinTypes.h>
#include <MachO.h>


/* 
	This needs to be in the MachO.h header.
	#####################################################
*/
#define	SEG_TEXT	"__TEXT"

/* mach_loader.h */
#define LOAD_SUCCESS            0
#define LOAD_BADARCH            1       /* CPU type/subtype not found */
#define LOAD_BADMACHO           2       /* malformed mach-o file */
#define LOAD_SHLIB              3       /* shlib version mismatch */
#define LOAD_FAILURE            4       /* Miscellaneous error */
#define LOAD_NOSPACE            5       /* No VM available */
#define LOAD_PROTECT            6       /* protection violation */
#define LOAD_RESOURCE           7       /* resource allocation failure */

#define ARM_THREAD_STATE 1

#ifndef CPU_TYPE_ARM
#define CPU_TYPE_ARM            ((cpu_type_t) 12)
#define CPU_SUBTYPE_ARM_V4T		((cpu_subtype_t) 5)
#define CPU_SUBTYPE_ARM_V6		((cpu_subtype_t) 6)
#endif

#ifndef CPU_SUBTYPE_ARM_V5TEJ
#define CPU_SUBTYPE_ARM_V5TEJ           ((cpu_subtype_t) 7)
#endif

#ifndef CPU_SUBTYPE_ARM_V7
#define CPU_SUBTYPE_ARM_V7		((cpu_subtype_t) 9)
#endif

/* ARM ONLY! */
#define trunc_page(x)           ((x) & ~PAGE_MASK)



/* 
	Forward declarations
	#####################################################
*/
static int fucking_core_dumper(struct coredump_params *cprm);
static int load_macho_binary(struct linux_binprm *bprm, struct pt_regs *regs);
static int load_macho_library(struct file *);
static unsigned long macho_map(struct file *, unsigned long, struct elf_phdr *,
				int, int, unsigned long);



/* 
	Impl
	#####################################################
*/
#define round_page(_v) (((_v) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))


static struct linux_binfmt macho_format = {
		.module		= THIS_MODULE,
		.load_binary	= load_macho_binary,
		.load_shlib	= load_macho_library,
		.core_dump	= fucking_core_dumper, /* YOU GET NOTHING! */
		.min_coredump	= 0,
		.hasvdso	= 0
};

#define BAD_ADDR(x) ((unsigned long)(x) >= TASK_SIZE)

/* Let's use some macros to make this stack manipulation a little clearer */
#ifdef CONFIG_STACK_GROWSUP
#define STACK_ADD(sp, items) ((elf_addr_t __user *)(sp) + (items))
#define STACK_ROUND(sp, items) \
	((15 + (unsigned long) ((sp) + (items))) &~ 15UL)
#define STACK_ALLOC(sp, len) ({ \
	elf_addr_t __user *old_sp = (elf_addr_t __user *)sp; sp += len; \
	old_sp; })
#else
#define STACK_ADD(sp, items) ((elf_addr_t __user *)(sp) - (items))
#define STACK_ROUND(sp, items) \
	(((unsigned long) (sp - items)) &~ 15UL)
#define STACK_ALLOC(sp, len) ({ sp -= len ; sp; })
#endif

static unsigned long load_macho_interp(struct elfhdr *interp_elf_ex,
		struct file *interpreter, unsigned long *interp_map_addr,
		unsigned long no_base)
{
	panic("load_macho_interp: not implemented, use ml_loadDylinker instead. ");
}


static unsigned long randomize_stack_top(unsigned long stack_top)
{
	return stack_top;
}

static int ml_setBrk(unsigned long start, unsigned long end)
{
	start = PAGE_ALIGN(start);
	end = PAGE_ALIGN(end);
	if (end > start) {
		unsigned long addr;
		down_write(&current->mm->mmap_sem);
		addr = do_brk(start, end - start);
		up_write(&current->mm->mmap_sem);
		if (BAD_ADDR(addr))
			return addr;
	}
	current->mm->start_brk = current->mm->brk = start;
	return 0;
}

static int _verboseLog = 0;

/* 
	LOADER
	#####################################################
*/
typedef int	vm_offset_t;
typedef int	vm_size_t;

static int ml_loadDylinker(struct linux_binprm *bprm, int file_size, struct dylinker_command * lcp, struct file **linker_file) {
	/*
		Setup the dynamic linker.
	*/
	char *name;
	char *p;
	
	if (lcp->cmdsize < sizeof(*lcp))
		return (LOAD_BADMACHO);

	name = (char *)lcp + lcp->name.offset;

	/* Make sure the linker path is null terminated */
	p = name;
	do {
		if (p >= (char *)lcp + lcp->cmdsize)
			return(LOAD_BADMACHO);
	} while (*p++);

	if (_verboseLog) 
		printk(KERN_WARNING "ml_loadDylinker: dynamic linker is @'%s'\n", name);

	/*
		Load the linker executable file.
	*/
	*linker_file = open_exec(name);
	if (IS_ERR(*linker_file)) {
		printk(KERN_WARNING "ml_loadDylinker: can't execute the dynamic linker\n");
		return(LOAD_BADMACHO);
	}

	return LOAD_SUCCESS;
}

static int ml_loadUnixThread(struct linux_binprm *bprm, int file_size, struct arm_thread_command * tcp, void** entry) {
	/*
		Setup the main thread.
	*/
	
	/* sanity */
	if (tcp->flavor != ARM_THREAD_STATE) {
		printk(KERN_WARNING "ml_loadUnixThread: main thread is of the wrong type %d (need %d)\n",
				tcp->flavor,
				ARM_THREAD_STATE);
	}
	else if (tcp->count != 17) {
		printk(KERN_WARNING "ml_loadUnixThread: has the wrong number of arm registers %d (need %d)\n",
				tcp->count,
				17);
	}
	else {
		/**/
		
		/* Entry point */
		if (_verboseLog)
			printk(KERN_WARNING "ml_loadUnixThread: success, pc @ %d\n", tcp->state.r15);
		
		*entry = (void*)tcp->state.r15;
	}
	
	return LOAD_SUCCESS;
}

static int ml_loadSegment(struct linux_binprm *bprm,
						  int file_size,
						  struct segment_command* scp,
						  int* top,
						  void** first_text,
						  vm_offset_t slide)
{
	/*
		Bootstrap a macho segment.
	*/
	
	/***/
	size_t segment_command_size = sizeof(struct segment_command);
	size_t total_section_size = scp->cmdsize - segment_command_size;
	size_t single_section_size  = sizeof(struct section);
	
	int ret;
	
	/* setup mapping vars */
	vm_offset_t map_addr = round_page(scp->vmaddr);
	vm_size_t map_size = round_page(scp->filesize);
	vm_size_t seg_size = round_page(scp->vmsize);
	vm_offset_t map_offset = scp->fileoff;
	vm_size_t delta_size;
	vm_offset_t addr;
	/*
	 	Segment sanity checks.
	*/
	/* is the command right? */
	if (scp->cmdsize < segment_command_size) {
		printk(KERN_WARNING "ml_loadSegment(%.*s): malformed command", 16, scp->segname);
		return (LOAD_BADMACHO);
	}
	/* is the segment in range? */
	if (scp->fileoff + scp->filesize < scp->fileoff ||
		scp->fileoff + scp->filesize > (uint64_t)file_size) {
		printk(KERN_WARNING "ml_loadSegment(%.*s): out of range", 16, scp->segname);
		return (LOAD_BADMACHO);
	}
	/* is page aligned? */
	if ((scp->fileoff & (PAGE_SIZE-1)) != 0) {
		printk(KERN_WARNING "ml_loadSegment(%.*s): not page aligned", 16, scp->segname);
		return (LOAD_BADMACHO);
	}
	
	
	
	/*
		Print some info about the segment.
	*/
	if (_verboseLog)
		printk(KERN_WARNING "ml_loadSegment(%.*s): addr %d, filesize %d, vmsize %d\n",
				16,
				scp->segname,
				map_addr,
				map_size,
				seg_size);
	
	/*
	do_mmap(struct file *file,
			unsigned long addr,
			unsigned long len,
			unsigned long prot,
			unsigned long flag,
			unsigned long offset)
	*/
	
	/* Actually map in the segment into the correct location.
	 */
	if (map_size > 0) {
		/* There is something from the file to map */
		
		addr = PAGE_ALIGN(map_addr + slide);
		
		if (_verboseLog)
			printk(KERN_WARNING "ml_loadSegment(%.*s): seg mmap @ %d, offset %d \n",
					16,
					scp->segname,
					addr,
					map_offset);
		
		/* lock */
		down_write(&current->mm->mmap_sem);
		void* mapped = 		
		do_mmap(bprm->file,
				addr,
				map_size,
				PROT_WRITE | PROT_READ | PROT_EXEC,
				MAP_PRIVATE | MAP_FIXED,
				map_offset);
		/* unlock */
		up_write(&current->mm->mmap_sem);
		
		if (strncmp(scp->segname, SEG_TEXT, 16) == 0) {
			/*
				This is a text segment, check if it's mapped from zero and then
				bump up the first_text variable to make sure it points to its start.
			*/
			if (map_offset == 0) {
				if (_verboseLog)
					printk(KERN_WARNING "ml_loadSegment(%.*s): this is the base segment \n", 16, scp->segname);
				
				*first_text = (void*)(addr);
			}
		}
		
		if ((mapped) <= 0) {
			printk(KERN_WARNING "ml_loadSegment(%.*s): map file seg failed \n", 16, scp->segname);
			ret = LOAD_RESOURCE;
			goto out;
		}
		else {
			if (_verboseLog)
				printk(KERN_WARNING "ml_loadSegment(%.*s): mapped in @ %d \n", 16, scp->segname, (void*)mapped);
		}
		
		/*
		 *	If the file didn't end on a page boundary,
		 *	we need to zero the leftover.
		 */
		delta_size = map_size - scp->filesize;
		if (delta_size > 0) {
			if (_verboseLog)
				printk(KERN_WARNING "ml_loadSegment(%.*s): fixxuuup \n", 16, scp->segname);	
		}
	}
	
	/*	If the virtual size of the segment is greater
	 *	than the size from the file, we need to allocate
	 *	anonymous zero fill memory for the rest. 
	 */
	delta_size = seg_size - map_size;
	if (delta_size > 0) {
		addr = PAGE_ALIGN(map_addr + map_size + slide);
		
		if (_verboseLog)
			printk(KERN_WARNING "ml_loadSegment(%.*s): mmap @ %d, size: %d\n", 16, scp->segname, addr, delta_size);
		
		/* lock */
		down_write(&current->mm->mmap_sem);
		void* mapped = 		
		do_mmap(NULL,
				addr,
				delta_size,
				PROT_WRITE | PROT_READ | PROT_EXEC,
				MAP_FIXED | MAP_PRIVATE,
				0);
		/* unlock */
		up_write(&current->mm->mmap_sem);
		
		if ((mapped) <= 0) {
			printk(KERN_WARNING "ml_loadSegment(%.*s): map anon failed \n", 16, scp->segname);
			ret = LOAD_RESOURCE;
			goto out;
		}
		else {
			if (_verboseLog)
				printk(KERN_WARNING "ml_loadSegment(%.*s): anon chunk mapped in @%p \n", 16, scp->segname, (void*)mapped);
		}
	}
	
	if (*top < (map_addr + slide + seg_size)) {
		/* highest address so far, update the top variable */
		*top = ((map_addr + slide + seg_size));
	}
	
	/* mapped in successfully */
	ret = LOAD_SUCCESS;
	
out:		
	return ret;
}

static int ml_getFileSize(struct file* file) {
	/* file size from struct file */
	
	/* sanity checks */
	if (!file)
		return -1;
	if (!file->f_path.dentry)
		return -1;
	if (!file->f_path.dentry->d_inode) 
		return -1;
		
	return file->f_path.dentry->d_inode->i_size;
}

static int ml_checkImage(struct file* file, macho_header* head) 
{
	/*
		Sanity checks.
	*/
	int retval = -ENOEXEC;
	int file_size = 0;
	size_t macho_header_sz = sizeof(macho_header);
	
	if (head->magic != MH_MAGIC) {
		printk(KERN_WARNING "ml_checkImage: binary is not a macho binary (magic: 0x%p) \n", (void*)head->magic);
		retval = -ENOEXEC;
		goto out_ret;
	}
	
	/*
		Validate architecture.
	*/
	if (head->cputype != CPU_TYPE_ARM) {
		printk(KERN_WARNING "ml_checkImage: wrong architecture in the executable\n");
		retval = -EINVAL;
		goto out_ret;
	}
	
	/*
		Run ARM-specific validation checks
	*/
	if (head->cputype == CPU_TYPE_ARM) {
		if (head->cpusubtype == CPU_SUBTYPE_ARM_V7)
		{
			if (cpu_architecture() != CPU_ARCH_ARMv7) {
				printk(KERN_WARNING "ml_checkImage: armv7 executables are not supported by the current platform\n");
				retval = -EINVAL;
				goto out_ret;
			}
		}
		else if (head->cpusubtype == CPU_SUBTYPE_ARM_V6)
		{
			if (cpu_architecture() != CPU_ARCH_ARMv6) {
				printk(KERN_WARNING "ml_checkImage: armv6 executables are not supported by the current platform\n");
				retval = -EINVAL;
				goto out_ret;
			}
		}
		else {
			printk(KERN_WARNING "ml_checkImage: unrecognized arm version in the executable (%d)\n", head->cpusubtype);
			retval = -EINVAL;
			goto out_ret;
		}
	}
	
	
	/*
	 	Make sure the file size can be retrieved in order 
	  	to perform sanity checks on the file.
	 */
	file_size = ml_getFileSize(file);
	if (file_size < 0) {
		printk(KERN_WARNING "ml_checkImage: can't retrieve binary size \n");
		retval = -EINVAL;
		goto out_ret;
	}
	
	/*
		Main portion of the sanity checks for the macho file.
	*/
	
	retval = -EINVAL;
	/* can we map it? */
	if (!file->f_op||!file->f_op->mmap) {
		printk(KERN_WARNING "ml_checkImage: binary file can't be mapped in \n");
		goto out_ret;
	}
	/* sane lc size? */
	if ((off_t)(macho_header_sz + head->sizeofcmds) > file_size) {
		printk(KERN_WARNING "ml_checkImage: malformed load commands size \n");
		goto out_ret;
	}
	if (head->filetype != MH_EXECUTE) {
		printk(KERN_WARNING "IGN:ml_checkImage: macho file is not executable \n");
		//goto out_ret;
	}
	
	/* Print some info about the macho file */
	if (_verboseLog)
		printk(KERN_WARNING "ml_checkImage: valid macho file: \n\tmagic: 0x%p \n\tsize: %d\n",
				(void*)head->magic,
				file_size);
	
	retval = 0;
	
out_ret:	
	return retval;
}

static int ml_bootstrapDylinker(struct file* file, /* file for the dylinker*/
								int* top_data, /* top of image data */
								void** first_text,
								void** entry_point) /* first text segment of the linker */
								
{
	/* fake bprm for the segment loader*/
	struct linux_binprm bprm;
	
	int retval;
	int load_addr = *top_data;
	size_t macho_header_sz = sizeof(macho_header);
	macho_header* head = kmalloc(macho_header_sz, GFP_KERNEL);
	int file_size = 0;
	
	/* this is for LC loader */
	int ret = 0;
	size_t offset;
	size_t oldoffset;
	uint32_t ncmds;
	uint8_t* addr;
	
	if (_verboseLog)
		printk(KERN_WARNING "ml_bootstrapDylinker: loading dynamic linker @ %d\n", load_addr);

	/*
		Read in the macho header.
	*/
	kernel_read(file, 0, head, macho_header_sz);

	retval = ml_checkImage(file, head);
	if (retval) {
		retval = LOAD_BADMACHO;
		printk(KERN_WARNING "ml_bootstrapDylinker: dylinker image failed sanity checks, not loading \n");
		goto out_ret;
	}
	
	/*
		XXX: this should be retrieved by ml_checkImage()
	*/
	file_size = ml_getFileSize(file);
	
	/*
		Read the load commands from the file.
	*/
	offset = 0;
	ncmds = head->ncmds;
	addr = kmalloc(head->sizeofcmds, GFP_KERNEL); /***/
	retval = -EINVAL;
	
	/* read in load commands */
	kernel_read(file, macho_header_sz, addr, head->sizeofcmds);
	
	bprm.file = file;
	
	while (ncmds--) {
		/* LC pointer */
		struct load_command	*lcp = 
		(struct load_command *)(addr + offset);
		
		oldoffset = offset;
		offset += lcp->cmdsize;
		
		if (oldoffset > offset ||
		    lcp->cmdsize < sizeof(struct load_command) ||
		    offset > head->sizeofcmds + macho_header_sz)
		{
			printk(KERN_WARNING "ml_bootstrapDylinker: malformed binary - lc overflow \n");
			goto lc_ret;
		}
		
		/*  Parse load commands.
		 
			We only need a bare minimum to get the image up an running. Dyld will
			take care of all the other stuff.
		 */
		switch(lcp->cmd) {
			case LC_SEGMENT:
			{
				/*
					Load and slide a dylinker segment.
				*/
				ret = ml_loadSegment(&bprm,
									file_size,
									(struct segment_command*)lcp,
									top_data, /* keep bumping the same top_data */
									first_text, /* first text segment */
									load_addr); /* slide up */
				
				if (ret != LOAD_SUCCESS) {
					printk(KERN_WARNING "ml_bootstrapDylinker: segment loading failure \n");
					goto lc_ret;
				}
				break;
			}
			case LC_UNIXTHREAD:
			{
				ret = ml_loadUnixThread(&bprm,
										file_size,
										(struct arm_thread_command*)lcp,
										entry_point);
										
				if (ret != LOAD_SUCCESS) {
					printk(KERN_WARNING "ml_bootstrapDylinker: unix thread loading failure \n");
					goto lc_ret;
				}
				break;
			}
			default: 
			{
				if (_verboseLog)
					printk(KERN_WARNING "ml_bootstrapDylinker: unsupported lc 0x%p \n", (void*)lcp->cmd);
				
				break;
			}
		}
	}


	/* loaded successfully */
	retval = LOAD_SUCCESS;
	
	/* free resources */
	lc_ret:
		kfree(addr);
	
	out_ret:	
		kfree(head);
		return retval;
}

static struct page* dpages[1] = {NULL};

static void wire_weird_pages(void)
{
	/* 0x80000000 */
	if (dpages[0] == NULL)
	{
		dpages[0] = alloc_pages(GFP_KERNEL, 0);
	}


	down_write(&current->mm->mmap_sem);
	int ret = 
	install_special_mapping(current->mm,
		0x80000000,
		PAGE_SIZE,
		VM_READ | VM_WRITE | VM_SHARED | VM_DONTCOPY,
		dpages);
	up_write(&current->mm->mmap_sem);

	void* addr = page_address(dpages[0]);

	memset(addr, 'w', PAGE_SIZE);

	printk("wired weird page! (%p, %d, %p)\n", dpages[0], ret, addr);
}

static int load_macho_binary(struct linux_binprm *bprm, struct pt_regs *regs)
{ 
	unsigned long def_flags = 0;
	void* entry_point = 0;
	int retval = -ENOEXEC;
	int file_size = 0;
	int executable_stack = EXSTACK_DEFAULT;
	size_t macho_header_sz = sizeof(macho_header);
	macho_header* head = ((macho_header*)bprm->buf);
	struct file *linker_file = NULL;
	
	/* have we got enough space? */
	if (!head) {
		retval = -ENOMEM;
		goto out_ret;
	}
	
	retval = ml_checkImage(bprm->file, head);
	if (retval) {
		printk(KERN_WARNING "load_macho_binary: image failed sanity checks, not loading \n");
		goto out_ret;
	}
	
	/*
		XXX: this should be retrieved by ml_checkImage()
	*/
	file_size = ml_getFileSize(bprm->file);
	
	/*
		The file seems to be alright, so set up an environment for the 
		new binary to run in. After this, the old image will no longer be 
		usable. If some of the load commands are broken, this process is doomed.
	*/
	retval = flush_old_exec(bprm);
	if (retval) {
		panic("load_macho_binary: flush_old_exec failed\n");
	}
	else {
		current->flags &= ~PF_FORKNOEXEC;
		current->mm->def_flags = def_flags;
		
		setup_new_exec(bprm);
		
		/* set personality */
		unsigned int personality = current->personality & ~PER_MASK;
		personality |= PER_LINUX;
		
		/*
		 	This flag has to be set for 32x architectures (I think).
		*/
		personality |= ADDR_LIMIT_32BIT;
		
		set_personality(personality);

		/* set stuff */
		current->mm->free_area_cache = current->mm->mmap_base;
		current->mm->cached_hole_size = 0;
		//retval = setup_arg_pages(bprm, randomize_stack_top(STACK_TOP), executable_stack);
					
		if (retval < 0) {
			//send_sig(SIGKILL, current, 0);
			//goto out_ret;
		}
		
		/* stack */
		current->mm->start_stack = bprm->p;
	}
	
	
	/*
		Read the load commands from the file.
	*/
	size_t offset;
	size_t oldoffset;
	uint32_t ncmds;
	uint8_t* addr;

	offset = 0;
	ncmds = head->ncmds;
	addr = kmalloc(head->sizeofcmds, GFP_KERNEL); /***/
	retval = -EINVAL;
	
	int ret = 0;
	
	/*
		Top of the image data. This is needed to position the heap.
	*/
	int top_data = 0;
	
	/*
		First text segment where the mach header is.
	*/
	void* first_text = 0;
	void* first_text_linker = 0;
	
	/* read in load commands */
	kernel_read(bprm->file, macho_header_sz, addr, head->sizeofcmds);
	
	while (ncmds--) {
		/* LC pointer */
		struct load_command	*lcp = 
		(struct load_command *)(addr + offset);
		
		oldoffset = offset;
		offset += lcp->cmdsize;
		
		if (oldoffset > offset ||
		    lcp->cmdsize < sizeof(struct load_command) ||
		    offset > head->sizeofcmds + macho_header_sz)
		{
			printk(KERN_WARNING "load_macho_binary: malformed binary - lc overflow \n");
			goto lc_ret;
		}
		
		/*  Parse load commands.
		 
			We only need a bare minimum to get the image up an running. Dyld will
			take care of all the other stuff.
		 */
		switch(lcp->cmd) {
			case LC_SEGMENT:
				ret = ml_loadSegment(bprm, file_size, (struct segment_command*)lcp, &top_data, &first_text, 0);
				if (ret != LOAD_SUCCESS) {
					printk(KERN_WARNING "load_macho_binary: segment loading failure \n");
					goto lc_ret;
				}
				break;
			case LC_LOAD_DYLINKER:
				ret = ml_loadDylinker(bprm, file_size, (struct dylinker_command*)lcp, &linker_file);
				if (ret != LOAD_SUCCESS) {
					printk(KERN_WARNING "load_macho_binary: dylinker loading failure \n");
					goto lc_ret;
				}
				else {
					/* done */
				}
				break;
			case LC_UNIXTHREAD:
				ret = ml_loadUnixThread(bprm, file_size, (struct arm_thread_command*)lcp, &entry_point);
				if (ret != LOAD_SUCCESS) {
					printk(KERN_WARNING "load_macho_binary: unix thread loading failure \n");
					goto lc_ret;
				}
				break;
			default: 
				if (_verboseLog)
					printk(KERN_WARNING "load_macho_binary: unsupported lc 0x%p \n", (void*)lcp->cmd);

				break;
		}
	}
	
	/*
		Bootstrap the dynamic linker if needed.
	*/
	if (linker_file) {
		int dylinker_load_addr = top_data;
		
		ml_bootstrapDylinker(linker_file,
							&top_data,
							&first_text_linker,
							&entry_point);
		
		/* slide the entry point */
		entry_point = entry_point + dylinker_load_addr;
			
		if (_verboseLog)				
			printk(KERN_WARNING "load_macho_binary: dylinker's first text segment @ %d, new pc @ %d \n",
					first_text_linker,
					(int)entry_point);
	}
	
	/*
		Now, I don't know what these are used for, but I'm fairly sure
		they're *very* important. So let's set them up. 
		
		See 'linux/mm_types.h':
		unsigned long start_code, end_code, start_data, end_data;
		unsigned long start_brk, brk, start_stack;
	*/	
	current->mm->start_code = 0; /* IMP */
	current->mm->end_code = top_data; /* IMP */
	current->mm->start_data = 0;
	current->mm->end_data = top_data;
		
	if (_verboseLog)
		printk(KERN_WARNING "load_macho_binary: setting up heap ...\n");

	/* Set up an empty heap. This will be grown as more memory is allocated.  */
	int brkret = ml_setBrk(top_data, top_data);

	if (_verboseLog)
		printk(KERN_WARNING "load_macho_binary: setting up misc ...\n");

	/* setup misc stuff */
	set_binfmt(&macho_format);
	install_exec_creds(bprm);

	/*
		Stack (grows down on ARM).
	*/
	uint32_t* stack = bprm->p;
	uint32_t* argv_array;
	uint32_t* argv;
	uint32_t* envp_array;
	uint32_t* envp;
	uint32_t total_argv_size;
	uint32_t total_env_size;

	/* Construct envp array. */
	envp = envp_array = stack = (uint32_t*)stack - ((bprm->envc+1));

	/* Construct argv array. */
	argv = argv_array = stack = (uint32_t*)stack - ((bprm->argc+1));

	if (_verboseLog)
		printk(KERN_WARNING "load_macho_binary: setting up stack @ %p ...\n", (uint32_t*)stack);

	uint32_t argc = bprm->argc;
	uint32_t envc = bprm->envc;
	char* p = bprm->p;

	/* Set up argv pointers */
	current->mm->arg_start = (unsigned long)p;
	while(argc--) {
		char c;

		put_user(p,argv++);
		do {
			get_user(c,p++);
		} while (c);
	}
	put_user(NULL,argv);

	/* Set up envp pointers */
	current->mm->arg_end = current->mm->env_start = (unsigned long) p;
	while(envc--) {
		char c;

		put_user(p,envp++);
		do {
			get_user(c,p++);
		} while (c);
	}
	put_user(NULL,envp);
	current->mm->env_end = (unsigned long) p;

	/*
		The actual stuff passed to the linker goes here.
	*/
	stack = (uint32_t*)stack - (4);

	stack[0] = (uint32_t)first_text; /* mach_header */
	stack[1] = bprm->argc; /* argc */
	stack[2] = argv_array; /* argv */
	stack[3] = (uint32_t)first_text_linker; /* linker's mach_header */
	
	if (_verboseLog)
		printk(KERN_WARNING "load_macho_binary: setting up main thread ...\n");	
	
	/*
		Set up the main thread
	*/
	if (BAD_ADDR(entry_point)) {
		/* entry point is not executable */
		
		printk(KERN_WARNING "load_macho_binary: bad entry point \n");
		force_sig(SIGSEGV, current);
		retval = -EINVAL;
		goto lc_ret;
	}
	
	if (_verboseLog)
		printk(KERN_WARNING "load_macho_binary: setting up registers ...\n");

	/* 
		See 'start_thread' in 'processor.h'
		'start_thread' provides an ELF implementation of this function.
		This is for the Darwin ABI implementation which is used by iPhoneOS binaries.
	*/
	unsigned long initial_pc = (unsigned long)entry_point;	
	
	/* exit supervisor and enter user */
	set_fs(USER_DS);
	memset(regs->uregs, 0, sizeof(regs->uregs));
	regs->ARM_cpsr = USR_MODE;	

	/* not sure */
	if (elf_hwcap & HWCAP_THUMB && initial_pc & 1)
		regs->ARM_cpsr |= PSR_T_BIT;
		
	/* set up control regs */	
	regs->ARM_cpsr |= PSR_ENDSTATE;	
	regs->ARM_pc = initial_pc & ~1;		/* pc */
	regs->ARM_sp = stack;		/* sp */

	/* This is actually ignored, but set it anyway */
	regs->ARM_r2 = stack[2];	/* r2 (envp) */	
	regs->ARM_r1 = stack[1];	/* r1 (argv) */
	regs->ARM_r0 = stack[0];	/* r0 (argc) */	
	
	/* this will work for mmu and nonmmu */
	nommu_start_thread(regs);
	
	wire_weird_pages();	
			
	/*
		Binary is now loaded. Return 0 to signify success.
	*/
	retval = 0;

	if (_verboseLog)
		printk(KERN_WARNING "load_macho_binary: complete, heap starts at %d, brkret %d \n", top_data, brkret);

	/*
	 	Teardown
	*/
	lc_ret:
		kfree(addr);
	out_ret:
		return retval;
}

static int fucking_core_dumper(struct coredump_params *cprm)
{
	printk(KERN_WARNING "----- Core Dump -----\n");

	printk(KERN_WARNING "PID: %d", current->pid);

	printk(KERN_WARNING "Received Signal: %ld", cprm->signr);

	printk(KERN_WARNING "Register Dump:\n"
	"\tpc @ %p (%d), sp @ %p \n"
	"\tr0 @ %p, r1 @ %p, r2 @ %p, r3 @ %p, r4 @ %p \n"
	"\tr5 @ %p, r6 @ %p, r7 @ %p, r8 @ %p, r9 @ %p \n"
	"\tr10 @ %p, lr @ %p,\n",
	(void*)cprm->regs->ARM_pc,
	(int)cprm->regs->ARM_pc,
	(void*)cprm->regs->ARM_sp,
	(void*)cprm->regs->ARM_r0,
	(void*)cprm->regs->ARM_r1,
	(void*)cprm->regs->ARM_r2,
	(void*)cprm->regs->ARM_r3,
	(void*)cprm->regs->ARM_r4,
	(void*)cprm->regs->ARM_r5,
	(void*)cprm->regs->ARM_r6,
	(void*)cprm->regs->ARM_r7,
	(void*)cprm->regs->ARM_r8,
	(void*)cprm->regs->ARM_r9,
	(void*)cprm->regs->ARM_r10,
	(void*)cprm->regs->ARM_lr);
	
	return 0;
}

/* This is really simpleminded and specialized - we are loading an
   a.out library that is given an ELF header. */
static int load_macho_library(struct file *file)
{
	panic("load_macho_library: not implemented.");
}

/* CoreDump code stripped from here */

static int __init init_macho_binfmt(void)
{
	printk(KERN_WARNING "init_macho_binfmt: MachO binary loader initialized! (load: %p) \n", load_macho_binary);
	
	return register_binfmt(&macho_format);
}

static void __exit exit_macho_binfmt(void)
{
	unregister_binfmt(&macho_format);
}

module_init(init_macho_binfmt);
module_exit(exit_macho_binfmt);


/*
 * Fuck everything about this.
 */
MODULE_LICENSE("GPL");

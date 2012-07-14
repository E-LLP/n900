/*
 * kext.c
 * Copyright (c) 2012 Christina Brooks
 *
 * What is this, I don't even ...
 */

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
#include "ke_runtime.h"
#include "loader.h"

int (*ray)(void);

typedef struct __KXFile {
	unsigned char *fMachO;
	struct symtab_command *fSymtab;
	
	uintptr_t fSegmentOffset;
	char *fStringBase;
	struct nlist *fSymbolBase;
    const struct nlist *fLocalSyms;
} KXFile;

static const struct nlist *
kld_find_symbol_by_name(KXFile *file, const char* name)
{
	/*
	 This is slow, but I don't care.
	 */
	
	const struct nlist *sym;
	int nsyms;
	
	nsyms = file->fSymtab->nsyms;
	sym = file->fSymbolBase;
	
	while (nsyms--) {
		/*
		 if ((sym->n_type & N_EXT))
		 return NULL;
		 */
		
		long strx = sym->n_un.n_strx;
		const char *symname = file->fStringBase + strx;
		
		if (strcmp(name, symname) == 0 && !(sym->n_type & N_STAB))
			return sym;
		
		sym += 1;
	}
	
	return NULL;
}

static const struct nlist *
kld_find_symbol_by_address(KXFile *file, void *entry)
{
	/*
		This is slow, but I don't care.
	 */
	
	const struct nlist *sym;
	int nsyms;
	
	nsyms = file->fSymtab->nsyms;
	sym = file->fSymbolBase;
	
	while (nsyms--) {
		/*
		if ((sym->n_type & N_EXT))
			return NULL;
		*/
		
		if (sym->n_value == (unsigned long) entry && !(sym->n_type & N_STAB))
			return sym;
		
		sym += 1;
	}

	return NULL;
}

Boolean kld_relocate_section(KXFile* file , struct section* sect, vm_offset_t delta)
{
	uint8_t* sbase;
	uint32_t nreloc;
	struct relocation_info *rinfo;
	
	sbase = (uint8_t*)sect->offset;
	nreloc = sect->nreloc;
	rinfo = (struct relocation_info *)(file->fMachO + sect->reloff);
	
	while (nreloc--) {
		void** entry;
		void** abs_entry;
		unsigned long r_symbolnum, r_length;
		const struct nlist *symbol;
		enum reloc_type_generic r_type;
		void *addr;
		
		/* Ignore scattered relocations */
		if ((rinfo->r_address & R_SCATTERED))
			continue;
		
		/* This is why we can't have nice things */
		entry = (void**)( (uintptr_t)rinfo->r_address + (uintptr_t)sbase );
		abs_entry = ((void**)( (uintptr_t)file->fMachO + (uintptr_t)entry ));
		
		r_type = (enum reloc_type_generic)rinfo->r_type;
		r_length = rinfo->r_length;
		
		/*
			In r_length, 2 stands for long.
		 */
		if (r_type != GENERIC_RELOC_VANILLA || r_length != 2)
			continue;
		
		r_symbolnum = rinfo->r_symbolnum;
		
		printk(KERN_WARNING "KldRelocSect: {t=%d, ba=%p, aa=%p, ln=%d, n=%ld}\n",
			   rinfo->r_type,
			   (void*)rinfo->r_address, /* relative */
			   entry, /* absolute (within the file) */
			   rinfo->r_length,
			   r_symbolnum);
		
		if (rinfo->r_extern) {
			/* External symbol entry */
			
			if(r_symbolnum >= file->fSymtab->nsyms)
			{
				printk(KERN_WARNING "KldRelocSect: invalid reloc entry\n");
				return false;
			}
			
			symbol = file->fSymbolBase;
			
			if ((symbol[r_symbolnum].n_type & N_TYPE) == N_INDR) {
				/*
					This is an indirect symbol, so get the value
					for the actual thing.
				 */
				r_symbolnum = symbol[r_symbolnum].n_value;
			}
			
			symbol = &symbol[r_symbolnum];
			
			if (symbol->n_type != (N_EXT | N_UNDF)) {
				printk(KERN_WARNING "KldRelocSect: invalid reloc symbol type - !(N_EXT | N_UNDF)\n");
				return false;
			}
		}
		else {
			/* Local symbol entry */
			
			/* Derp */
			if (r_symbolnum == R_ABS)
				continue;
			
			/* Not this pointer crap again */
			addr = *abs_entry;
			symbol = kld_find_symbol_by_address(file, addr);
			
			printk(KERN_WARNING "KldRelocSect: findByAddr(%p) = %p\n", 
				   addr,
				   symbol);
		}
		
		/* Resolve */
		
		/* Good, move on */
		printk(KERN_WARNING "KldRelocSect: FINAL {val=%p, sym=%p, ent=%p}\n",
			   *abs_entry,
			   symbol,
			   rinfo);

		rinfo++;
	}
	
	return true;
}

Boolean kld_parse_symtab(KXFile* file)
{
	const struct nlist *sym;
    unsigned int i, firstlocal = 0, nsyms;
    unsigned long strsize;
    const char *strbase;
	
	file->fSymbolBase = 
	(struct nlist *)(file->fMachO + file->fSymtab->symoff); 
	
	file->fStringBase = 
	(char *)(file->fMachO + file->fSymtab->stroff);
	
	i = 0;
	nsyms = file->fSymtab->nsyms;
	strsize = file->fSymtab->strsize;
	strbase = file->fStringBase;
	sym = file->fSymbolBase;
	
	while (i < nsyms) {
		long strx = sym->n_un.n_strx;
        const char *symname = strbase + strx;
        unsigned char n_type = sym->n_type & N_TYPE;
		
		printk(KERN_WARNING "KldParseSymtab: {type=%d, val=%p} '%s'\n",
			   n_type,
			   (void*)sym->n_value,
			   symname);
		
		n_type = sym->n_type & (N_TYPE | N_EXT);
		
        /*
			First exported symbol 
			This is done for the sake of performance
		 */
        if ( !firstlocal && (n_type & N_EXT) ) {
            firstlocal = i;
            file->fLocalSyms = sym;
        }
		
		/* Increment stuff */
		i += 1;
		sym	+= 1;
	}
	
	if (!file->fLocalSyms) {
		printk(KERN_WARNING "KldParseSymtab: no symbols found\n");
		return false;
	}
	
	printk(KERN_WARNING "KldParseSymtab: {loc=%p}\n",
		   file->fLocalSyms);
	
	return true;
}

Boolean kld_process_segment(KXFile* file, struct segment_command* seg) 
{
	struct section* sect;
	uint32_t nsects;
	
	nsects = seg->nsects;
	sect = (struct section*)((uintptr_t)seg + sizeof(struct segment_command));
	
	while (nsects--) {
		printk(KERN_WARNING "KldProcessSemgnet: sect {nrel=%d} '%s' \n",
			   sect->nreloc,
			   sect->sectname);
		
		kld_relocate_section(file, sect, 0);
		
		/* Over to the next section */
		sect++;
	}
	
	return true;
}

Boolean kld_file_map(void* buffer, long size, KXFile* file)
{
	bzero(file, sizeof(file));
	
	size_t macho_header_sz = sizeof(struct mach_header);
	uint8_t* load_commands;
	struct mach_header* head;
	
	/* command parser */
	boolean_t has_segment = FALSE;
	size_t offset;
	size_t oldoffset;
	uint32_t ncmds;
	
	/* segment */
	struct segment_command *seg_hdr;
	uintptr_t sect_offset = 0;
	uint32_t nsects = 0;
	
	head = buffer;
	load_commands = buffer + macho_header_sz;
	
	offset = 0;
	ncmds = head->ncmds;
	
	file->fMachO = buffer;
	
	printk(KERN_WARNING "KldMap: macho {fl=%d}\n", head->flags);
	
	while (ncmds--) {
		struct load_command	*lcp = 
		(struct load_command *)(load_commands + offset);
		
		oldoffset = offset;
		offset += lcp->cmdsize;
		
		if (oldoffset > offset ||
		    lcp->cmdsize < sizeof(struct load_command) ||
		    offset > head->sizeofcmds + macho_header_sz)
		{
			printk(KERN_WARNING "KldMap: malformed load command\n");
			return false;
		}
		
		/*
			Mach objects (MH_OBJECT) are only meant to have one segment that has all the bits.
		 */
		switch(lcp->cmd) {
			case LC_SEGMENT:
			{
				if (has_segment) {
					printk(KERN_WARNING "KldMap: more than one segment in the file \n");
					return false;
				}
				
				seg_hdr = (struct segment_command *)lcp;
				
				nsects = seg_hdr->nsects;
				sect_offset = (uintptr_t)(seg_hdr + sizeof(struct segment_command));
				
				file->fSegmentOffset = seg_hdr->fileoff;
				
				printk(KERN_WARNING "KldMap: LC_SEGMENT {nsects=%d} \n",
					   seg_hdr->nsects);
				
				has_segment = TRUE;
				
				break;
			}
			case LC_UUID:
			{
				/* Do. Not. Care. */
				break;
			}
			case LC_SYMTAB:
			{
				file->fSymtab = (struct symtab_command*)lcp;
				break;
			}
			default:
			{
				printk(KERN_WARNING "KldMap: unsupported load command %d \n",
					   lcp->cmd);
				
				return false;
				break;
			}
		}
	}
	
	if (!file->fSymtab) {
		printk(KERN_WARNING "KldMap: object file missing symbols \n");
		return false;
	}
	else {
		kld_parse_symtab(file);
	}
	
	if (!has_segment) {
		printk(KERN_WARNING "KldMap: object file missing segment \n");
		return false;
	}
	else {
		kld_process_segment(file, seg_hdr);
	}
	
	return true;
}


void kmsg_load_kext(kmsg_load_kext_msg_t* msg)
{
	/* 
	 *	mach_msg_header_t head;
	 *	void* buffer;
	 *	unsigned int buffer_len;
	 */

	Boolean ret;
	size_t size;
	void* buf;
	const struct nlist* nl;

 	size = msg->buffer_len;
	buf = ke_alloc(size);

	if (copy_from_user(buf, msg->buffer, size))
	{
	 	printk(KERN_WARNING "kmsg_load_kext: goof \n");
		return;
	}

	KXFile file;
	kld_file_map(buf, size, &file);

	nl = kld_find_symbol_by_name(&file, "_CoolStuff");
	
	if (nl == NULL) {
		printk(KERN_WARNING "kmsg_load_kext: symbol not found \n");
		return;
	}

	uintptr_t val = nl->n_value;
	uint16_t* whatever = (uint16_t*)((val + file.fSegmentOffset + (uintptr_t)file.fMachO) | 1);
	
	ray = (void*)whatever;
	
	printk(KERN_WARNING "func {abs=%p, rel=%p, sect=%p} \n",
		   (void*)ray,
		   (void*)nl->n_value,
		   (void*)file.fSegmentOffset);

	printk(KERN_WARNING "instruct: %p \n", (void*)*(whatever));
	printk(KERN_WARNING "call {%d} \n", ray());
}
























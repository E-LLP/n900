/*
 * loader.h
 * Copyright (c) 2012 Christina Brooks
 *
 * Loader types.
 */

#ifndef _H_MG_LOADER_
#define _H_MG_LOADER_

#include "ipc_types.h"
#include <DarwinTypes.h>
#include <MachO.h>

/* Nlist */

#define	N_STAB	0xe0  /* if any of these bits set, a symbolic debugging entry */
#define	N_PEXT	0x10  /* private external symbol bit */
#define	N_TYPE	0x0e  /* mask for the type bits */
#define	N_EXT	0x01  /* external symbol bit, set for external symbols */

/*
 * Only symbolic debugging entries have some of the N_STAB bits set and if any
 * of these bits are set then it is a symbolic debugging entry (a stab).  In
 * which case then the values of the n_type field (the entire field) are given
 * in <mach-o/stab.h>
 */

/*
 * Values for N_TYPE bits of the n_type field.
 */
#define	N_UNDF	0x0		/* undefined, n_sect == NO_SECT */
#define	N_ABS	0x2		/* absolute, n_sect == NO_SECT */
#define	N_SECT	0xe		/* defined in section number n_sect */
#define	N_PBUD	0xc		/* prebound undefined (defined in a dylib) */
#define N_INDR	0xa		/* indirect */

struct nlist {
	union {
#ifndef __LP64__
		char *n_name;	/* for use when in-core */
#endif
		int32_t n_strx;	/* index into the string table */
	} n_un;
	uint8_t n_type;		/* type flag, see below */
	uint8_t n_sect;		/* section number or NO_SECT */
	int16_t n_desc;		/* see <mach-o/stab.h> */
	uint32_t n_value;	/* value of this symbol (or stab offset) */
};

#define N_NO_DEAD_STRIP 0x0020 /* symbol is not to be dead stripped */
#define N_DESC_DISCARDED 0x0020	/* symbol is discarded */
#define N_WEAK_REF	0x0040 /* symbol is weak referenced */
#define N_WEAK_DEF	0x0080 /* coalesed symbol is a weak definition */
#define	N_REF_TO_WEAK	0x0080 /* reference to a weak symbol */
#define N_ARM_THUMB_DEF	0x0008 /* symbol is a Thumb function (ARM) */
#define N_SYMBOL_RESOLVER  0x0100 

/* Reloc stuff */

 #define R_SCATTERED 0x80000000	/* mask to be applied to the r_address field 
				   of a relocation_info structure to tell that
				   is is really a scattered_relocation_info
				   stucture */

enum reloc_type_generic
{
    GENERIC_RELOC_VANILLA,	/* generic relocation as discribed above */
    GENERIC_RELOC_PAIR,		/* Only follows a GENERIC_RELOC_SECTDIFF */
    GENERIC_RELOC_SECTDIFF,
    GENERIC_RELOC_PB_LA_PTR,	/* prebound lazy pointer */
    GENERIC_RELOC_LOCAL_SECTDIFF,
    GENERIC_RELOC_TLV		/* thread local variables */
};

struct relocation_info {
   int32_t	r_address;	/* offset in the section to what is being
				   relocated */
   uint32_t     r_symbolnum:24,	/* symbol index if r_extern == 1 or section
				   ordinal if r_extern == 0 */
		r_pcrel:1, 	/* was relocated pc relative already */
		r_length:2,	/* 0=byte, 1=word, 2=long, 3=quad */
		r_extern:1,	/* does not include value of sym referenced */
		r_type:4;	/* if not 0, machine specific relocation type */
};

#define	R_ABS	0		/* absolute relocation type for Mach-O files */

/* Actual loader stuff */
struct symtab_command {
	uint32_t	cmd;		/* LC_SYMTAB */
	uint32_t	cmdsize;	/* sizeof(struct symtab_command) */
	uint32_t	symoff;		/* symbol table offset */
	uint32_t	nsyms;		/* number of symbol table entries */
	uint32_t	stroff;		/* string table offset */
	uint32_t	strsize;	/* string table size in bytes */
};

#endif
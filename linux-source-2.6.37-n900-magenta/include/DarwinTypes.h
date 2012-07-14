#ifndef _DARWIN_TYPES_H_
#define _DARWIN_TYPES_H_

#ifndef __arm__
#error Can I haz ARM?
#endif

typedef long			__darwin_intptr_t;
typedef unsigned int		__darwin_natural_t;

typedef int			integer_t;

#if defined(__GNUC__) && defined(__SIZE_TYPE__)
typedef __SIZE_TYPE__		__darwin_size_t;	/* sizeof() */
#else
typedef unsigned long		__darwin_size_t;	/* sizeof() */
#endif

/* size_t */
#ifndef	_SIZE_T
#define	_SIZE_T
typedef	__darwin_size_t		size_t;
#endif

/* 7.18.1.1 Exact-width integer types */
#ifndef _INT8_T
#define _INT8_T
typedef signed char           int8_t;
#endif /*_INT8_T */

#ifndef _INT16_T
#define _INT16_T
typedef short                int16_t;
#endif /* _INT16_T */

#ifndef _INT32_T
#define _INT32_T
typedef int                  int32_t;
#endif /* _INT32_T */

#ifndef _INT64_T
#define _INT64_T
typedef long long            int64_t;
#endif /* _INT64_T */

#ifndef _UINT8_T
#define _UINT8_T
typedef unsigned char         uint8_t;
#endif /*_UINT8_T */

#ifndef _UINT16_T
#define _UINT16_T
typedef unsigned short       uint16_t;
#endif /* _UINT16_T */

#ifndef _UINT32_T
#define _UINT32_T
typedef unsigned int         uint32_t;
#endif /* _UINT32_T */

#ifndef _UINT64_T
#define _UINT64_T
typedef unsigned long long   uint64_t;
#endif /* _UINT64_T */

#endif
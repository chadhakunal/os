#ifndef PRINTK_FORMATS_H
#define PRINTK_FORMATS_H

/* Format specifier symbols for printk */

/* Signed integers */
#define FMT_INT     "%d" // Default is 32 bit
#define FMT_INT8    "%hhd" // h divides by 2, thus this is 8 bits -> 32/2/ = 16/2 = 8
#define FMT_INT16   "%hd" // 16 bits
#define FMT_INT32   "%d" 
#define FMT_INT64   "%lld" // 64 bits, lld and ld would be the same

/* Unsigned integers */
#define FMT_UINT    "%u"
#define FMT_UINT8   "%hhu"
#define FMT_UINT16  "%hu"
#define FMT_UINT32  "%u"
#define FMT_UINT64  "%llu"

/* Hexadecimal (unsigned) */
#define FMT_HEX     "%x"
#define FMT_HEX_UPPER "%X"

/* Strings and characters */
#define FMT_STR     "%s"
#define FMT_CHAR    "%c"

/* Addresses and pointers */
#define FMT_PTR     "%p"

/* Escape */
#define FMT_PERCENT "%%"

#endif

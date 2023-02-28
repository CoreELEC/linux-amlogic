// SPDX-License-Identifier: GPL-2.0-only
/*
 * Based on arch/arm/kernel/io.c
 *
 * Copyright (C) 2012 ARM Ltd.
 */

#include <linux/export.h>
#include <linux/types.h>
#if IS_ENABLED(CONFIG_AMLOGIC_BGKI_DEBUG_IOTRACE)
#define SKIP_IO_TRACE
#include <linux/io.h>
#undef SKIP_IO_TRACE
#include <linux/amlogic/debug_ftrace_ramoops.h>
#else
#include <linux/io.h>
#endif

/*
 * Copy data from IO memory space to "real" memory space.
 */
void __memcpy_fromio(void *to, const volatile void __iomem *from, size_t count)
{
#if IS_ENABLED(CONFIG_AMLOGIC_BGKI_DEBUG_IOTRACE)
	pstore_ftrace_io_copy_from((unsigned long)from, (unsigned long)count);
#endif
	while (count && !IS_ALIGNED((unsigned long)from, 8)) {
		*(u8 *)to = __raw_readb(from);
		from++;
		to++;
		count--;
	}

	while (count >= 8) {
		*(u64 *)to = __raw_readq(from);
		from += 8;
		to += 8;
		count -= 8;
	}

	while (count) {
		*(u8 *)to = __raw_readb(from);
		from++;
		to++;
		count--;
	}
#if IS_ENABLED(CONFIG_AMLOGIC_BGKI_DEBUG_IOTRACE)
	pstore_ftrace_io_copy_from_end((unsigned long)from, (unsigned long)count);
#endif
}
EXPORT_SYMBOL(__memcpy_fromio);

/*
 * Copy data from "real" memory space to IO memory space.
 */
void __memcpy_toio(volatile void __iomem *to, const void *from, size_t count)
{
#if IS_ENABLED(CONFIG_AMLOGIC_BGKI_DEBUG_IOTRACE)
	pstore_ftrace_io_copy_to((unsigned long)to, (unsigned long)count);
#endif
	while (count && !IS_ALIGNED((unsigned long)to, 8)) {
		__raw_writeb(*(u8 *)from, to);
		from++;
		to++;
		count--;
	}

	while (count >= 8) {
		__raw_writeq(*(u64 *)from, to);
		from += 8;
		to += 8;
		count -= 8;
	}

	while (count) {
		__raw_writeb(*(u8 *)from, to);
		from++;
		to++;
		count--;
	}
#if IS_ENABLED(CONFIG_AMLOGIC_BGKI_DEBUG_IOTRACE)
	pstore_ftrace_io_copy_to_end((unsigned long)to, (unsigned long)count);
#endif
}
EXPORT_SYMBOL(__memcpy_toio);

/*
 * "memset" on IO memory space.
 */
void __memset_io(volatile void __iomem *dst, int c, size_t count)
{
	u64 qc = (u8)c;

#if IS_ENABLED(CONFIG_AMLOGIC_BGKI_DEBUG_IOTRACE)
	pstore_ftrace_io_memset((unsigned long)dst, (unsigned long)count);
#endif

	qc |= qc << 8;
	qc |= qc << 16;
	qc |= qc << 32;

	while (count && !IS_ALIGNED((unsigned long)dst, 8)) {
		__raw_writeb(c, dst);
		dst++;
		count--;
	}

	while (count >= 8) {
		__raw_writeq(qc, dst);
		dst += 8;
		count -= 8;
	}

	while (count) {
		__raw_writeb(c, dst);
		dst++;
		count--;
	}
#if IS_ENABLED(CONFIG_AMLOGIC_BGKI_DEBUG_IOTRACE)
	pstore_ftrace_io_memset_end((unsigned long)dst, (unsigned long)count);
#endif
}
EXPORT_SYMBOL(__memset_io);

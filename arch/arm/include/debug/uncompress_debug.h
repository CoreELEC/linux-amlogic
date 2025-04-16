/* SPDX-License-Identifier: GPL-2.0 */
#ifdef CONFIG_AMLOGIC_DEBUG_EARLY_UART
extern void putc(int c);
static inline void flush(void) {}
static void putstr_debug(const char *ptr)
{
       char c;

       while ((c = *ptr++) != '\0') {
               if (c == '\n')
                       putc('\r');
               putc(c);
       }

       flush();
}
#endif

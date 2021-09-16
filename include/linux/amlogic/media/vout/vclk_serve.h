/*
 * include/linux/amlogic/media/vout/vclk_serve.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _AML_VCLK_H_
#define _AML_VCLK_H_

#ifdef CONFIG_AMLOGIC_VOUT_CLK_SERVE
unsigned int vclk_ana_reg_read(unsigned int _reg);
void vclk_ana_reg_write(unsigned int _reg, unsigned int _value);
void vclk_ana_reg_setb(unsigned int _reg, unsigned int _value,
		       unsigned int _start, unsigned int _len);
unsigned int vclk_ana_reg_getb(unsigned int _reg,
			       unsigned int _start, unsigned int _len);

unsigned int vclk_clk_reg_read(unsigned int _reg);
void vclk_clk_reg_write(unsigned int _reg, unsigned int _value);
void vclk_clk_reg_setb(unsigned int _reg, unsigned int _value,
		       unsigned int _start, unsigned int _len);
unsigned int vclk_clk_reg_getb(unsigned int _reg,
			       unsigned int _start, unsigned int _len);
#else
static inline unsigned int vclk_ana_reg_read(unsigned int _reg)
{
	return 0;
}

static inline void vclk_ana_reg_write(unsigned int _reg, unsigned int _value)
{
}

static inline void vclk_ana_reg_setb(unsigned int _reg, unsigned int _value,
				     unsigned int _start, unsigned int _len)
{
}

static inline unsigned int vclk_ana_reg_getb(unsigned int _reg,
					     unsigned int _start,
					     unsigned int _len)
{
	return 0;
}

static inline unsigned int vclk_clk_reg_read(unsigned int _reg)
{
	return 0;
}

static inline void vclk_clk_reg_write(unsigned int _reg, unsigned int _value)
{
}

static inline void vclk_clk_reg_setb(unsigned int _reg, unsigned int _value,
				     unsigned int _start, unsigned int _len)
{
}

static inline unsigned int vclk_clk_reg_getb(unsigned int _reg,
					     unsigned int _start,
					     unsigned int _len)
{
	return 0;
}
#endif

#endif

/*
 * AMLOGIC DVB driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 */

#define ENABLE_DEMUX_DRIVER

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/fcntl.h>
#include <asm/irq.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/amlogic/amports/amstream.h>
#ifdef ARC_700
#include <asm/arch/am_regs.h>
#else
#include "wetek_stb_define.h"
#include "wetek_stb_regs_define.h"
#endif

/*#include <mach/mod_gate.h>*/
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/amlogic/amdsc.h>
#include <linux/string.h>
#include <linux/pinctrl/consumer.h>

#include <linux/reset.h>

#include "linux/amlogic/cpu_version.h"

#include "wetek_dvb.h"
#include "wetek_dvb_reg.h"
#include "nimdetect.h"

#define pr_dbg(fmt, args...)\
	do {\
		if (debug_dvb)\
			printk("DVB: %s: " fmt, __func__, ## args);\
	} while (0)
#define pr_error(fmt, args...) printk("DVB: %s: " fmt, __func__, ## args)
#define pr_inf(fmt, args...)   printk("DVB: %s: " fmt, __func__, ## args)

MODULE_PARM_DESC(debug_dvb, "\n\t\t Enable dvb debug information");
static int debug_dvb;
module_param(debug_dvb, int, 0644);

#define CARD_NAME "wetek-dvb"

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

static struct wetek_dvb wetek_dvb_device;
static struct reset_control *wetek_dvb_demux_reset_ctl;
static struct reset_control *wetek_dvb_afifo_reset_ctl;
static struct reset_control *wetek_dvb_ahbarb0_reset_ctl;
static struct reset_control *wetek_dvb_uparsertop_reset_ctl;

static int wetek_tsdemux_reset(void);
static int wetek_tsdemux_set_reset_flag(void);
static int wetek_tsdemux_request_irq(irq_handler_t handler, void *data);
static int wetek_tsdemux_free_irq(void);
static int wetek_tsdemux_set_vid(int vpid);
static int wetek_tsdemux_set_aid(int apid);
static int wetek_tsdemux_set_sid(int spid);
static int wetek_tsdemux_set_pcrid(int pcrpid);
static int wetek_tsdemux_set_skipbyte(int skipbyte);
static int wetek_tsdemux_set_demux(int id);

static struct tsdemux_ops wetek_tsdemux_ops = {
.reset          = wetek_tsdemux_reset,
.set_reset_flag = wetek_tsdemux_set_reset_flag,
.request_irq    = wetek_tsdemux_request_irq,
.free_irq       = wetek_tsdemux_free_irq,
.set_vid        = wetek_tsdemux_set_vid,
.set_aid        = wetek_tsdemux_set_aid,
.set_sid        = wetek_tsdemux_set_sid,
.set_pcrid      = wetek_tsdemux_set_pcrid,
.set_skipbyte   = wetek_tsdemux_set_skipbyte,
.set_demux      = wetek_tsdemux_set_demux
};
static struct class   wetek_stb_class;

static void wetek_dvb_dmx_release(struct wetek_dvb *advb, struct wetek_dmx *dmx)
{
	int i;

	dvb_net_release(&dmx->dvb_net);
	wetek_dmx_hw_deinit(dmx);
	dmx->demux.dmx.close(&dmx->demux.dmx);
	dmx->demux.dmx.remove_frontend(&dmx->demux.dmx, &dmx->mem_fe);

	for (i=0; i<DMX_DEV_COUNT; i++) {
		dmx->demux.dmx.remove_frontend(&dmx->demux.dmx, &dmx->hw_fe[i]);
	}

	dvb_dmxdev_release(&dmx->dmxdev);
	dvb_dmx_release(&dmx->demux);
}

static int wetek_dvb_dmx_init(struct wetek_dvb *advb, struct wetek_dmx *dmx, int id)
{
	int i, ret;
#ifndef CONFIG_OF
	struct resource *res;
	char buf[32];
#endif
	switch(id){
		case 0:
			dmx->dmx_irq = INT_DEMUX;
			break;
		case 1:
			dmx->dmx_irq = INT_DEMUX_1;
			break;
		case 2:
			dmx->dmx_irq = INT_DEMUX_2;
			break;
	}

#ifndef CONFIG_OF
	snprintf(buf, sizeof(buf), "demux%d_irq", id);
	res = platform_get_resource_byname(advb->pdev, IORESOURCE_IRQ, buf);
	if (res) {
		dmx->dmx_irq = res->start;
	}
#endif
	pr_dbg("demux%d_irq: %d\n", id, dmx->dmx_irq);

	dmx->source  = 0;
	dmx->dump_ts_select = 0;
	dmx->dvr_irq = -1;

	dmx->demux.dmx.capabilities 	= (DMX_TS_FILTERING | DMX_SECTION_FILTERING | DMX_MEMORY_BASED_FILTERING);
	dmx->demux.filternum 		= dmx->demux.feednum = FILTER_COUNT;
	dmx->demux.priv 		= advb;
	dmx->demux.start_feed 		= wetek_dmx_hw_start_feed;
	dmx->demux.stop_feed 		= wetek_dmx_hw_stop_feed;
	dmx->demux.write_to_decoder 	= NULL;

	if ((ret = dvb_dmx_init(&dmx->demux)) < 0) {
		pr_error("dvb_dmx failed: error %d\n",ret);
		goto error_dmx_init;
	}

	dmx->dmxdev.filternum = dmx->demux.feednum;
	dmx->dmxdev.demux = &dmx->demux.dmx;
	dmx->dmxdev.capabilities = 0;
	if ((ret = dvb_dmxdev_init(&dmx->dmxdev, &advb->dvb_adapter)) < 0) {
		pr_error("dvb_dmxdev_init failed: error %d\n",ret);
		goto error_dmxdev_init;
	}

	for (i=0; i<DMX_DEV_COUNT; i++) {
		int source = i+DMX_FRONTEND_0;
		dmx->hw_fe[i].source = source;
	pr_dbg("demux%d: source:%d\n", i, source);

		if ((ret = dmx->demux.dmx.add_frontend(&dmx->demux.dmx, &dmx->hw_fe[i])) < 0) {
			pr_error("adding hw_frontend to dmx failed: error %d",ret);
			dmx->hw_fe[i].source = 0;
			goto error_add_hw_fe;
		}
	}

	dmx->mem_fe.source = DMX_MEMORY_FE;
	if ((ret = dmx->demux.dmx.add_frontend(&dmx->demux.dmx, &dmx->mem_fe)) < 0) {
		pr_error("adding mem_frontend to dmx failed: error %d",ret);
		goto error_add_mem_fe;
	}

	if ((ret = dmx->demux.dmx.connect_frontend(&dmx->demux.dmx, &dmx->hw_fe[1])) < 0) {
		pr_error("connect frontend failed: error %d",ret);
		goto error_connect_fe;
	}

	dmx->id = id;
	dmx->aud_chan = -1;
	dmx->vid_chan = -1;
	dmx->sub_chan = -1;
	dmx->pcr_chan = -1;
	dmx->smallsec.bufsize   = SS_BUFSIZE_DEF;
	dmx->smallsec.enable    = 0;
	dmx->smallsec.dmx       = dmx;
	dmx->timeout.dmx        =  dmx;
	dmx->timeout.enable     = 1;
	dmx->timeout.timeout    = DTO_TIMEOUT_DEF;
	dmx->timeout.ch_disable = DTO_CHDIS_VAS;
	dmx->timeout.match      = 1;
	dmx->timeout.trigger    = 0;

	if ((ret = wetek_dmx_hw_init(dmx)) <0) {
		pr_error("demux hw init error %d", ret);
		dmx->id = -1;
		goto error_dmx_hw_init;
	}
pr_dbg("demux%d \n", id);

	dvb_net_init(&advb->dvb_adapter, &dmx->dvb_net, &dmx->demux.dmx);

	return 0;
error_dmx_hw_init:
error_connect_fe:
	dmx->demux.dmx.remove_frontend(&dmx->demux.dmx, &dmx->mem_fe);
error_add_mem_fe:
error_add_hw_fe:
	for (i=0; i<DMX_DEV_COUNT; i++) {
		if (dmx->hw_fe[i].source)
			dmx->demux.dmx.remove_frontend(&dmx->demux.dmx, &dmx->hw_fe[i]);
	}
	dvb_dmxdev_release(&dmx->dmxdev);
error_dmxdev_init:
	dvb_dmx_release(&dmx->demux);
error_dmx_init:
	return ret;
}
struct wetek_dvb* wetek_get_dvb_device(void)
{
	return &wetek_dvb_device;
}

EXPORT_SYMBOL(wetek_get_dvb_device);


static int wetek_dvb_asyncfifo_init(struct wetek_dvb *advb, struct wetek_asyncfifo *asyncfifo, int id)
{
#ifndef CONFIG_OF
	struct resource *res;
	char buf[32];
#endif

	if(id == 0)
		asyncfifo->asyncfifo_irq = INT_ASYNC_FIFO_FLUSH;
	else
		asyncfifo->asyncfifo_irq = INT_ASYNC_FIFO2_FLUSH;

#ifndef CONFIG_OF
	snprintf(buf, sizeof(buf), "dvr%d_irq", id);
	res = platform_get_resource_byname(advb->pdev, IORESOURCE_IRQ, buf);
	if (res) {
		asyncfifo->asyncfifo_irq = res->start;
	}
#endif

	asyncfifo->dvb = advb;
	asyncfifo->id = id;
	asyncfifo->init = 0;
	asyncfifo->flush_size = 256*1024;

	return wetek_asyncfifo_hw_init(asyncfifo);
}


static void wetek_dvb_asyncfifo_release(struct wetek_dvb *advb, struct wetek_asyncfifo *asyncfifo)
{
	wetek_asyncfifo_hw_deinit(asyncfifo);
}


/*Show the STB input source*/
static ssize_t stb_show_source(struct class *class, struct class_attribute *attr,char *buf)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	ssize_t ret = 0;
	char *src;

	switch(dvb->stb_source) {
		case AM_TS_SRC_TS0:
		case AM_TS_SRC_S_TS0:
			src = "ts0";
		break;
		case AM_TS_SRC_TS1:
		case AM_TS_SRC_S_TS1:
			src = "ts1";
		break;
		case AM_TS_SRC_TS2:
		case AM_TS_SRC_S_TS2:
			src = "ts2";
		break;
		case AM_TS_SRC_HIU:
			src = "hiu";
		break;
		case AM_TS_SRC_DMX0:
			src = "dmx0";
		break;
		case AM_TS_SRC_DMX1:
			src = "dmx1";
		break;
		case AM_TS_SRC_DMX2:
			src = "dmx2";
		break;
		default:
			src = "disable";
		break;
	}

	ret = sprintf(buf, "%s\n", src);
	return ret;
}



/*Set the STB input source*/
static ssize_t stb_store_source(struct class *class,struct class_attribute *attr, const char *buf, size_t size)
{
    dmx_source_t src = -1;
    if(!strncmp("ts0", buf, 3)) {
    	src = DMX_SOURCE_FRONT0;
    } else if(!strncmp("ts1", buf, 3)) {
    	src = DMX_SOURCE_FRONT1;
    } else if(!strncmp("ts2", buf, 3)) {
    	src = DMX_SOURCE_FRONT2;
    } else if(!strncmp("hiu", buf, 3)) {
    	src = DMX_SOURCE_DVR0;
    } else if(!strncmp("dmx0", buf, 4)) {
        src = DMX_SOURCE_FRONT0+100;
    } else if(!strncmp("dmx1", buf, 4)) {
        src = DMX_SOURCE_FRONT1+100;
    } else if(!strncmp("dmx2", buf, 4)) {
        src = DMX_SOURCE_FRONT2+100;
    }
    if(src!=-1) {
    	wetek_stb_hw_set_source(&wetek_dvb_device, src);
    }
    return size;
}

/*Show the descrambler's input source*/
static ssize_t dsc_show_source(struct class *class,struct class_attribute *attr, char *buf)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	ssize_t ret = 0;
	char *src;

	switch(dvb->dsc_source+7) {
		case AM_TS_SRC_DMX0:
			src = "dmx0";
		break;
		case AM_TS_SRC_DMX1:
			src = "dmx1";
		break;
		case AM_TS_SRC_DMX2:
			src = "dmx2";
		break;
		default:
			src = "bypass";
		break;
	}

	ret = sprintf(buf, "%s\n", src);
	return ret;
}



/*Set the descrambler's input source*/
static ssize_t dsc_store_source(struct class *class, struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
	dmx_source_t src = -1;

	if(!strncmp("dmx0", buf, 4)) {
		src = DMX_SOURCE_FRONT0; 	//	src = DMX_SOURCE_FRONT0+100;
	} else if(!strncmp("dmx1", buf, 4)) {
		src = DMX_SOURCE_FRONT1;	//	src = DMX_SOURCE_FRONT1+100;
	} else if(!strncmp("dmx2", buf, 4)) {
		src = DMX_SOURCE_FRONT2;	//	src = DMX_SOURCE_FRONT2+100;
	}
	wetek_dsc_hw_set_source(&wetek_dvb_device, src);

	return size;
}


/*Show free descramblers count*/
static ssize_t dsc_show_free_dscs(struct class *class,
				  struct class_attribute *attr,
				  char *buf)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	int fid, count;
	ssize_t ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);
	count = 0;
	for(fid = 0; fid < DSC_COUNT; fid++){
		if(!dvb->dsc[fid].used)
			count++;
	}
	spin_unlock_irqrestore(&dvb->slock, flags);

	ret = sprintf(buf, "%d\n", count);
	return ret;
}


/*Show the TS output source*/
static ssize_t tso_show_source(struct class *class, struct class_attribute *attr,char *buf)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	ssize_t ret = 0;
	char *src;

	switch(dvb->tso_source) {
		case AM_TS_SRC_TS0:
		case AM_TS_SRC_S_TS0:
			src = "ts0";
		break;
		case AM_TS_SRC_TS1:
		case AM_TS_SRC_S_TS1:
			src = "ts1";
		break;
		case AM_TS_SRC_TS2:
		case AM_TS_SRC_S_TS2:
			src = "ts2";
		break;
		case AM_TS_SRC_HIU:
			src = "hiu";
		break;
		case AM_TS_SRC_DMX0:
			src = "dmx0";
		break;
		case AM_TS_SRC_DMX1:
			src = "dmx1";
		break;
		case AM_TS_SRC_DMX2:
			src = "dmx2";
		break;
		default:
			src = "default";
		break;
	}

	ret = sprintf(buf, "%s\n", src);
	return ret;
}



/*Set the TS output source*/
static ssize_t tso_store_source(struct class *class,struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
    dmx_source_t src = -1;

    if(!strncmp("ts0", buf, 3)) {
    	src = DMX_SOURCE_FRONT0;
    } else if(!strncmp("ts1", buf, 3)) {
    	src = DMX_SOURCE_FRONT1;
    } else if(!strncmp("ts2", buf, 3)) {
    	src = DMX_SOURCE_FRONT2;
    } else if(!strncmp("hiu", buf, 3)) {
    	src = DMX_SOURCE_DVR0;
    } else if(!strncmp("dmx0", buf, 4)) {
        src = DMX_SOURCE_FRONT0+100;
    } else if(!strncmp("dmx1", buf, 4)) {
        src = DMX_SOURCE_FRONT1+100;
    } else if(!strncmp("dmx2", buf, 4)) {
        src = DMX_SOURCE_FRONT2+100;
    }

	wetek_tso_hw_set_source(&wetek_dvb_device, src);

    return size;
}

/*Show PCR*/
#define DEMUX_PCR_FUNC_DECL(i)  \
static ssize_t demux##i##_show_pcr(struct class *class,  struct class_attribute *attr,char *buf)\
{\
	int f = 0;\
	if(i == 0)\
		f = READ_MPEG_REG(PCR_DEMUX);\
	else if(i==1)\
		f = READ_MPEG_REG(PCR_DEMUX_2);\
	else if(i==2)\
		f = READ_MPEG_REG(PCR_DEMUX_3);\
	return sprintf(buf, "%08x\n", f);\
}

/*Show the STB input source*/
#define DEMUX_SOURCE_FUNC_DECL(i)  \
static ssize_t demux##i##_show_source(struct class *class,  struct class_attribute *attr,char *buf)\
{\
	struct wetek_dvb *dvb = &wetek_dvb_device;\
	struct wetek_dmx *dmx = &dvb->dmx[i];\
	ssize_t ret = 0;\
	char *src;\
	switch(dmx->source) {\
		case AM_TS_SRC_TS0:\
		case AM_TS_SRC_S_TS0:\
			src = "ts0";\
		break;\
		case AM_TS_SRC_TS1:\
		case AM_TS_SRC_S_TS1:\
			src = "ts1";\
		break;\
		case AM_TS_SRC_TS2:\
		case AM_TS_SRC_S_TS2:\
			src = "ts2";\
		break;\
		case AM_TS_SRC_HIU:\
			src = "hiu";\
		break;\
		default:\
			src = "";\
		break;\
	}\
	ret = sprintf(buf, "%s\n", src);\
	return ret;\
}\
static ssize_t demux##i##_store_source(struct class *class,  struct class_attribute *attr,const char *buf, size_t size)\
{\
    dmx_source_t src = -1;\
	if(!strncmp("ts0", buf, 3)) {\
    	src = DMX_SOURCE_FRONT0;\
    } else if(!strncmp("ts1", buf, 3)) {\
    	src = DMX_SOURCE_FRONT1;\
    } else if(!strncmp("ts2", buf, 3)) {\
    	src = DMX_SOURCE_FRONT2;\
    } else if(!strncmp("hiu", buf, 3)) {\
    	src = DMX_SOURCE_DVR0;\
    }\
    if(src!=-1) {\
    	wetek_dmx_hw_set_source(wetek_dvb_device.dmx[i].dmxdev.demux, src);\
    }\
    return size;\
}

/*Show free filters count*/
#define DEMUX_FREE_FILTERS_FUNC_DECL(i)  \
static ssize_t demux##i##_show_free_filters(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct wetek_dvb *dvb = &wetek_dvb_device;\
	struct dvb_demux *dmx = &dvb->dmx[i].demux;\
	int fid, count;\
	ssize_t ret = 0;\
	if (mutex_lock_interruptible(&dmx->mutex)) \
		return -ERESTARTSYS; \
	count = 0;\
	for (fid = 0; fid < dmx->filternum; fid++) {\
		if (!dmx->filter[fid].state != DMX_STATE_FREE)\
			count++;\
	} \
	mutex_unlock(&dmx->mutex);\
	ret = sprintf(buf, "%d\n", count);\
	return ret;\
}

/*Show filter users count*/
#define DEMUX_FILTER_USERS_FUNC_DECL(i)  \
static ssize_t demux##i##_show_filter_users(struct class *class,  struct class_attribute *attr,char *buf)\
{\
	struct wetek_dvb *dvb = &wetek_dvb_device;\
	struct wetek_dmx *dmx = &dvb->dmx[i];\
	int dmxdevfid, count;\
	ssize_t ret = 0;\
	unsigned long flags;\
	spin_lock_irqsave(&dvb->slock, flags);\
	count = 0;\
	for (dmxdevfid = 0; dmxdevfid < dmx->dmxdev.filternum; dmxdevfid++) {\
		if (dmx->dmxdev.filter[dmxdevfid].state >= DMXDEV_STATE_ALLOCATED)\
			count++;\
	}\
	if (count > dmx->demux_filter_user) {\
		count = dmx->demux_filter_user;\
	} else{\
		dmx->demux_filter_user = count;\
	}\
	spin_unlock_irqrestore(&dvb->slock, flags);\
	ret = sprintf(buf, "%d\n", count);\
	return ret;\
}\
static ssize_t demux##i##_store_filter_used(struct class *class,  struct class_attribute *attr,const char *buf, size_t size)\
{\
	struct wetek_dvb *dvb = &wetek_dvb_device;\
	struct wetek_dmx *dmx = &dvb->dmx[i];\
	unsigned long filter_used;\
	unsigned long flags;/*char *endp;*/\
	/*filter_used = simple_strtol(buf, &endp, 0);*/\
	int ret = kstrtol(buf, 0, &filter_used);\
	spin_lock_irqsave(&dvb->slock, flags);\
	if (ret == 0 && filter_used) {\
		if(dmx->demux_filter_user < FILTER_COUNT)\
			dmx->demux_filter_user++;\
	}else{\
		if(dmx->demux_filter_user > 0)\
			dmx->demux_filter_user--;\
	}\
	spin_unlock_irqrestore(&dvb->slock, flags);\
	return size;\
}


/*Show ts header*/
#define DEMUX_TS_HEADER_FUNC_DECL(i)  \
static ssize_t demux##i##_show_ts_header(struct class *class,  struct class_attribute *attr,char *buf)\
{\
	int hdr = 0;\
	if(i == 0)\
		hdr = READ_MPEG_REG(TS_HEAD_1);\
	else if(i==1)\
		hdr = READ_MPEG_REG(TS_HEAD_1_2);\
	else if(i==2)\
		hdr = READ_MPEG_REG(TS_HEAD_1_3);\
	return sprintf(buf, "%08x\n", hdr);\
}

/*Show channel activity*/
#define DEMUX_CHANNEL_ACTIVITY_FUNC_DECL(i)  \
static ssize_t demux##i##_show_channel_activity(struct class *class,  struct class_attribute *attr,char *buf)\
{\
	int f = 0;\
	if(i == 0)\
		f = READ_MPEG_REG(DEMUX_CHANNEL_ACTIVITY);\
	else if(i==1)\
		f = READ_MPEG_REG(DEMUX_CHANNEL_ACTIVITY_2);\
	else if(i==2)\
		f = READ_MPEG_REG(DEMUX_CHANNEL_ACTIVITY_3);\
	return sprintf(buf, "%08x\n", f);\
}

/*DVR record mode*/
#define DVR_MODE_FUNC_DECL(i)  \
static ssize_t dvr##i##_show_mode(struct class *class,  struct class_attribute *attr,char *buf)\
{\
	struct wetek_dvb *dvb = &wetek_dvb_device;\
	struct wetek_dmx *dmx = &dvb->dmx[i];\
	ssize_t ret = 0;\
	char *mode;\
	if(dmx->dump_ts_select) {\
		mode = "ts";\
	}else{\
		mode = "pid";\
	}\
	ret = sprintf(buf, "%s\n", mode);\
	return ret;\
}\
static ssize_t dvr##i##_store_mode(struct class *class,  struct class_attribute *attr,const char *buf, size_t size)\
{\
    struct wetek_dvb *dvb = &wetek_dvb_device;\
    struct wetek_dmx *dmx = &dvb->dmx[i];\
    int dump_ts_select = -1;\
    \
    if(!strncmp("pid", buf, 3) && dmx->dump_ts_select) {\
    	dump_ts_select = 0;\
    } else if(!strncmp("ts", buf, 2) && !dmx->dump_ts_select) {\
    	dump_ts_select = 1;\
    }\
    if(dump_ts_select!=-1) {\
    	wetek_dmx_hw_set_dump_ts_select(wetek_dvb_device.dmx[i].dmxdev.demux, dump_ts_select);\
    }\
    return size;\
}

#if DMX_DEV_COUNT>0
	DEMUX_PCR_FUNC_DECL(0)
	DEMUX_SOURCE_FUNC_DECL(0)
	DEMUX_FREE_FILTERS_FUNC_DECL(0)
	DEMUX_FILTER_USERS_FUNC_DECL(0)
	DVR_MODE_FUNC_DECL(0)
	DEMUX_TS_HEADER_FUNC_DECL(0)
	DEMUX_CHANNEL_ACTIVITY_FUNC_DECL(0)
#endif
#if DMX_DEV_COUNT>1
	DEMUX_PCR_FUNC_DECL(1)
	DEMUX_SOURCE_FUNC_DECL(1)
	DEMUX_FREE_FILTERS_FUNC_DECL(1)
	DEMUX_FILTER_USERS_FUNC_DECL(1)
	DVR_MODE_FUNC_DECL(1)
	DEMUX_TS_HEADER_FUNC_DECL(1)
	DEMUX_CHANNEL_ACTIVITY_FUNC_DECL(1)
#endif
#if DMX_DEV_COUNT>2
	DEMUX_PCR_FUNC_DECL(2)
	DEMUX_SOURCE_FUNC_DECL(2)
	DEMUX_FREE_FILTERS_FUNC_DECL(2)
	DEMUX_FILTER_USERS_FUNC_DECL(2)
	DVR_MODE_FUNC_DECL(2)
	DEMUX_TS_HEADER_FUNC_DECL(2)
	DEMUX_CHANNEL_ACTIVITY_FUNC_DECL(2)
#endif

/*Show the async fifo source*/
#define ASYNCFIFO_SOURCE_FUNC_DECL(i)  \
static ssize_t asyncfifo##i##_show_source(struct class *class,  struct class_attribute *attr,char *buf)\
{\
	struct wetek_dvb *dvb = &wetek_dvb_device;\
	struct wetek_asyncfifo *afifo = &dvb->asyncfifo[i];\
	ssize_t ret = 0;\
	char *src;\
	switch(afifo->source) {\
		case AM_DMX_0:\
			src = "dmx0";\
		break;\
		case AM_DMX_1:\
			src = "dmx1";\
		break;\
		case AM_DMX_2:\
			src = "dmx2";\
		break;\
		default:\
			src = "";\
		break;\
	}\
	ret = sprintf(buf, "%s\n", src);\
	return ret;\
}\
static ssize_t asyncfifo##i##_store_source(struct class *class,  struct class_attribute *attr,const char *buf, size_t size)\
{\
    enum wetek_dmx_id_t src = -1;\
	if(!strncmp("dmx0", buf, 4)) {\
    	src = AM_DMX_0;\
    } else if(!strncmp("dmx1", buf, 4)) {\
    	src = AM_DMX_1;\
    } else if(!strncmp("dmx2", buf, 4)) {\
    	src = AM_DMX_2;\
    }\
    if(src!=-1) {\
    	wetek_asyncfifo_hw_set_source(&wetek_dvb_device.asyncfifo[i], src);\
    }\
    return size;\
}

#if ASYNCFIFO_COUNT>0
	ASYNCFIFO_SOURCE_FUNC_DECL(0)
#endif
#if ASYNCFIFO_COUNT>1
	ASYNCFIFO_SOURCE_FUNC_DECL(1)
#endif

/*Show the async fifo flush size*/
#define ASYNCFIFO_FLUSHSIZE_FUNC_DECL(i)  \
static ssize_t asyncfifo##i##_show_flush_size(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct wetek_dvb *dvb = &wetek_dvb_device;\
	struct wetek_asyncfifo *afifo = &dvb->asyncfifo[i];\
	ssize_t ret = 0;\
	ret = sprintf(buf, "%d\n", afifo->flush_size);\
	return ret;\
} \
static ssize_t asyncfifo##i##_store_flush_size(struct class *class,  \
					struct class_attribute *attr, \
					const char *buf, size_t size)\
{\
	struct wetek_dvb *dvb = &wetek_dvb_device;\
	struct wetek_asyncfifo *afifo = &dvb->asyncfifo[i];\
	/*int fsize = simple_strtol(buf, NULL, 10);*/\
	int fsize = 0;\
	long value;\
	int ret = kstrtol(buf, 0, &value);\
	if (ret == 0)\
		fsize = value;\
	if (fsize != afifo->flush_size) {\
		afifo->flush_size = fsize;\
	wetek_asyncfifo_hw_reset(&wetek_dvb_device.asyncfifo[i]);\
	} \
	return size;\
}

#if ASYNCFIFO_COUNT > 0
ASYNCFIFO_FLUSHSIZE_FUNC_DECL(0)
#endif

#if ASYNCFIFO_COUNT > 1
	ASYNCFIFO_FLUSHSIZE_FUNC_DECL(1)
#endif
/*Reset the Demux*/
static ssize_t demux_do_reset(struct class *class,struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
	if(!strncmp("1", buf, 1)) {
		struct wetek_dvb *dvb = &wetek_dvb_device;
		unsigned long flags;

		spin_lock_irqsave(&dvb->slock, flags);
		pr_dbg("Reset demux, call dmx_reset_hw\n");
		dmx_reset_hw_ex(dvb, 0);
		spin_unlock_irqrestore(&dvb->slock, flags);
	}

	return size;
}





/*Show the Video PTS value*/
static ssize_t demux_show_video_pts(struct class *class,  struct class_attribute *attr,char *buf)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	ssize_t ret = 0;

	ret = sprintf(buf, "%u\n", wetek_dmx_get_video_pts(dvb));

	return ret;
}


/*Show the Audio PTS value*/
static ssize_t demux_show_audio_pts(struct class *class,  struct class_attribute *attr,char *buf)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	ssize_t ret = 0;

	ret = sprintf(buf, "%u\n", wetek_dmx_get_audio_pts(dvb));

	return ret;
}



/*Show the First Video PTS value*/
static ssize_t demux_show_first_video_pts(struct class *class,  struct class_attribute *attr,char *buf)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	ssize_t ret = 0;

	ret = sprintf(buf, "%u\n", wetek_dmx_get_first_video_pts(dvb));

	return ret;
}



/*Show the First Audio PTS value*/
static ssize_t demux_show_first_audio_pts(struct class *class,  struct class_attribute *attr,char *buf)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	ssize_t ret = 0;

	ret = sprintf(buf, "%u\n", wetek_dmx_get_first_audio_pts(dvb));

	return ret;
}


static ssize_t stb_show_hw_setting(struct class *class, struct class_attribute *attr, char *buf)
{
	int r, total = 0;
	int i;
	struct wetek_dvb *dvb = &wetek_dvb_device;
	int invert, ctrl;

	for(i=0; i<TS_IN_COUNT; i++){
		struct wetek_ts_input *ts = &dvb->ts[i];

		if(ts->s2p_id != -1){
			invert = dvb->s2p[ts->s2p_id].invert;
		}else{
			invert = 0;
		}

		ctrl = ts->control;

		r = sprintf(buf, "ts%d %s control: 0x%x invert: 0x%x\n", i,
				ts->mode==AM_TS_DISABLE?"disable":(ts->mode==AM_TS_SERIAL?"serial":"parallel"),
				ctrl, invert);
		buf += r;
		total += r;
	}

	return total;
}



static ssize_t stb_store_hw_setting(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
	int id, ctrl, invert, r, mode;
	char mname[32];
	char pname[32];
	unsigned long flags;
	struct wetek_ts_input *ts;
	struct wetek_dvb *dvb = &wetek_dvb_device;

	r = sscanf(buf, "%d %s %x %x", &id, mname, &ctrl, &invert);
	if(r != 4)
		return -EINVAL;

	if(id < 0 || id >= TS_IN_COUNT)
		return -EINVAL;

	if((mname[0] == 's') || (mname[0] == 'S')){
		sprintf(pname, "s_ts%d", id);
		mode = AM_TS_SERIAL;
	}else if((mname[0] == 'p') || (mname[0] == 'P')){
		sprintf(pname, "p_ts%d", id);
		mode = AM_TS_PARALLEL;
	}else{
		mode = AM_TS_DISABLE;
	}

	spin_lock_irqsave(&dvb->slock, flags);

	ts = &dvb->ts[id];

	if((mode == AM_TS_SERIAL) && (ts->mode != AM_TS_SERIAL)){
		int i;
		int scnt = 0;

		for(i = 0; i < TS_IN_COUNT; i++){
			if(dvb->ts[i].s2p_id != -1){
				scnt++;
			}
		}

		if(scnt >= S2P_COUNT){
			pr_error("no free s2p\n");
		}else{
			ts->s2p_id = scnt;
		}
	}

	if((mode != AM_TS_SERIAL) || (ts->s2p_id != -1)){
		if(ts->pinctrl){
			devm_pinctrl_put(ts->pinctrl);
			ts->pinctrl = NULL;
		}

		ts->pinctrl  = devm_pinctrl_get_select(&dvb->pdev->dev, pname);
		ts->mode     = mode;
		ts->control  = ctrl;

		if(mode == AM_TS_SERIAL){
			dvb->s2p[ts->s2p_id].invert = invert;
		}else{
			ts->s2p_id = -1;
		}
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return count;
}

/*Get the STB source demux*/
static struct wetek_dmx* get_stb_dmx(void)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	struct wetek_dmx *dmx = NULL;
	int i;

	switch(dvb->stb_source){
		case AM_TS_SRC_DMX0:
			dmx = &dvb->dmx[0];
		break;
		case AM_TS_SRC_DMX1:
			dmx = &dvb->dmx[1];
		break;
		case AM_TS_SRC_DMX2:
			dmx = &dvb->dmx[2];
		break;
		default:
			for(i=0; i<DMX_DEV_COUNT; i++) {
				dmx = &dvb->dmx[i];
				if(dmx->source==dvb->stb_source) {
					return dmx;
				}
			}
		break;
	}

	return dmx;
}

#define DEMUX_RESET_FUNC_DECL(i)  \
static ssize_t demux##i##_reset_store(struct class *class,  struct class_attribute *attr,const char *buf, size_t size)\
{\
	if(!strncmp("1", buf, 1)) { \
		struct wetek_dvb *dvb = &wetek_dvb_device; \
		pr_info("Reset demux["#i"], call dmx_reset_dmx_hw\n"); \
		dmx_reset_dmx_id_hw_ex(dvb, i, 0); \
	} \
	return size; \
}
#if DMX_DEV_COUNT>0
	DEMUX_RESET_FUNC_DECL(0)
#endif
#if DMX_DEV_COUNT>1
	DEMUX_RESET_FUNC_DECL(1)
#endif
#if DMX_DEV_COUNT>2
	DEMUX_RESET_FUNC_DECL(2)
#endif

static struct class_attribute wetek_stb_class_attrs[] = {
	__ATTR(hw_setting, S_IRUGO|S_IWUSR, stb_show_hw_setting, stb_store_hw_setting),
	__ATTR(source,  S_IRUGO | S_IWUSR | S_IWGRP, stb_show_source, stb_store_source),
	__ATTR(dsc_source,  S_IRUGO | S_IWUSR, dsc_show_source, dsc_store_source),
	__ATTR(tso_source,  S_IRUGO | S_IWUSR, tso_show_source, tso_store_source),
#define DEMUX_SOURCE_ATTR_PCR(i)\
		__ATTR(demux##i##_pcr,  S_IRUGO | S_IWUSR, demux##i##_show_pcr, NULL)
#define DEMUX_SOURCE_ATTR_DECL(i)\
		__ATTR(demux##i##_source,  S_IRUGO | S_IWUSR | S_IWGRP, demux##i##_show_source, demux##i##_store_source)
#define DEMUX_FREE_FILTERS_ATTR_DECL(i)\
		__ATTR(demux##i##_free_filters,  S_IRUGO | S_IWUSR, demux##i##_show_free_filters, NULL)
#define DEMUX_FILTER_USERS_ATTR_DECL(i)\
		__ATTR(demux##i##_filter_users,  S_IRUGO | S_IWUSR, demux##i##_show_filter_users, demux##i##_store_filter_used)
#define DVR_MODE_ATTR_DECL(i)\
		__ATTR(dvr##i##_mode,  S_IRUGO | S_IWUSR, dvr##i##_show_mode, dvr##i##_store_mode)
#define DEMUX_TS_HEADER_ATTR_DECL(i)\
		__ATTR(demux##i##_ts_header,  S_IRUGO | S_IWUSR, demux##i##_show_ts_header, NULL)
#define DEMUX_CHANNEL_ACTIVITY_ATTR_DECL(i)\
		__ATTR(demux##i##_channel_activity,  S_IRUGO | S_IWUSR, demux##i##_show_channel_activity, NULL)
#define DMX_RESET_ATTR_DECL(i)\
		__ATTR(demux##i##_reset,  S_IRUGO | S_IWUSR, NULL, demux##i##_reset_store)
#if DMX_DEV_COUNT>0
	DEMUX_SOURCE_ATTR_PCR(0),
	DEMUX_SOURCE_ATTR_DECL(0),
	DEMUX_FREE_FILTERS_ATTR_DECL(0),
	DEMUX_FILTER_USERS_ATTR_DECL(0),
	DVR_MODE_ATTR_DECL(0),
	DEMUX_TS_HEADER_ATTR_DECL(0),
	DEMUX_CHANNEL_ACTIVITY_ATTR_DECL(0),
	DMX_RESET_ATTR_DECL(0),
#endif
#if DMX_DEV_COUNT>1
	DEMUX_SOURCE_ATTR_PCR(1),
	DEMUX_SOURCE_ATTR_DECL(1),
	DEMUX_FREE_FILTERS_ATTR_DECL(1),
	DEMUX_FILTER_USERS_ATTR_DECL(1),
	DVR_MODE_ATTR_DECL(1),
	DEMUX_TS_HEADER_ATTR_DECL(1),
	DEMUX_CHANNEL_ACTIVITY_ATTR_DECL(1),
	DMX_RESET_ATTR_DECL(1),
#endif
#if DMX_DEV_COUNT>2
	DEMUX_SOURCE_ATTR_PCR(2),
	DEMUX_SOURCE_ATTR_DECL(2),
	DEMUX_FREE_FILTERS_ATTR_DECL(2),
	DEMUX_FILTER_USERS_ATTR_DECL(2),
	DVR_MODE_ATTR_DECL(2),
	DEMUX_TS_HEADER_ATTR_DECL(2),
	DEMUX_CHANNEL_ACTIVITY_ATTR_DECL(2),
	DMX_RESET_ATTR_DECL(2),
#endif
#define ASYNCFIFO_SOURCE_ATTR_DECL(i)\
		__ATTR(asyncfifo##i##_source,  S_IRUGO | S_IWUSR | S_IWGRP, asyncfifo##i##_show_source, asyncfifo##i##_store_source)
#define ASYNCFIFO_FLUSHSIZE_ATTR_DECL(i)\
		__ATTR(asyncfifo##i##_flush_size,  S_IRUGO | S_IWUSR | S_IWGRP, asyncfifo##i##_show_flush_size, asyncfifo##i##_store_flush_size)
#if ASYNCFIFO_COUNT>0
	ASYNCFIFO_SOURCE_ATTR_DECL(0),
	ASYNCFIFO_FLUSHSIZE_ATTR_DECL(0),
#endif
#if ASYNCFIFO_COUNT>1
	ASYNCFIFO_SOURCE_ATTR_DECL(1),
	ASYNCFIFO_FLUSHSIZE_ATTR_DECL(1),
#endif
	__ATTR(demux_reset,  S_IRUGO | S_IWUSR, NULL, demux_do_reset),
	__ATTR(video_pts,  S_IRUGO | S_IWUSR | S_IWGRP, demux_show_video_pts, NULL),
	__ATTR(audio_pts,  S_IRUGO | S_IWUSR | S_IWGRP, demux_show_audio_pts, NULL),
	__ATTR(first_video_pts,  S_IRUGO | S_IWUSR, demux_show_first_video_pts, NULL),
	__ATTR(first_audio_pts,  S_IRUGO | S_IWUSR, demux_show_first_audio_pts, NULL),
	__ATTR(free_dscs,  S_IRUGO | S_IWUSR, dsc_show_free_dscs, NULL),
	__ATTR_NULL
};

static struct class wetek_stb_class = {
	.name = "stb",
	.class_attrs = wetek_stb_class_attrs,
};

extern int wetek_regist_dmx_class(void);
extern int wetek_unregist_dmx_class(void);

int __init wetek_dvb_init(void)
{
	struct wetek_nims p;
	struct platform_device *pdev;
	struct wetek_dvb *advb;
	struct devio_wetek_platform_data *pd_dvb;
	struct dvb_frontend_ops *ops;
	int i, ret = 0;
	get_nims_infos(&p);
	pdev = p.pdev;

	advb = &wetek_dvb_device;
	memset(advb, 0, sizeof(wetek_dvb_device));

	spin_lock_init(&advb->slock);

	advb->dev = &pdev->dev;
	advb->pdev = pdev;
	advb->stb_source = -1;
	advb->dsc_source = AM_TS_SRC_S_TS0;
	advb->tso_source = -1;

	for (i = 0; i < DMX_DEV_COUNT; i++) {
		advb->dmx[i].dmx_irq = -1;
		advb->dmx[i].dvr_irq = -1;
	}
#ifdef CONFIG_OF
	for (i=0; i<TS_IN_COUNT; i++){
		advb->ts[i].mode   = AM_TS_DISABLE;
		advb->ts[i].s2p_id = -1;
	}
	advb->ts[0].mode    = AM_TS_PARALLEL;
	advb->ts[0].pinctrl = p.card_pinctrl;
#endif
	pd_dvb = (struct devio_wetek_platform_data *)advb->dev->platform_data;

	ret =
	    dvb_register_adapter(&advb->dvb_adapter, CARD_NAME, THIS_MODULE,
				 advb->dev, adapter_nr);
	if (ret < 0)
		return ret;
	pr_inf("Registered adpter: %s\n", CARD_NAME);

	for (i = 0; i < DMX_DEV_COUNT; i++)
		advb->dmx[i].id = -1;

	advb->dvb_adapter.priv = advb;
	dev_set_drvdata(advb->dev, advb);

	for (i = 0; i < DMX_DEV_COUNT; i++) {
		ret = wetek_dvb_dmx_init(advb, &advb->dmx[i], i);
		if (ret < 0)
			goto error;
	}
	/*Init the async fifos */
	for (i = 0; i < ASYNCFIFO_COUNT; i++) {
		ret = wetek_dvb_asyncfifo_init(advb, &advb->asyncfifo[i], i);
		if (ret < 0)
			goto error;

	}
	wetek_asyncfifo_hw_set_source(&advb->asyncfifo[0], AM_DMX_0);

	wetek_regist_dmx_class();

	if (class_register(&wetek_stb_class) < 0) {
		pr_error("register class error\n");
		goto error;
	}

	tsdemux_set_ops(&wetek_tsdemux_ops);

	if (dvb_register_frontend(&advb->dvb_adapter, p.fe[0])) {
		pr_err("Frontend wetek registration failed!!!\n");
		ops = &p.fe[0]->ops;
		if (ops->release != NULL)
			ops->release(p.fe[0]);
		p.fe[0] = NULL;
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, p.fe[0]);

	pr_inf("WeTeK dvb frontend registered successfully.\n");
	return ret;
error:
	for (i = 0; i < ASYNCFIFO_COUNT; i++) {
		if (advb->asyncfifo[i].id != -1)
			wetek_dvb_asyncfifo_release(advb, &advb->asyncfifo[i]);
	}

	for (i = 0; i < DMX_DEV_COUNT; i++) {
		if (advb->dmx[i].id != -1)
			wetek_dvb_dmx_release(advb, &advb->dmx[i]);
	}

	dvb_unregister_adapter(&advb->dvb_adapter);

	return ret;
}

void __exit wetek_dvb_exit(void)
{
	struct wetek_nims p;
	struct platform_device *pdev;
	struct wetek_dvb *advb;
	int i;

	get_nims_infos(&p);
	pdev = p.pdev;
	advb = (struct wetek_dvb *)dev_get_drvdata(&pdev->dev);

	tsdemux_set_ops(NULL);

	wetek_unregist_dmx_class();
	class_unregister(&wetek_stb_class);


	for (i = 0; i < DMX_DEV_COUNT; i++)
		wetek_dvb_dmx_release(advb, &advb->dmx[i]);

	dvb_unregister_adapter(&advb->dvb_adapter);

	for (i = 0; i < TS_IN_COUNT; i++) {
		if (advb->ts[i].pinctrl)
			devm_pinctrl_put(advb->ts[i].pinctrl);
	}

	/*switch_mod_gate_by_name("demux", 0); */
	reset_control_assert(wetek_dvb_uparsertop_reset_ctl);
	reset_control_assert(wetek_dvb_ahbarb0_reset_ctl);
	reset_control_assert(wetek_dvb_afifo_reset_ctl);					
	reset_control_assert(wetek_dvb_demux_reset_ctl);
}

static int wetek_tsdemux_reset(void)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);
	if(dvb->reset_flag) {
		struct wetek_dmx *dmx = get_stb_dmx();
		dvb->reset_flag = 0;
		if(dmx)
			dmx_reset_dmx_hw_ex_unlock(dvb, dmx, 0);
	}
	spin_unlock_irqrestore(&dvb->slock, flags);

	return 0;
}

static int wetek_tsdemux_set_reset_flag(void)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);
	dvb->reset_flag = 1;
	spin_unlock_irqrestore(&dvb->slock, flags);

	return 0;

}

/*Add the amstream irq handler*/
static int wetek_tsdemux_request_irq(irq_handler_t handler, void *data)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	struct wetek_dmx *dmx;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);

	dmx = get_stb_dmx();
	if(dmx) {
		dmx->irq_handler = handler;
		dmx->irq_data = data;
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return 0;
}

/*Free the amstream irq handler*/
static int wetek_tsdemux_free_irq(void)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	struct wetek_dmx *dmx;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);

	dmx = get_stb_dmx();
	if(dmx) {
		dmx->irq_handler = NULL;
		dmx->irq_data = NULL;
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return 0;
}

/*Reset the video PID*/
static int wetek_tsdemux_set_vid(int vpid)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	struct wetek_dmx *dmx;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dvb->slock, flags);

	dmx = get_stb_dmx();

	spin_unlock_irqrestore(&dvb->slock, flags);

	if(dmx) {
		mutex_lock(&dmx->dmxdev.mutex);

		spin_lock_irqsave(&dvb->slock, flags);

		if(dmx->vid_chan!=-1) {
			dmx_free_chan(dmx, dmx->vid_chan);
			dmx->vid_chan = -1;
		}

		if((vpid>=0) && (vpid<0x1FFF)) {
			dmx->vid_chan = dmx_alloc_chan(dmx, DMX_TYPE_TS, DMX_PES_VIDEO, vpid);
			if(dmx->vid_chan==-1) {
				ret = -1;
			}
		}

		spin_unlock_irqrestore(&dvb->slock, flags);

		mutex_unlock(&dmx->dmxdev.mutex);
	}

	return ret;
}

/*Reset the audio PID*/
static int wetek_tsdemux_set_aid(int apid)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	struct wetek_dmx *dmx;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dvb->slock, flags);

	dmx = get_stb_dmx();

	spin_unlock_irqrestore(&dvb->slock, flags);

	if(dmx) {
		mutex_lock(&dmx->dmxdev.mutex);

		spin_lock_irqsave(&dvb->slock, flags);

		if(dmx->aud_chan!=-1) {
			dmx_free_chan(dmx, dmx->aud_chan);
			dmx->aud_chan = -1;
		}

		if((apid>=0) && (apid<0x1FFF)) {
			dmx->aud_chan = dmx_alloc_chan(dmx, DMX_TYPE_TS, DMX_PES_AUDIO, apid);
			if(dmx->aud_chan==-1) {
				ret = -1;
			}
		}

		spin_unlock_irqrestore(&dvb->slock, flags);

		mutex_unlock(&dmx->dmxdev.mutex);
	}

	return ret;
}

/*Reset the subtitle PID*/
static int wetek_tsdemux_set_sid(int spid)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	struct wetek_dmx *dmx;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dvb->slock, flags);

	dmx = get_stb_dmx();

	spin_unlock_irqrestore(&dvb->slock, flags);

	if(dmx) {
		mutex_lock(&dmx->dmxdev.mutex);

		spin_lock_irqsave(&dvb->slock, flags);

		if(dmx->sub_chan!=-1) {
			dmx_free_chan(dmx, dmx->sub_chan);
			dmx->sub_chan = -1;
		}

		if((spid>=0) && (spid<0x1FFF)) {
			dmx->sub_chan = dmx_alloc_chan(dmx, DMX_TYPE_TS, DMX_PES_SUBTITLE, spid);
			if(dmx->sub_chan==-1) {
				ret = -1;
			}
		}

		spin_unlock_irqrestore(&dvb->slock, flags);

		mutex_unlock(&dmx->dmxdev.mutex);
	}

	return ret;
}

static int wetek_tsdemux_set_pcrid(int pcrpid)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	struct wetek_dmx *dmx;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dvb->slock, flags);

	dmx = get_stb_dmx();

	spin_unlock_irqrestore(&dvb->slock, flags);

	if(dmx) {
		mutex_lock(&dmx->dmxdev.mutex);

		spin_lock_irqsave(&dvb->slock, flags);

		if(dmx->pcr_chan!=-1) {
			dmx_free_chan(dmx, dmx->pcr_chan);
			dmx->pcr_chan = -1;
		}

		if((pcrpid>=0) && (pcrpid<0x1FFF)) {
			dmx->pcr_chan = dmx_alloc_chan(dmx, DMX_TYPE_TS, DMX_PES_PCR, pcrpid);
			if(dmx->pcr_chan==-1) {
				ret = -1;
			}
		}

		spin_unlock_irqrestore(&dvb->slock, flags);

		mutex_unlock(&dmx->dmxdev.mutex);
	}

	return ret;
}

static int wetek_tsdemux_set_skipbyte(int skipbyte)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);
	wetek_dmx_set_skipbyte(dvb, skipbyte);
	spin_unlock_irqrestore(&dvb->slock, flags);

	return 0;
}

static int wetek_tsdemux_set_demux(int id)
{
	struct wetek_dvb *dvb = &wetek_dvb_device;

	wetek_dmx_set_demux(dvb, id);
	return 0;
}

module_init(wetek_dvb_init);
module_exit(wetek_dvb_exit);

MODULE_DESCRIPTION("driver for WeTeK DVB card");
MODULE_AUTHOR("afl1");
MODULE_LICENSE("GPL");



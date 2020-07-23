#ifndef __SCPI_COMMON__
#define __SCPI_COMMON__

enum scpi_chan {
	SCPI_DSPA = 0, /* to dspa */
	SCPI_DSPB = 1, /* to dspb */
	SCPI_AOCPU = 2, /* to aocpu */
	SCPI_OTHER = 3, /* to other core */
	SCPI_MAXNUM,
};

extern u32 mhu_f;
extern u32 mhu_dsp_f;
extern u32 mhu_fifo_f;
#endif

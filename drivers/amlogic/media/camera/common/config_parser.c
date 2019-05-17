/*
 * drivers/amlogic/media/camera/common/config_parser.c
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

#include "config_parser.h"
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>

static struct file *fp;
mm_segment_t fs;

char *aet_key = "aet_start";
char *hw_key = "hw_start";
char *effect_key = "effect_start";
char *capture_key = "capture_start";
char *scenes_key = "scenes_start";
char *wb_key = "wb_start";
char *wave_key = "wave_start";
char *lens_key = "lens_start";
char *gamma_key = "gamma_start";
char *wb_sensor_key = "mwb_sensor_start";
char *version_key = "version_start";
char *cm_key = "cm_start";
char *nr_key = "nr_start";
char *peaking_key = "peaking_start";

struct buffer_para_s {
	char *buffer;
	int data_start;
	int data_size;
	int buffer_len;
};

void *realloc_mem(char *buffer, int new_size, int *old_size)
{
	char *tmp = vmalloc(new_size);

	if (*old_size >= new_size) {
		vfree(tmp);
		return buffer;
	}

	if (tmp != NULL) {
		memcpy(tmp, buffer, *old_size);
		*old_size = new_size;
		vfree(buffer);
		return tmp;
	} else
		return NULL;
}

/*
 ***************************************
 *Name	:   camera_open_config
 *Input	:	*config_file
 *Output	:	file size
 *function :	open the firware file, and return total size
 ***************************************
 */

int camera_open_config(const char *config_file)
{
	loff_t file_size;
	struct inode *inode = NULL;

	fp = filp_open(config_file, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("read config file error");
		return -1;
	}

	inode = fp->f_path.dentry->d_inode;
	file_size = inode->i_size;
	fs = get_fs();
	set_fs(KERNEL_DS);

	return file_size;

}

/*
 ***********************************
 *Name	:	camera_read_config
 *Input	:	offset
 *length, read length
 *buf, return buffer
 *Output	:
 *function	:	read data to buffer
 ***********************************
 */

int camera_read_config(int offset, int length, char *buf)
{
	loff_t pos = offset;

	vfs_read(fp, buf, length, &pos);
	return 0;
}

/*
 **********************************
 *Name	:	camera_close_config
 *Input	:
 *Output	:
 *function	:	close file
 **********************************
 */

int camera_close_config(void)
{
	set_fs(fs);
	filp_close(fp, NULL);
	return 0;
}

static int camera_read_buff(struct i2c_adapter *adapter,
			    unsigned short i2c_addr, char *buf, int addr_len,
			    int data_len)
{
	int i2c_flag = -1;

	struct i2c_msg msgs[] = {{
			.addr = i2c_addr, .flags = 0,
			.len = addr_len, .buf = buf,
		}, {
			.addr = i2c_addr, .flags = I2C_M_RD,
			.len = data_len, .buf = buf,
		}
	};

	i2c_flag = i2c_transfer(adapter, msgs, 2);

	return i2c_flag;
}

static int camera_write_buff(struct i2c_adapter *adapter,
			     unsigned short i2c_addr, char *buf, int len)
{
	struct i2c_msg msg[] = {{
			.addr = i2c_addr, .flags = 0,    /* |I2C_M_TEN, */
			.len = len, .buf = buf,
		}

	};

	if (i2c_transfer(adapter, msg, 1) < 0)
		return -1;
	else
		return 0;
}

int my_i2c_put_byte(struct i2c_adapter *adapter, unsigned short i2c_addr,
		    unsigned short addr, unsigned char data)
{
	unsigned char buff[4];

	buff[0] = (unsigned char)((addr >> 8) & 0xff);
	buff[1] = (unsigned char)(addr & 0xff);
	buff[2] = data;
	if (camera_write_buff(adapter, i2c_addr, buff, 3) < 0)
		return -1;
	return 0;
}

int my_i2c_put_byte_add8(struct i2c_adapter *adapter, unsigned short i2c_addr,
			 char *buf, int len)
{
	if (camera_write_buff(adapter, i2c_addr, buf, len) < 0)
		return -1;
	return 0;
}

int my_i2c_get_byte(struct i2c_adapter *adapter, unsigned short i2c_addr,
		    unsigned short addr)
{
	unsigned char buff[4];

	buff[0] = (unsigned char)((addr >> 8) & 0xff);
	buff[1] = (unsigned char)(addr & 0xff);

	if (camera_read_buff(adapter, i2c_addr, buff, 2, 1) < 0)
		return -1;
	return buff[0];
}

int my_i2c_get_word(struct i2c_adapter *adapter, unsigned short i2c_addr)
{
	unsigned char buff[4];
	unsigned short data;

	buff[0] = 0;

	if (camera_read_buff(adapter, i2c_addr, buff, 1, 2) < 0)
		return -1;

	data = buff[0];
	data = (data << 8) | buff[1];

	return data;
}

char *search_string(struct buffer_para_s *buf_para, int *offset, int *remained,
		    char *start, char *end)
{
	char *iter, *pter, *buffer;
	int data_start, data_size, buffer_len;
	int add = 0;

	buffer = buf_para->buffer;
	data_start = buf_para->data_start;
	data_size = buf_para->data_size;
	buffer_len = buf_para->buffer_len;
	iter = strstr(buffer + data_start, start);
	while (iter == NULL) {
		if (iter == NULL && *remained < strlen(start)) {
			pr_err("wrong config file");
			return NULL;
		}
		memset(buffer, 0, buffer_len);
		if (*remained < BUFFER_SIZE) {
			camera_read_config(*offset - strlen(start),
				*remained, buffer); /* check bounds */
			*offset += *remained - strlen(start);
			data_size = *remained;
			*remained = 0;
			data_start = 0;
		} else {
			camera_read_config(*offset - strlen(start),
				BUFFER_SIZE, buffer);/* check bounds */
			*offset += BUFFER_SIZE - strlen(start);
			data_size = BUFFER_SIZE;
			*remained -= BUFFER_SIZE;
			data_start = 0;
		}
		iter = strstr(buffer, start);
	}
	data_start = iter - buffer;
	/*** check **/
	if (data_start > 512) {
		data_size -= data_start;
		memmove(buffer, iter, data_size);
		*(buffer + data_size) = '\0';
		iter = buffer;
		data_start = 0;
	}
	pter = strstr(iter + strlen(start), end);
	while (pter == NULL) {
		if (pter == NULL && *remained < strlen(end)) {
			pr_err("wrong config file");
			return NULL;
		}
		buffer =
		(char *)realloc_mem(buffer, data_size + BUFFER_SIZE + 1,
			&buffer_len);
		if (buffer == NULL) {
			pr_err("realloc failed\n");
			return NULL;
		}
		if (*remained < BUFFER_SIZE) {
			camera_read_config(*offset, *remained,
				buffer + data_size); /* check bounds */
			add = *remained;
			*offset += *remained;
			*remained = 0;
		} else {
			camera_read_config(*offset, BUFFER_SIZE,
				buffer + data_size); /* check bounds */
			add = BUFFER_SIZE;
			*remained -= BUFFER_SIZE;
			*offset += BUFFER_SIZE;
		}
		*(buffer + data_size + add) = '\0';
		pter = strstr(buffer + data_size - strlen(end), end);
		data_size += add;
	}
	/* if realloc ,iter is invalid,so recalculate */
	iter = buffer + data_start;
	data_start = pter - buffer;

	buf_para->buffer = buffer;
	buf_para->data_start = data_start;
	buf_para->data_size = data_size;
	buf_para->buffer_len = buffer_len;
	return iter;
}

char *search_key(struct buffer_para_s *buf_para, int *offset, int *remained)
{
	char *iter, *buffer;
	int data_start, data_size, buffer_len;
	int add = 0;

	buffer = buf_para->buffer;
	data_start = buf_para->data_start;
	data_size = buf_para->data_size;
	buffer_len = buf_para->buffer_len;

	iter = strstr(buffer + data_start, "[");
	while (iter == NULL) {
		if (iter == NULL && *remained < 20) {
			pr_err("file end\n");
			return NULL;
		}
		memset(buffer, 0, buffer_len);
		if (*remained < BUFFER_SIZE) {
			camera_read_config(*offset,
				*remained, buffer); /* check bounds */
			*offset += *remained;
			data_size = *remained;
			*remained = 0;
			data_start = 0;
		} else {
			camera_read_config(*offset,
				BUFFER_SIZE, buffer); /* check bounds */
			*offset += BUFFER_SIZE;
			data_size = BUFFER_SIZE;
			*remained -= BUFFER_SIZE;
			data_start = 0;
		}
		iter = strstr(buffer, "[");
	}
	data_start = iter - buffer;
	/*** check **/
	if (data_start + 20 > data_size) { /* ensure we have an complete key */
		buffer =
		(char *)realloc_mem(buffer, data_size + BUFFER_SIZE + 1,
			&buffer_len);
		if (buffer == NULL) {
			pr_err("realloc failed\n");
			return NULL;
		}
		if (*remained < BUFFER_SIZE) {
			camera_read_config(*offset, *remained,
				buffer + data_size); /* check bounds */
			add = *remained;
			*offset += *remained;
			*remained = 0;

		} else {
			camera_read_config(*offset, BUFFER_SIZE,
				buffer + data_size); /* check bounds */
			add = BUFFER_SIZE;
			*remained -= BUFFER_SIZE;
			*offset += BUFFER_SIZE;
		}
		*(buffer + data_size + add) = '\0';
		data_size += add;
	}
/* if realloc ,iter is invalid,so recalculate */
	iter = buffer + data_start;

	buf_para->buffer = buffer;
	buf_para->data_start = data_start;
	buf_para->data_size = data_size;
	buf_para->buffer_len = buffer_len;
	return iter;
}

int parse_head(char *buffer, int *sum)
{
	int ret;
	char *iter;

	iter = strstr(buffer, "sum");
	if (iter == NULL)
		return -WRONG_FORMAT;
	iter += 4; /* point to value */
	ret = kstrtoint(iter, 10, sum);
	return 0;
}

int parse_body_head(char *buffer, int *no, int check, char *name)
{
	int ret;
	char *iter;

	iter = strstr(buffer, "no");
	iter += 3;
	ret = kstrtoint(iter, 10, no);
	iter = strstr(iter, "name");
	iter += 5;
	ret = kstrtos8(name, 16, iter);
	return 0;
}
int parse_aet_element_info(char **iter, struct sensor_aet_info_s *info)
{
	int ret;

	*iter = strstr(*iter, "export");
	*iter += 7;
	ret = kstrtouint(*iter, 16, &(info->fmt_main_fr));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(info->fmt_capture));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(info->fmt_hactive));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(info->fmt_vactive));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(info->fmt_rated_fr));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(info->fmt_min_fr));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(info->tbl_max_step));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(info->tbl_rated_step));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(info->tbl_max_gain));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(info->tbl_min_gain));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter,
	     16, &(info->format_transfer_parameter));
	*iter = strstr(*iter, ",");
	*iter += 1;
	return 0;

}

int parse_aet_element_tbl(char **iter, struct sensor_aet_s *tbl)
{
	int ret;

	ret = kstrtouint(*iter, 16, &(tbl->exp));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(tbl->ag));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(tbl->vts));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(tbl->gain));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(tbl->fr));
	*iter = strstr(*iter, ",");
	*iter += 1;
	return 0;
}

int parse_last_aet_element_tbl(char **iter, struct sensor_aet_s *tbl)
{
	int ret;

	ret = kstrtouint(*iter, 16, &(tbl->exp));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(tbl->ag));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(tbl->vts));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(tbl->gain));
	*iter = strstr(*iter, ",");
	*iter += 1;
	ret = kstrtouint(*iter, 16, &(tbl->fr));
	return 0;
}

int parse_effect(struct configure_s *cf, struct buffer_para_s *buf_para,
		 int *remained, int *offset)
{
	int ret1, ret2, ret3, sum, check, i;
	char *iter;

	iter = search_string(buf_para, offset, remained, "effect_start]",
			     "[effect]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	/***parser head***/
	ret1 = parse_head(iter, &sum);
	if (ret1 != 0)
		return -HEAD_FAILED;
	cf->eff.sum = sum;
	/**parser body***/
	check = 0;
	while (check < sum) {
		iter = search_string(buf_para, offset,
			remained, "[effect]", "[effect");
		if (iter == NULL)
			return -WRONG_FORMAT;
		ret2 = parse_body_head(iter, &(cf->eff.eff[check].num), check,
				      cf->eff.eff[check].name);
		if (ret2 != 0)
			return -BODY_HEAD_FAILED;
		iter = strstr(iter, "export");
		iter += 7;
		i = 0;
		while (i < EFFECT_MAX && iter != NULL) {
			ret3 = kstrtouint(iter,
			   16, &(cf->eff.eff[check].export[i]));
			i++;
			iter = strstr(iter, ",");
			if (iter == NULL)
				break;
			iter += 1;
		}
		if (i != EFFECT_MAX)
			return -CHECK_LEN_FAILED;
		check++;
	}
	if (check != sum)
		return -CHECK_FAILED;
	return 0;
}

int parse_aet(struct configure_s *cf, struct buffer_para_s *buf_para,
	      int *remained, int *offset)
{
	int sum, ret, check, i;
	char *iter;

	iter = search_string(buf_para, offset, remained, "aet_start]", "[aet]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	/***parser head***/
	ret = parse_head(iter, &sum);
	if (sum <= 0)
		return -HEAD_FAILED;
	cf->aet.sum = sum;
	/**parser body***/
	for (i = 0; i < sum; i++) {
		cf->aet.aet[i].info =
		kmalloc(sizeof(struct sensor_aet_info_s), 0);
		if ((cf->aet.aet[i].info) == NULL) {
			while (i-- > 0)
				kfree(cf->aet.aet[i].info);
			return -NO_MEM;
		}
	} /* alloc head */
	check = 0;
	while (check < sum) {
		iter = search_string(buf_para, offset,
			remained, "[aet]", "[aet");
		if (iter == NULL) {
			pr_err("aet wrong config format\n");
			ret = -WRONG_FORMAT;
			goto clean;
		}
		ret = parse_body_head(iter, &(cf->aet.aet[check].num), check,
				      cf->aet.aet[check].name);
		if (ret != 0) {
			ret = -HEAD_FAILED;
			goto clean;
		}
		ret = parse_aet_element_info(&iter, cf->aet.aet[check].info);
		if (ret != 0) {
			ret = -BODY_ELEMENT_FAILED;
			goto clean;
		}

		cf->aet.aet[check].aet_table =
		vmalloc(sizeof(struct sensor_aet_s) *
		(cf->aet.aet[check].info->tbl_max_step + 1));
		if ((cf->aet.aet[check].aet_table) == NULL) {
			for (i = 0; i < check; i++)
				vfree(cf->aet.aet[i].aet_table);
			ret = -NO_MEM;
			goto clean;
		}
		for (i = 0; i <= cf->aet.aet[check].info->tbl_max_step; i++) {
			if (i == cf->aet.aet[check].info->tbl_max_step) {
				ret = parse_last_aet_element_tbl(
			      &iter, &(cf->aet.aet[check].aet_table[i]));
			} else
				ret = parse_aet_element_tbl(&iter,
					    &(cf->aet.aet[check].aet_table[i]));
			if (ret != 0) {
				ret = -BODY_ELEMENT_FAILED;
				goto clean_table;
			}
		}
		check++;
	}
	return 0;

clean_table:
	for (i = 0; i <= check; i++)
		vfree(cf->aet.aet[i].aet_table);
clean:
	for (i = 0; i < sum; i++)
		kfree(cf->aet.aet[i].info);
	return ret;
}

int parse_hw(struct configure_s *cf, struct buffer_para_s *buf_para,
	     int *remained, int *offset)
{
	int ret1, ret2, ret3, sum, check, i;
	char *iter = NULL;
	char *eter = NULL;

	iter = search_string(buf_para, offset, remained, "hw_start]", "[hw]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	/***parser head***/
	ret1 = parse_head(iter, &sum);
	if (ret1 != 0)
		return -HEAD_FAILED;
	cf->hw.sum = sum;
	/**parser body***/
	check = 0;
	while (check < sum) {
		iter = search_string(buf_para, offset, remained, "[hw]", "[hw");
		if (iter == NULL)
			return -WRONG_FORMAT;
		ret2 = parse_body_head(iter, &(cf->hw.hw[check].num), check,
				      cf->hw.hw[check].name);
		if (ret2 != 0)
			return -BODY_HEAD_FAILED;
		iter = strstr(iter, "export");
		iter += 7;
		eter = strstr(iter, ";");
		if (eter == NULL)
			return -WRONG_FORMAT;
		i = 0;
		while (iter < eter) {
			ret3 = kstrtouint(iter,
			   16, &(cf->hw.hw[check].export[i]));
			i++;
			iter = strstr(iter, ",");
			if (iter == NULL)
				break;
			iter += 1;
		}
		check++;
	}
	if (check != sum)
		return -CHECK_FAILED;
	return 0;
}

int parse_wb(struct configure_s *cf, struct buffer_para_s *buf_para,
	     int *remained, int *offset)
{
	int ret1, ret2, ret3, sum, check, i;
	char *iter = NULL;
	char *eter = NULL;

	iter = search_string(buf_para, offset, remained, "wb_start]", "[wb]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	/***parser head***/
	ret1 = parse_head(iter, &sum);
	if (ret1 != 0)
		return -HEAD_FAILED;
	cf->wb.sum = sum;
	/**parser body***/
	check = 0;
	while (check < sum) {
		iter = search_string(buf_para, offset, remained, "[wb]", "[wb");
		if (iter == NULL)
			return -WRONG_FORMAT;
		ret2 = parse_body_head(iter, &(cf->wb.wb[check].num), check,
				      cf->wb.wb[check].name);
		if (ret2 != 0)
			return -BODY_HEAD_FAILED;
		iter = strstr(iter, "export");
		iter += 7;
		eter = strstr(iter, ";");
		if (eter == NULL)
			return -WRONG_FORMAT;
		i = 0;
		while (iter < eter) {
			ret3 = kstrtouint(iter,
			   16, &(cf->wb.wb[check].export[i]));
			i++;
			iter = strstr(iter, ",");
			if (iter == NULL)
				break;
			iter += 1;

		}
		if (i != WB_MAX)
			return -CHECK_LEN_FAILED;
		check++;
	}
	if (check != sum)
		return -CHECK_FAILED;
	return 0;

}

int parse_capture(struct configure_s *cf, struct buffer_para_s *buf_para,
		  int *remained, int *offset)
{
	int ret1, ret2, ret3, sum, check, i;
	char *iter = NULL;
	char *eter = NULL;

	iter = search_string(buf_para, offset, remained, "capture_start]",
			     "[capture]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	/***parser head***/
	ret1 = parse_head(iter, &sum);
	if (ret1 != 0)
		return -HEAD_FAILED;
	cf->capture.sum = sum;
	/* pr_err("capture sum:%d\n",sum); */
	/**parser body***/
	check = 0;
	while (check < sum) {
		iter = search_string(buf_para, offset, remained, "[capture]",
				     "[capture");
		if (iter == NULL) {
			pr_err("search wrong\n");
			return -WRONG_FORMAT;
		}
		ret2 = parse_body_head(iter,
				&(cf->capture.capture[check].num), check,
				      cf->capture.capture[check].name);
		if (ret2 != 0)
			return -BODY_HEAD_FAILED;
		/* pr_err("name:%s\n",cf->capture.capture[check].name); */
		iter = strstr(iter, "export");
		if (iter == NULL) {
			pr_err("iter is NULL\n");
			return -WRONG_FORMAT;
		}
		iter += 7;
		eter = strstr(iter, ";");
		if (eter == NULL)
			return -WRONG_FORMAT;
		i = 0;
		while (iter < eter) {
			ret3 = kstrtouint(iter,
			    16, &(cf->capture.capture[check].export[i]));
			i++;
			iter = strstr(iter, ",");
			if (iter == NULL)
				break;
			iter += 1;
		}
		if (i != CAPTURE_MAX)
			return -CHECK_LEN_FAILED;
		check++;
	}
	if (check != sum)
		return -CHECK_FAILED;
	return 0;

}

int parse_wave(struct configure_s *cf, struct buffer_para_s *buf_para,
	       int *remained, int *offset)
{
	int i, ret;
	char *iter = NULL;
	char *eter = NULL;

	iter = search_string(buf_para, offset, remained, "wave_start]",
			     "[wave_end]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	iter = strstr(iter, "export");
	iter += 7;
	eter = strstr(iter, ";");
	if (eter == NULL)
		return -WRONG_FORMAT;
	i = 0;
	while (iter < eter) {
		ret = kstrtouint(iter, 16, &(cf->wave.export[i]));
		/* pr_err("wave:%x\n",cf->wave.export[i]); */
		iter = strstr(iter, ",");
		i++;
		if (iter == NULL)
			break;
		iter += 1;
	}
	if (i != WAVE_MAX)
		return -CHECK_LEN_FAILED;
	return 0;
}

int parse_scene(struct configure_s *cf, struct buffer_para_s *buf_para,
		int *remained, int *offset)
{
	int sum, ret1, ret2, ret3, check, i;
	char *iter = NULL;
	char *eter = NULL;

	iter = search_string(buf_para, offset, remained, "scenes_start]",
			     "[scenes]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	/***parser head***/
	ret1 = parse_head(iter, &sum);
	if (sum <= 0)
		return -HEAD_FAILED;
	cf->scene.sum = sum;

	/**parser body***/
	check = 0;
	while (check < sum) {
		iter = search_string(buf_para, offset,
					remained, "[scenes]", "[scenes");
		if (iter == NULL) {
			pr_err("scene wrong config format\n");
			return -WRONG_FORMAT;
		}
		ret2 = parse_body_head(iter, &((cf->scene.scene[check]).num),
				check, (cf->scene.scene[check].name));
		if (ret2 != 0)
			return -BODY_HEAD_FAILED;
		iter = strstr(iter, "export");
		iter += 7;
		eter = strstr(iter, ";");
		if (eter == NULL)
			return -WRONG_FORMAT;
		i = 0;
		while (iter < eter) {
			ret3 = kstrtouint(iter,
			   16, &(cf->scene.scene[check].export[i]));
			i++;
			iter = strstr(iter, ",");
			if (iter == NULL)
				break;
			iter += 1;
		}
		if (i != SCENE_MAX)
			return -CHECK_LEN_FAILED;
		check++;
	}
	if (check != sum)
		return -CHECK_FAILED;
	return 0;
}

int parse_lens(struct configure_s *cf, struct buffer_para_s *buf_para,
	       int *remained, int *offset)
{
	int sum, ret1, ret2, ret3, check, i;
	char *iter = NULL;
	char *eter = NULL;

	iter = search_string(buf_para, offset,
				remained, "lens_start]", "[lens]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	/***parser head***/
	ret1 = parse_head(iter, &sum);
	if (sum <= 0)
		return -HEAD_FAILED;
	cf->lens.sum = sum;

	/**parser body***/
	check = 0;
	while (check < sum) {
		iter = search_string(buf_para, offset,
				remained, "[lens]", "[lens");
		if (iter == NULL) {
			pr_err("lens wrong config format\n");
			return -WRONG_FORMAT;
		}
		ret2 = parse_body_head(iter,
				&((cf->lens.lens[check]).num),
				check,
				(cf->lens.lens[check].name));
		if (ret2 != 0)
			return -BODY_HEAD_FAILED;
		iter = strstr(iter, "export");
		iter += 7;
		eter = strstr(iter, ";");
		if (eter == NULL)
			return -WRONG_FORMAT;
		i = 0;
		while (iter < eter) {
			ret3 = kstrtouint(iter,
			   16, &(cf->lens.lens[check].export[i]));
			i++;
			iter = strstr(iter, ",");
			if (iter == NULL)
				break;
			iter += 1;
		}
		if (i != LENS_MAX)
			return -CHECK_LEN_FAILED;
		check++;
	}
	if (check != sum)
		return -CHECK_FAILED;
	return 0;
}

int parse_gamma(struct configure_s *cf, struct buffer_para_s *buf_para,
		int *remained, int *offset)
{
	int i, ret;
	char *iter;

	iter = search_string(buf_para, offset, remained, "gamma_start]",
			     "[gamma_end]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	/***parser head***/
	iter = strstr(iter, "export");
	iter += 7;
	for (i = 0; i < GAMMA_MAX && iter != NULL;) {
		ret = kstrtouint(iter, 16, &(cf->gamma.gamma_r[i]));
		i++;
		iter = strstr(iter, ",");
		if (iter == NULL)
			break;
		iter += 1;
	}
	if (i != GAMMA_MAX)
		return -CHECK_LEN_FAILED;
	for (i = 0; i < GAMMA_MAX && iter != NULL;) {
		ret = kstrtouint(iter, 16, &(cf->gamma.gamma_g[i]));
		i++;
		iter = strstr(iter, ",");
		if (iter == NULL)
			break;
		iter += 1;
	}
	if (i != GAMMA_MAX)
		return -CHECK_LEN_FAILED;
	for (i = 0; i < GAMMA_MAX && iter != NULL;) {
		ret = kstrtouint(iter,
		   16, &(cf->gamma.gamma_b[i]));
		i++;
		iter = strstr(iter, ",");
		if (iter == NULL)
			break;
		iter += 1;
	}
	if (i != GAMMA_MAX)
		return -CHECK_LEN_FAILED;
	return 0;
}

int parse_wb_sensor(struct configure_s *cf, struct buffer_para_s *buf_para,
		    int *remained, int *offset)
{
	int i, ret;
	char *iter;

	iter = search_string(buf_para, offset, remained, "mwb_sensor_start]",
			     "[mwb_sensor_end]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	/***parser head***/
	iter = strstr(iter, "export");
	iter += 7;
	i = 0;
	while (i < WB_SENSOR_MAX && iter != NULL) {
		ret = kstrtouint(iter, 16, &(cf->wb_sensor_data.export[i]));
		/* pr_err("wb sensor:%x\n",cf->wb_sensor_data.export[i]); */
		i++;
		iter = strstr(iter, ",");
		if (iter == NULL)
			break;
		iter += 1;
	}
	if (i != WB_SENSOR_MAX)
		return -CHECK_LEN_FAILED;
	return 0;
}

int parse_version(struct configure_s *cf, struct buffer_para_s *buf_para,
		  int *remained, int *offset)
{
	char *iter, *end;
	int len = 0;

	iter = search_string(buf_para, offset, remained, "version_start]",
			     "[version_end]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	iter = strstr(iter, "Date");
	iter += 5;
	end = strstr(iter, "Module");
	if (end == NULL)
		return -WRONG_FORMAT;
	len = end - iter;
	memcpy(cf->version.date, iter, len);
	cf->version.date[len] = '\0';

	iter = end + 7;
	end = strstr(iter, "Version");
	if (end == NULL)
		return -WRONG_FORMAT;
	len = end - iter;
	memcpy(cf->version.module, iter, len);
	cf->version.module[len] = '\0';

	iter = end + 8;
	end = strstr(iter, "[version");
	if (end == NULL)
		return -WRONG_FORMAT;
	len = end - iter;
	memcpy(cf->version.version, iter, len);
	cf->version.version[len] = '\0';
	pr_info("version:%s", cf->version.version);
	return 0;
}

int parse_cm(struct configure_s *cf, struct buffer_para_s *buf_para,
	     int *remained, int *offset)
{
	int i;
	int ret;
	char *iter;

	iter = search_string(buf_para, offset,
			remained, "cm_start]", "[cm_end]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	/***parser body***/
	iter = strstr(iter, "export");
	iter += 7;
	i = 0;
	while (i < CM_MAX && iter != NULL) {
		ret = kstrtouint(iter, 16, &(cf->cm.export[i]));
		/* pr_err("cm:%x\n",cf->cm.export[i]); */
		i++;
		iter = strstr(iter, ",");
		if (iter == NULL)
			break;
		iter += 1;
	}
	if (i != CM_MAX)
		return -CHECK_LEN_FAILED;
	return 0;
}

int parse_nr(struct configure_s *cf, struct buffer_para_s *buf_para,
	     int *remained, int *offset)
{
	int ret1, ret2, ret3, sum, check, i;
	char *iter;

	iter = search_string(buf_para, offset, remained, "nr_start]", "[nr]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	/***parser head***/
	ret1 = parse_head(iter, &sum);
	if (ret1 != 0)
		return -HEAD_FAILED;
	cf->nr.sum = sum;
	/* pr_err("nr sum:%d\n",sum); */
	/**parser body***/
	check = 0;
	while (check < sum) {
		iter = search_string(buf_para, offset, remained, "[nr]", "[nr");
		if (iter == NULL) {
			pr_err("search wrong\n");
			return -WRONG_FORMAT;
		}
		ret2 = parse_body_head(iter, &(cf->nr.nr[check].num), check,
				      cf->nr.nr[check].name);
		if (ret2 != 0)
			return -BODY_HEAD_FAILED;
		iter = strstr(iter, "export");
		iter += 7;
		i = 0;
		while (i < NR_MAX && iter != NULL) {
			ret3 = kstrtouint(iter, 16,
			    &(cf->nr.nr[check].export[i]));
			/* pr_err("nr:%x\n",cf->nr.nr[check].export[i]); */
			i++;
			iter = strstr(iter, ",");
			if (iter == NULL)
				break;
			iter += 1;
		}
		if (i != NR_MAX)
			return -CHECK_LEN_FAILED;
		check++;
	}
	if (check != sum)
		return -CHECK_FAILED;
	return 0;
}

int parse_peaking(struct configure_s *cf, struct buffer_para_s *buf_para,
		  int *remained, int *offset)
{
	int ret1, ret2, ret3, sum, check, i;
	char *iter;

	iter = search_string(buf_para, offset, remained, "peaking_start]",
			     "[peaking]");
	if (iter == NULL)
		return -WRONG_FORMAT;
	/***parser head***/
	ret1 = parse_head(iter, &sum);
	if (ret1 != 0)
		return -HEAD_FAILED;
	cf->peaking.sum = sum;
	/* pr_err("peaking sum:%d\n",sum); */
	/**parser body***/
	check = 0;
	while (check < sum) {
		iter = search_string(buf_para, offset, remained, "[peaking]",
				     "[peaking");
		if (iter == NULL) {
			pr_err("search wrong\n");
			return -WRONG_FORMAT;
		}
		ret2 = parse_body_head(iter,
				&(cf->peaking.peaking[check].num), check,
				cf->peaking.peaking[check].name);
		if (ret2 != 0)
			return -BODY_HEAD_FAILED;
		iter = strstr(iter, "export");
		iter += 7;
		i = 0;
		while (i < PEAKING_MAX && iter != NULL) {
			ret3 = kstrtouint(iter,
			   16, &(cf->peaking.peaking[check].export[i]));
			i++;
			iter = strstr(iter, ",");
			if (iter == NULL)
				break;
			iter += 1;
		}
		if (i != PEAKING_MAX)
			return -CHECK_LEN_FAILED;
		check++;
	}
	if (check != sum)
		return -CHECK_FAILED;
	return 0;
}

int parse_config(const char *path, struct configure_s *cf)
{
	char *buffer, *iter;
	int file_size;
	int remained_size;
	int read_offset = 0;
	int ret = 0;
	struct buffer_para_s buf_para;

	buffer = vmalloc(BUFFER_SIZE + 1);/*(char *)*/
	if (buffer == NULL)
		return -NO_MEM;

	buf_para.buffer = buffer;

	memset(cf, 0, sizeof(struct configure_s));
	file_size = camera_open_config(path);
	if (file_size < 0) {
		pr_err("open failed :%d\n", file_size);
		ret = -READ_ERROR;
		goto clean_mem;
	} else if (file_size == 0) {
		pr_err("file is null\n");
		ret = -READ_ERROR;
		goto clean_all;
	} else {
		if (file_size < BUFFER_SIZE) {
			camera_read_config(0, file_size, buffer);
			remained_size = 0;
			read_offset = file_size;
			*(buffer + file_size) = '\0';
		} else {
			camera_read_config(0, BUFFER_SIZE, buffer);
			remained_size = file_size - BUFFER_SIZE;
			read_offset = BUFFER_SIZE;
			*(buffer + BUFFER_SIZE) = '\0';
		}
	}
	buf_para.data_start = 0;
	buf_para.data_size = read_offset;
	buf_para.buffer_len = BUFFER_SIZE;

	while (read_offset <= file_size) {
		iter = search_key(&buf_para, &read_offset, &remained_size);
		if (iter == NULL) {
			pr_info("finish parse file\n");
			return 0;
		}
		iter += 1;
		buf_para.data_start += 1;
		switch (*iter) {
		case 'a':
			if (memcmp(iter, aet_key, strlen(aet_key)) == 0) {
				cf->aet_valid = 1;
				ret = parse_aet(cf, &buf_para,
					&remained_size, &read_offset);
				if (ret != 0) {
					cf->aet_valid = 0;
					pr_err("aet invalid :%d\n", ret);
					goto clean_all;
				}
			} else
				buf_para.data_start += strlen(aet_key);
			break;
		case 'h':
			if (memcmp(iter, hw_key, strlen(hw_key)) == 0) {
				cf->hw_valid = 1;
				ret = parse_hw(cf, &buf_para,
					&remained_size, &read_offset);
				if (ret != 0) {
					cf->hw_valid = 0;
					pr_err("hw invalid :%d\n", ret);
					goto clean_all;
				}
			} else
				buf_para.data_start += strlen(hw_key);
			break;
		case 'e':
			if (memcmp(iter, effect_key, strlen(effect_key)) == 0) {
				cf->effect_valid = 1;
				ret = parse_effect(cf, &buf_para,
					&remained_size, &read_offset);
				if (ret != 0) {
					cf->effect_valid = 0;
					pr_err("effect invalid :%d\n", ret);
					goto clean_all;
				}
			} else
				buf_para.data_start += strlen(effect_key);
			break;
		case 'w':
			if (*(iter + 1) == 'b') {
				if (memcmp(iter, wb_key, strlen(wb_key)) == 0) {
					cf->wb_valid = 1;
					ret = parse_wb(cf, &buf_para,
						&remained_size, &read_offset);
					if (ret != 0) {
						cf->wb_valid = 0;
						pr_err("wb invalid :%d\n", ret);
						goto clean_all;
					}
				} else
					buf_para.data_start += strlen(wb_key);
			} else if (*(iter + 1) == 'a') {
				if (memcmp(iter, wave_key,
						strlen(wave_key)) == 0) {
					cf->wave_valid = 1;
					ret = parse_wave(cf, &buf_para,
						&remained_size, &read_offset);
					if (ret != 0) {
						cf->wave_valid = 0;
						pr_err("wave invalid :%d\n",
							ret);
						goto clean_all;
					}
				} else
					buf_para.data_start += strlen(wave_key);
			} else
				buf_para.data_start += 1;

			break;
		case 's':
			if (memcmp(iter, scenes_key,
					strlen(scenes_key)) == 0) {
				cf->scene_valid = 1;
				ret = parse_scene(cf, &buf_para,
				&remained_size, &read_offset);
				if (ret != 0) {
					cf->scene_valid = 0;
					pr_err("scene invalid :%d\n", ret);
					goto clean_all;
				}
			} else
				buf_para.data_start += strlen(scenes_key);
			break;
		case 'c':
			if (*(iter + 1) == 'a') {
				if (memcmp(iter, capture_key,
						strlen(capture_key)) == 0) {
					cf->capture_valid = 1;
					ret = parse_capture(cf, &buf_para,
						&remained_size, &read_offset);
					if (ret != 0) {
						cf->capture_valid = 0;
						pr_err("capture invalid :%d\n",
							ret);
						goto clean_all;
					}
				} else
					buf_para.data_start +=
						strlen(capture_key);
			} else if (*(iter + 1) == 'm') {
				if (memcmp(iter, cm_key, strlen(cm_key)) == 0) {
					cf->cm_valid = 1;
					ret = parse_cm(cf, &buf_para,
						&remained_size, &read_offset);
					if (ret != 0) {
						cf->cm_valid = 0;
						pr_err("cm invalid :%d\n", ret);
						goto clean_all;
					}
				} else
					buf_para.data_start += strlen(cm_key);
			} else
				buf_para.data_start += 1;
			break;
		case 'l':
			if (memcmp(iter, lens_key, strlen(lens_key)) == 0) {
				cf->lens_valid = 1;
				ret = parse_lens(cf, &buf_para,
					&remained_size, &read_offset);
				if (ret != 0) {
					cf->lens_valid = 0;
					pr_err("lens invalid :%d\n", ret);
					goto clean_all;
				}
			} else
				buf_para.data_start += strlen(lens_key);
			break;
		case 'g':
			if (memcmp(iter, gamma_key, strlen(gamma_key)) == 0) {
				cf->gamma_valid = 1;
				ret = parse_gamma(cf, &buf_para,
					&remained_size, &read_offset);
				if (ret != 0) {
					cf->gamma_valid = 0;
					pr_err("gamma invalid :%d\n", ret);
					goto clean_all;
				}
			} else
				buf_para.data_start += strlen(gamma_key);
			break;
		case 'm':
			if (memcmp(iter, wb_sensor_key,
					strlen(wb_sensor_key)) == 0) {
				cf->wb_sensor_data_valid = 1;
				ret = parse_wb_sensor(cf, &buf_para,
					&remained_size, &read_offset);
				if (ret != 0) {
					cf->wb_sensor_data_valid = 0;
					pr_err("wb sensor data invalid :%d\n",
						ret);
					goto clean_all;
				}
			} else
				buf_para.data_start += strlen(wb_sensor_key);
			break;
		case 'v':
			if (memcmp(iter, version_key,
					strlen(version_key)) == 0) {
				cf->version_info_valid = 1;
				ret = parse_version(cf, &buf_para,
					&remained_size, &read_offset);
				if (ret != 0) {
					cf->version_info_valid = 0;
					pr_err("version info invalid :%d\n",
									ret);
					goto clean_all;
				}
			} else
				buf_para.data_start += strlen(version_key);
			break;
		case 'n':
			if (memcmp(iter, nr_key, strlen(nr_key)) == 0) {
				cf->nr_valid = 1;
				ret = parse_nr(cf, &buf_para,
					&remained_size, &read_offset);
				if (ret != 0) {
					cf->nr_valid = 0;
					pr_err("nr invalid :%d\n", ret);
					goto clean_all;
				}
			} else
				buf_para.data_start += strlen(nr_key);
			break;
		case 'p':
			if (memcmp(iter, peaking_key,
					strlen(peaking_key)) == 0) {
				cf->peaking_valid = 1;
				ret = parse_peaking(cf, &buf_para,
						&remained_size, &read_offset);
				if (ret != 0) {
					cf->peaking_valid = 0;
					pr_err("peaking invalid :%d\n", ret);
					goto clean_all;
				}
			} else
				buf_para.data_start += strlen(peaking_key);
			break;
		default:
			buf_para.data_start += 1;
			break;
		}
	}
	ret = 0;

clean_all:
	camera_close_config();
clean_mem:
	vfree(buf_para.buffer);
	return ret;
}

struct hw_para_s {
	char name[20];
	int size;
	int *array;
};

struct hw_para_s hw_para[] = {
	{"TOP", XML_TOP, NULL}, {"TP", XML_TP, NULL}, {"CG", XML_CG, NULL}, {
		"LENS_SHADING", XML_LS, NULL
	}, {"DFT", XML_DP, NULL}, {
		"DMS", XML_DM, NULL
	}, {"MATRIX", XML_CSC, NULL}, {
		"PEAKING", XML_SH, NULL
	}, {"NR", XML_NR, NULL}, {"AWB", XML_AWB, NULL},
	{"AE", XML_AE, NULL}, {"AF", XML_AF, NULL}, {"BLNR", XML_BN, NULL}, {
		"DBG", XML_DBG, NULL
	}, {"GC", XML_GC, NULL}, {"", 0, NULL}
};

void init_hw_para(struct xml_default_regs_s *reg)
{
	hw_para[0].array = &(reg->top.reg_map[0]);
	hw_para[1].array = &(reg->tp.reg_map[0]);
	hw_para[2].array = &(reg->cg.reg_map[0]);
	hw_para[3].array = &(reg->ls.reg_map[0]);
	hw_para[4].array = &(reg->dp.reg_map[0]);
	hw_para[5].array = &(reg->dm.reg_map[0]);
	hw_para[6].array = &(reg->csc.reg_map[0]);
	hw_para[7].array = &(reg->sharp.reg_map[0]);
	hw_para[8].array = &(reg->nr.reg_map[0]);
	hw_para[9].array = &(reg->awb_reg.reg_map[0]);
	hw_para[10].array = &(reg->ae_reg.reg_map[0]);
	hw_para[11].array = &(reg->af_reg.reg_map[0]);
	hw_para[12].array = &(reg->bn.reg_map[0]);
	hw_para[13].array = &(reg->dbg.reg_map[0]);
	hw_para[14].array = &(reg->gc.reg_map[0]);
}

/* call back functions */

unsigned int get_aet_max_step(void *priv)
{
	struct camera_priv_data_s *camera_priv_data =
		(struct camera_priv_data_s *)priv;
	if (camera_priv_data == NULL ||
			camera_priv_data->sensor_aet_info == NULL) {
		pr_err("get_aet_max_step null\n");
		WARN_ON((!camera_priv_data) ||
		 (!camera_priv_data->sensor_aet_info));
	}
	return camera_priv_data->sensor_aet_info->tbl_max_step;
}

unsigned int get_aet_max_gain(void *priv)
{
	struct camera_priv_data_s *camera_priv_data =
		(struct camera_priv_data_s *)priv;
	if (camera_priv_data == NULL ||
			camera_priv_data->sensor_aet_info == NULL) {
		pr_err("get_aet_max_gain null\n");
		WARN_ON((!camera_priv_data) ||
		 (!camera_priv_data->sensor_aet_info));
	}
	return camera_priv_data->sensor_aet_info->tbl_max_gain;
}

unsigned int get_aet_min_gain(void *priv)
{
	struct camera_priv_data_s *camera_priv_data =
		(struct camera_priv_data_s *)priv;
	if (camera_priv_data == NULL ||
			camera_priv_data->sensor_aet_info == NULL) {
		pr_err("get_aet_min_gain null\n");
		WARN_ON((!camera_priv_data) ||
		 (!camera_priv_data->sensor_aet_info));
	}
	return camera_priv_data->sensor_aet_info->tbl_min_gain;
}

unsigned int get_aet_current_step(void *priv)
{
	struct camera_priv_data_s *camera_priv_data =
		(struct camera_priv_data_s *)priv;
	if (camera_priv_data == NULL ||
			camera_priv_data->sensor_aet_info == NULL) {
		pr_err("get_aet_current_step null\n");
		WARN_ON((!camera_priv_data) ||
		 (!camera_priv_data->sensor_aet_info));
	}
	return camera_priv_data->sensor_aet_step;
}

unsigned int get_aet_current_gain(void *priv)
{
	struct sensor_aet_s *sensor_aet_table;
	unsigned int sensor_aet_step;
	struct camera_priv_data_s *camera_priv_data =
		(struct camera_priv_data_s *)priv;
	if (camera_priv_data == NULL)
		WARN_ON(!camera_priv_data);

	sensor_aet_table =
		camera_priv_data->sensor_aet_table;
	sensor_aet_step = camera_priv_data->sensor_aet_step;

	if (sensor_aet_table == NULL) {
		pr_err("get_aet_current_gain null\n");
		WARN_ON(!sensor_aet_table);
	}
	return sensor_aet_table[sensor_aet_step].gain;
}

unsigned int get_aet_new_gain(void *priv, unsigned int new_step)
{
	struct camera_priv_data_s *camera_priv_data =
		(struct camera_priv_data_s *)priv;
	struct sensor_aet_s *sensor_aet_table;

	if (camera_priv_data == NULL)
		WARN_ON(!camera_priv_data);
	sensor_aet_table =
		camera_priv_data->sensor_aet_table;
	if (sensor_aet_table == NULL) {
		pr_err("get_aet_current_gain null\n");
		WARN_ON(!sensor_aet_table);
	}
	return sensor_aet_table[new_step].gain;
}

int generate_para(struct cam_parameter_s *para, struct para_index_s pindex,
		  struct configure_s *cf)
{
	int i = 0;
	int j = 0;
	struct xml_scenes_s *scene;
	struct xml_default_regs_s *reg;
	struct xml_effect_manual_s *effect;
	struct xml_wb_manual_s *wb;
	struct xml_capture_s *capture;
	struct wave_s *wave;

	/**init callback func**/
	para->cam_function.set_af_new_step = NULL;
	if (cf->aet_valid == 1) {
		para->cam_function.get_aet_current_step = get_aet_current_step;
		para->cam_function.get_aet_max_step = get_aet_max_step;
		para->cam_function.get_aet_current_gain = get_aet_current_gain;
		para->cam_function.get_aet_min_gain = get_aet_min_gain;
		para->cam_function.get_aet_max_gain = get_aet_max_gain;
		para->cam_function.get_aet_gain_by_step = get_aet_new_gain;

	}
	/**init scenes**/
	if (cf->scene_valid == 1) {
		para->xml_scenes =
			vmalloc(sizeof(struct xml_scenes_s));
		if (para->xml_scenes == NULL) {
			pr_err("alloc mem failed\n");
			return -ENOMEM;
		}
		scene = para->xml_scenes;
		memcpy(&(scene->ae),
			cf->scene.scene[pindex.scenes_index].export,
			AE_LEN * sizeof(unsigned int));
		memcpy(&(scene->awb),
			cf->scene.scene[pindex.scenes_index].export + AE_LEN,
			AWB_LEN * sizeof(unsigned int));
		memcpy(&(scene->af),
		cf->scene.scene[pindex.scenes_index].export +
		(AE_LEN + AWB_LEN),
		AF_LEN * sizeof(unsigned int));
	} else
		para->xml_scenes = NULL;

	/**init hw**/
	if (cf->hw_valid == 1) {
		para->xml_regs_map = vmalloc(sizeof(struct xml_default_regs_s));
		if (para->xml_regs_map == NULL) {
			pr_err("alloc mem failed\n");
			return -ENOMEM;
		}
		reg = para->xml_regs_map;
		init_hw_para(reg);
		for (i = 0; i < cf->hw.sum; i++) {
			if ((strcmp(hw_para[i].name, cf->hw.hw[i].name)) == 0) {
				for (j = 0; j < hw_para[i].size; j++)
					hw_para[i].array[j] =
						cf->hw.hw[i].export[j];
			}
		}

	} else
		para->xml_regs_map = NULL;
	/** init gamma **/
	if (cf->gamma_valid == 1) {
		if (para->xml_regs_map == NULL) {
			para->xml_regs_map =
				vmalloc(sizeof(struct xml_default_regs_s));
			if (para->xml_regs_map == NULL) {
				pr_err("alloc mem failed\n");
				return -ENOMEM;
			}
		}
		reg = para->xml_regs_map;
		memcpy(reg->lut_gc.gamma_r, cf->gamma.gamma_r,
		       GAMMA_MAX * sizeof(unsigned short));
		memcpy(reg->lut_gc.gamma_g, cf->gamma.gamma_g,
		       GAMMA_MAX * sizeof(unsigned short));
		memcpy(reg->lut_gc.gamma_b, cf->gamma.gamma_b,
		       GAMMA_MAX * sizeof(unsigned short));
	}
	/**init effect**/
	if (cf->effect_valid == 1) {
		para->xml_effect_manual =
			kmalloc(sizeof(struct xml_effect_manual_s), 0);
		if (para->xml_effect_manual == NULL) {
			pr_err("alloc mem failed\n");
			return -ENOMEM;
		}
		effect = para->xml_effect_manual;
		memcpy(effect->csc.reg_map,
		cf->eff.eff[pindex.effect_index].export,
		EFFECT_MAX * sizeof(unsigned int));
	} else
		para->xml_effect_manual = NULL;
	/**init wb**/
	if (cf->wb_valid == 1) {
		para->xml_wb_manual =
			kmalloc(sizeof(struct xml_wb_manual_s), 0);
		if (para->xml_wb_manual == NULL) {
			pr_err("alloc mem failed\n");
			return -ENOMEM;
		}
		wb = para->xml_wb_manual;
		memcpy(wb->reg_map, cf->wb.wb[pindex.wb_index].export,
		       2 * sizeof(unsigned int));
	} else
		para->xml_wb_manual = NULL;

	/**init capture**/
	if (cf->capture_valid == 1) {
		para->xml_capture = vmalloc(sizeof(struct xml_capture_s));
		if (para->xml_capture == NULL) {
			pr_err("alloc mem failed\n");
			return -ENOMEM;
		}
		capture = para->xml_capture;
		capture->ae_try_max_cnt = (unsigned int)(cf->capture.
			capture[pindex.capture_index].export[0]);
		capture->sigle_count = (unsigned int)(cf->capture.
			capture[pindex.capture_index].export[3]);
		capture->skip_step = (unsigned int)(cf->capture.
			capture[pindex.capture_index].export[4]);
		capture->multi_capture_num = (unsigned int)(cf->capture.
			capture[pindex.capture_index].export[5]);
		capture->af_mode = (enum cam_scanmode_e)(cf->capture.
			capture[pindex.capture_index].export[2]);
		capture->eyetime = (unsigned int)(cf->capture.
			capture[pindex.capture_index].export[6]);
		capture->pretime = (unsigned int)(cf->capture.
			capture[pindex.capture_index].export[7]);
		capture->postime = (unsigned int)(cf->capture.
			capture[pindex.capture_index].export[8]);
	} else
		para->xml_capture = NULL;
	/**init wave**/
	if (cf->wave_valid == 1) {
		para->xml_wave = kmalloc(sizeof(struct wave_s), 0);
		if (para->xml_wave == NULL) {
			pr_err("alloc mem failed\n");
			return -ENOMEM;
		}
		wave = para->xml_wave;
		memcpy(wave, cf->wave.export, WAVE_MAX * sizeof(unsigned int));
	} else
		para->xml_wave = NULL;
	return 0;
}

void free_para(struct cam_parameter_s *para)
{
	if (para->xml_peripheral != NULL) {
		kfree(para->xml_peripheral);
		para->xml_peripheral = NULL;
	}
	if (para->xml_scenes != NULL) {
		vfree(para->xml_scenes);
		para->xml_scenes = NULL;
	}
	if (para->xml_regs_map != NULL) {
		vfree(para->xml_regs_map);
		para->xml_regs_map = NULL;
	}
	if (para->xml_effect_manual != NULL) {
		kfree(para->xml_effect_manual);
		para->xml_effect_manual = NULL;
	}
	if (para->xml_wb_manual != NULL) {
		kfree(para->xml_wb_manual);
		para->xml_wb_manual = NULL;
	}
	if (para->xml_capture != NULL) {
		vfree(para->xml_capture);
		para->xml_capture = NULL;
	}
	if (para->xml_wave != NULL) {
		kfree(para->xml_wave);
		para->xml_wave = NULL;
	}
}

int update_fmt_para(int width, int height, struct cam_parameter_s *para,
		    struct para_index_s *pindex, struct configure_s *cf)
{
	int i = 0;
	struct xml_default_regs_s *reg;

	if (cf->lens_valid == 1) {
		while (i < cf->lens.sum) {
			if (cf->lens.lens[i].export[0] == width &&
		    cf->lens.lens[i].export[1] == height)
				break;
			i++;
		}
		if (i < cf->lens.sum)
			pindex->lens_index = i;
		else {
			pr_info("width:%x,height:%x no match lens param\n",
		    width, height);
			pindex->lens_index = 0;
		}
		/** init lens **/
		if (para->xml_regs_map == NULL) {
			para->xml_regs_map =
			vmalloc(sizeof(struct xml_default_regs_s));
			if (para->xml_regs_map == NULL) {
				pr_err("alloc mem failed\n");
				return -ENOMEM;
			}
		}
		reg = para->xml_regs_map;
		memcpy(reg->lnsd.reg_map,
		cf->lens.lens[pindex->lens_index].export + 2,
	   (LENS_MAX - 2) * sizeof(unsigned int));
	}

	i = 0;
	if (cf->nr_valid == 1) {
		while (i < cf->nr.sum) {
			if (cf->nr.nr[i].export[0] == width &&
			cf->nr.nr[i].export[1] == height)
				break;
			i++;
		}
		if (i < cf->nr.sum)
			pindex->nr_index = i;
		else {
			pr_info("width:%x,height:%x no match nr param\n",
		    width, height);
			pindex->nr_index = 0;
		}
		/** init nr **/
		if (para->xml_regs_map == NULL) {
			para->xml_regs_map =
			vmalloc(sizeof(struct xml_default_regs_s));
			if (para->xml_regs_map == NULL) {
				pr_err("alloc mem failed\n");
				return -ENOMEM;
			}
		}
		reg = para->xml_regs_map;
		memcpy(reg->nr.reg_map, cf->nr.nr[pindex->nr_index].export + 2,
		       (NR_MAX - 2) * sizeof(unsigned int));
	}

	i = 0;
	if (cf->peaking_valid == 1) {
		while (i < cf->peaking.sum) {
			if (cf->peaking.peaking[i].export[0] == width &&
				cf->peaking.peaking[i].export[1] == height)
				break;
			i++;
		}
		if (i < cf->peaking.sum)
			pindex->peaking_index = i;
		else {
			pr_info("width:%x,height:%x no match peaking param\n",
			width, height);
			pindex->peaking_index = 0;
		}
		/** init sharp **/
		if (para->xml_regs_map == NULL) {
			para->xml_regs_map =
			vmalloc(sizeof(struct xml_default_regs_s));
			if (para->xml_regs_map == NULL) {
				pr_err("alloc mem failed\n");
				return -ENOMEM;
			}
		}
		reg = para->xml_regs_map;
		memcpy(reg->sharp.reg_map,
		       cf->peaking.peaking[pindex->peaking_index].export + 2,
		       (PEAKING_MAX - 2) * sizeof(unsigned int));
	}
	return 0;
}

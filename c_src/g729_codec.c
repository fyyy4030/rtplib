/*
 * Copyright (c) 2008-2012 Peter Lemenkov <lemenkov@gmail.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * * Neither the name of the authors nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "erl_driver.h"
#include <bcg729/decoder.h>
#include <bcg729/encoder.h>

typedef struct {
	ErlDrvPort port;
	bcg729EncoderChannelContextStruct* estate;
	bcg729DecoderChannelContextStruct* dstate;
} codec_data;

enum {
	CMD_ENCODE = 1,
	CMD_DECODE = 2
};

static ErlDrvData codec_drv_start(ErlDrvPort port, char *buff)
{
	codec_data* d = (codec_data*)driver_alloc(sizeof(codec_data));
	d->port = port;
	d->estate = initBcg729EncoderChannel();
	d->dstate = initBcg729DecoderChannel();
	set_port_control_flags(port, PORT_CONTROL_FLAG_BINARY);
	return (ErlDrvData)d;
}

static void codec_drv_stop(ErlDrvData handle)
{
	codec_data *d = (codec_data *) handle;
	closeBcg729EncoderChannel(d->estate);
	closeBcg729DecoderChannel(d->dstate);
	driver_free((char*)handle);
}

static int codec_drv_control(
		ErlDrvData handle,
		unsigned int command,
		char *buf, int len,
		char **rbuf, int rlen)
{
	codec_data* d = (codec_data*)handle;

	int ret = 0;
	ErlDrvBinary *out;
	*rbuf = NULL;

	switch(command) {
		case CMD_ENCODE:
			switch(len){
				case 320: // 20 msec
					out = driver_alloc_binary(20); // 2*80 bits
					bcg729Encoder(d->estate, (int16_t*)buf, (uint8_t*)out->orig_bytes);
					bcg729Encoder(d->estate, (int16_t*)buf+160, (uint8_t*)out->orig_bytes+10);
					ret = 20;
					break;
				case 160: // 10 msec
					out = driver_alloc_binary(10); // 80 bits
					bcg729Encoder(d->estate, (int16_t*)buf, (uint8_t*)out->orig_bytes);
					ret = 10;
					break;
				default:
					break;
			}
			*rbuf = (char *) out;
			break;
		 case CMD_DECODE:
			switch(len){
				case 10: // 10 msec
					out = driver_alloc_binary(160);
					bcg729Decoder(d->dstate, (uint8_t*)buf, 0, (int16_t*)out->orig_bytes);
					ret = 160;
					break;
				case 20: // 20 msec
					out = driver_alloc_binary(320);
					bcg729Decoder(d->dstate, (uint8_t*)buf, 0, (int16_t*)out->orig_bytes);
					bcg729Decoder(d->dstate, (uint8_t*)buf+10, 0, (int16_t*)out->orig_bytes+160);
					ret = 320;
					break;
				default:
					break;
			}
			*rbuf = (char *) out;
			break;
		 default:
			break;
	}
	return ret;
}

ErlDrvEntry codec_driver_entry = {
	NULL,			/* F_PTR init, N/A */
	codec_drv_start,	/* L_PTR start, called when port is opened */
	codec_drv_stop,		/* F_PTR stop, called when port is closed */
	NULL,			/* F_PTR output, called when erlang has sent */
	NULL,			/* F_PTR ready_input, called when input descriptor ready */
	NULL,			/* F_PTR ready_output, called when output descriptor ready */
	"g729_codec_drv",		/* char *driver_name, the argument to open_port */
	NULL,			/* F_PTR finish, called when unloaded */
	NULL,			/* handle */
	codec_drv_control,	/* F_PTR control, port_command callback */
	NULL,			/* F_PTR timeout, reserved */
	NULL,			/* F_PTR outputv, reserved */
	NULL,
	NULL,
	NULL,
	NULL,
	ERL_DRV_EXTENDED_MARKER,
	ERL_DRV_EXTENDED_MAJOR_VERSION,
	ERL_DRV_EXTENDED_MINOR_VERSION,
	0,
	NULL,
	NULL,
	NULL
};

DRIVER_INIT(codec_drv) /* must match name in driver_entry */
{
	return &codec_driver_entry;
}

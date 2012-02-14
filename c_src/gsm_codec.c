/* Loosely based on Evgeniy Khramtsov's original approach - erlrtp */

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "erl_driver.h"
#include <spandsp/telephony.h>
#include <spandsp/bit_operations.h>
#include <spandsp/gsm0610.h>

typedef struct {
	ErlDrvPort port;
	gsm0610_state_t* dstate;
	gsm0610_state_t* estate;
} codec_data;

enum {
	CMD_ENCODE = 1,
	CMD_DECODE = 2
};

#define FRAME_SIZE 160
#define GSM_SIZE 33

static ErlDrvData codec_drv_start(ErlDrvPort port, char *buff)
{
	codec_data* d = (codec_data*)driver_alloc(sizeof(codec_data));
	d->port = port;
	d->dstate = gsm0610_init(NULL, GSM0610_PACKING_VOIP);
	d->estate = gsm0610_init(NULL, GSM0610_PACKING_VOIP);
	set_port_control_flags(port, PORT_CONTROL_FLAG_BINARY);
	return (ErlDrvData)d;
}

static void codec_drv_stop(ErlDrvData handle)
{
	codec_data *d = (codec_data *) handle;
	gsm0610_free(d->dstate);
	gsm0610_free(d->estate);
	driver_free((char*)handle);
}

static int codec_drv_control(
		ErlDrvData handle,
		unsigned int command,
		char *buf, int len,
		char **rbuf, int rlen)
{
	codec_data* d = (codec_data*)handle;
	int16_t sample[FRAME_SIZE];

	int i;
	int ret = 0;
	ErlDrvBinary *out;
	*rbuf = NULL;


	switch(command) {
		case CMD_ENCODE:
			if (len != FRAME_SIZE * 2)
				break;
			out = driver_alloc_binary(GSM_SIZE);
			ret = gsm0610_encode(d->estate, (uint8_t*)out->orig_bytes, (const int16_t*)buf, len >> 1);
			*rbuf = (char *)out;
			break;
		 case CMD_DECODE:
			if (len != GSM_SIZE)
				break;
			gsm0610_decode(d->dstate, sample, (const uint8_t*)buf, len);
			out = driver_alloc_binary(FRAME_SIZE * 2);
			for (i = 0; i < FRAME_SIZE; i++){
				out->orig_bytes[i * 2] = (char) (sample[i] & 0xff);
				out->orig_bytes[i * 2 + 1] = (char) (sample[i] >> 8);
			}
			*rbuf = (char *)out;
			ret = FRAME_SIZE * 2;
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
	"gsm_codec_drv",	/* char *driver_name, the argument to open_port */
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

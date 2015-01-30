#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libusb-1.0/libusb.h>
#include <time.h>
#include <sys/time.h>
#include <memory.h>
/*
**    (c) xof, may 2007, 2015
**    This program is free software; you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or
**    (at your option) any later version.
**
**    This program is distributed in the hope that it will be useful,
**    but WITHOUT ANY WARRANTY; without even the implied warranty of
**    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**    GNU General Public License for more details.
**
*/
/*
** This program interacts with an USB/CCID device
** to read Belgian eid cards and write the data
** into a HTML file (with a JPEG picture)
** It is more an exercice in using libusb than anything else...
** (eid-viewer is a much more interesting program...)
*/
typedef unsigned char uchar;

libusb_context *uctxp;
libusb_device_handle *udevp = NULL;
#ifdef DETROP
void xdump(unsigned char *ucp, int len)
	{
	int	i;

	for (i = 0; i < len; i++)
		fprintf(stderr, "%02x ", *ucp++);
	fprintf(stderr, "\n");
	}
void adump(unsigned char *ucp, int len)
	{
	int	i;

	for (i = 0; i < len; i++)
		fprintf(stderr, "%c", *ucp++);
	fprintf(stderr, "\n");
	}
#endif

int init()
	{
	int	x;
	struct libusb_device *udp;
        struct libusb_version *uvp;

        if ((x = libusb_init(&uctxp)) != LIBUSB_SUCCESS)
                {
                fprintf(stderr, "libusb_init() returns %d\n", x);
                return(-1);
                }
	if ((udevp = libusb_open_device_with_vid_pid(uctxp, 0x1a44, 0x0870)) == NULL)
		{
		fprintf(stderr, "libusb_open_device_with_vid_pid(1a44:0870) failed\n");
		perror("open");
		fprintf(stderr, "No reader or probably needs > sudo (or lsusb; chmod/chown)\n");
		return(-1);
		}
	if ((udp = libusb_get_device(udevp)) == NULL)
		{
		fprintf(stderr, "Can't get_device()\n");
		perror("libusb_get_device()\n");
		return(-1);
		}
	if ((x = libusb_claim_interface(udevp, 0)) != 0)
		{
		fprintf(stderr, "libusb_claim_interface() failed. error=%d=%s\n", x, libusb_error_name(x));
		fprintf(stderr, "if BUSY, > sudo /etc/init.d/pcscd stop\n");
		return(-1);
		}
	return(0);
	}
finish()
	{
	if (udevp)
		libusb_close(udevp);
	libusb_exit(uctxp);
	}
char *ICCstatus_str[] = { "ICC present & active", "ICC present & inactive", "No ICC"};
int ICC_status()
	{
	/* PC_to_RDR_GetSlotStatus */
	uchar bufout[] = {0x65, 0x00,0x00,0x00,0x00, 0x00, 0x01, 0x00,0x00,0x00};
	/* RDR_to_PC_SlotStatus */
	uchar bufin[10], bStatus, bError;
	int	n;

	if (libusb_bulk_transfer(udevp, 0x01, bufout, sizeof(bufout), &n, 1000) < 0)
		{
		fprintf(stderr, "bulk-out failed\n");
		perror("PC_to_RDR_GetSlotStatus");
		return;
		}
	if (libusb_bulk_transfer(udevp, 0x82, bufin, sizeof(bufin), &n, 1000) < 0)
		{
		fprintf(stderr, "bulk-in failed\n");
		perror("RDR_to_PC_SlotStatus");
		return;
		}
//	fprintf(stderr, "Checking presence ok: ");
	bStatus = bufin[7]; bError = bufin[8];
	fprintf(stderr, "bStatus = 0x%02x; bError = 0%02x\n", bStatus, bError);
	if (bStatus & (3<<6))
		{ /* error */
		fprintf(stderr, "bStatus error\n");
		}
	else	{
//		fprintf(stderr, "ICCstatus : %s\n", ICCstatus_str[bStatus&3]); 
		}
	return(bStatus&3);
	}
int ICC_power(int onoff) /* 1 On - 0 Off */
	{
	/* PC_to_RDR_IccPowerOn */
	uchar bufout[] = {0x62, 0x00,0x00,0x00,0x00, 0x00, 0x01, 0x00,0x00,0x00};
	/* RDR_to_PC_DataBlock  */
	uchar bufin[10], bStatus, bError;
	int	n;

fprintf(stderr, "ICC_power_on(%d)\n", onoff);
	bufout[0] = (onoff) ? 0x62 : 0x63;
	if (libusb_bulk_transfer(udevp, 0x01, bufout, sizeof(bufout), &n, 1000) < 0)
		{
		fprintf(stderr, "bulk-out failed\n");
		perror("PC_to_RDR_IccPowerOn");
		return;
		}
	if (libusb_bulk_transfer(udevp, 0x82, bufin, sizeof(bufin), &n, 3000) < 0)
		{
		fprintf(stderr, "bulk-in failed\n");
		perror("power-on-response");
		return;
		}
	bStatus = bufin[7]; bError = bufin[8]; /* 6.2.3 */
	fprintf(stderr, "IccPowerOn  ok: bStatus:0x%02x - bError:0x%02x\n", bStatus, bError);
	}
/*---------------------------------------------*/
/*----------- Accessing the card files -----------*/
/* the layers reader/card should be separated... */
int card_status;
int reader_status;
fxdump(FILE *fp, char *txt, uchar *ucp, int len)
    {
    fprintf(fp, "%s : ", txt);
    while (len-- > 0)
        fprintf(fp, "%02x ", *ucp++);
    fprintf(fp, "\n");
    }
eid_error(char *txt)
    {
    perror(txt);
    fprintf(stderr, "reader_status : 0x%02x\n", reader_status);
    fprintf(stderr, "card_status   : 0x%04x\n", card_status);
    return(-1);
    }
#define TIMEOUT 20*100 /*HZ*/
int ACR_xchange(uchar *outp, int out_len, uchar *inp, int in_len)
	{
	int n;
	uchar PC_to_RDR_XfrBloc[] = { 0x6f, 0x00,0x00,0x00,0x00, 0x00, 0x00,0x00,0x00, 0x00}; /* 6.1.4 p30 */
	uchar bufout[256];
	uchar bufin[256];

	memcpy(bufout, PC_to_RDR_XfrBloc, 10);
	memcpy(bufout+10, outp, out_len);
	bufout[1] = out_len; /* little endian */
	if (libusb_bulk_transfer(udevp, 0x01, (char *)bufout, out_len+10, &n, TIMEOUT) < 0)
		{
		fprintf(stderr, "ACR_xchange/libusb_bulk_transfer() failed; n=%d\n", n); perror("libusb_bulk_transfer");
		return(eid_error("usb_bulk_write"));
		}
	if (libusb_bulk_transfer(udevp, 0x82, (char *)bufin, sizeof(bufin), &n, TIMEOUT) < 0)
		return(eid_error("usb_bulk_read"));
	n -= 10; /* skip header */
	if (n > in_len)
		n = in_len; /* secure but should not arrive */
	memcpy(inp, bufin+10, n);
	return(n);
	}
int MCU_select_belpicfile_by_num(int dnum, int fnum)
	{
	const uchar iso_file_select[] = {0x00, 0xA4, 0x08, 0x0c, 0x04, 0xdf, 0x01, 0x00, 0x00, 0x10};
	uchar buf_in[128];
	uchar cmd[32];
	int n;

	memcpy(cmd, iso_file_select, sizeof(iso_file_select));
	cmd[5] = (dnum >> 8) & 0xff;	/* directory */
	cmd[6] = (dnum     ) & 0xff;
	cmd[7] = (fnum >> 8) & 0xff;	/* file */
	cmd[8] = (fnum     ) & 0xff;
	if ((n = ACR_xchange(cmd, 10, buf_in, sizeof(buf_in))) < 0)
		return(eid_error("MCU_file_select"));
//	fxdump(stderr, "MCU_file_select:", buf_in, 4+buf_in[3]);
	card_status = (buf_in[n-2]*256) + buf_in[n-1];
//fprintf(stderr, ">>>>>>>>>>>>card_status %04x <<<<<<<<<<<<<<<<<\n", card_status);
/*
	if (card_status != 0x9000)
		return(eid_error("MCU_select_belpicfile_by_num"));
*/
	return(0);
	}
int MCU_read(int address, uchar *buf, int len)
	{	/* length should be in the range 1..250 */
	uchar iso_read_binary[] = {0x00, 0xb0, 0x00, 0x00, 0xf0};
	uchar buf_in[512];
	uchar cmd[9];
	int n;

	memcpy(cmd, iso_read_binary, sizeof(iso_read_binary));
	cmd[2] = (address >> 8) & 0xff;	/* file offset */
	cmd[3] = (address     ) & 0xff;
	cmd[4] = len;		/* number of bytes to read */

	if ((n = ACR_xchange(cmd, 5, buf_in, sizeof(buf_in))) < 0)
		return(eid_error("MCU_read"));
#ifdef TODO
	if (buf_in[3]==2 && buf_in[4] == 0x6c) /* incomplete read; asking too much */
		{
fprintf(stderr, "Incomplete!\n");
		cmd[4] = buf_in[5]; /* number of available bytes */
		if (ACR_xchange(cmd, 9, buf_in, sizeof(buf_in)) < 0)
			return(eid_error("MCU_read2"));
		}
//	fxdump(stderr, "MCU_READ:", buf_in, 4+buf_in[3]);
#endif
	card_status = (buf_in[n-2]*256) + buf_in[n-1];
	if (card_status != 0x9000 && card_status != 0x6b00)
		return(eid_error("MCU_read"));
	memcpy(buf, buf_in, n);
	return(n);  /* number of bytes read */
	}
/*----------- Printing the result in an .html file -----------*/
char	*id_tags[] = {
	/*0*/	NULL,	/* file structure version */
	/*1*/	"Card number",
	/*2*/	NULL,   /* chip number */
	/*3*/	"Card validity from",
	/*4*/	"Card validity until",
	/*5*/	"Card delivery municipality",
	/*6*/	"National Number",
	/*7*/	"Name",
	/*8*/	"Given names",
	/*9*/	"(given 3)",
	/*10*/	"Nationality",
	/*11*/	"Birth Location",
	/*12*/	"Birth date",
	/*13*/	"Sex",
	/*14*/	"Noble condition",
	/*15*/	"Document type",
	/*16*/	"Special status",
	/*17*/	NULL	/* photo hash */
		};
char	*ad_tags[] = {
	/*0*/	NULL,	/* file structure version */
	/*1*/	"Street + Nr",
	/*2*/	"ZIP code",
	/*3*/	"Municipality"
		};
char *nn_fname(uchar *idp, char *suffix)
	{
	static char fname[20]; /* yymmddaaabb.html */

	while (*idp++ != 6) /* we are looking for tag6 */
		idp += *idp++;  /* skip this one */
	if (*idp++ != 11) /* NN length should be 11 */
		return("noname");
	memcpy(fname, idp, 11);
	strcpy(&fname[11], suffix);
	return(fname);
	}
#define nelem(p) (sizeof(p)/sizeof(p[0]))
html_dump(char *fname, uchar *idp, uchar *adp)
	{
	FILE	*fp;
	int	i;
	uchar	*ucp;

	if ((fp = fopen(fname, "w+")) == NULL)
		return(eid_error(fname));
	fprintf(fp, "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">");
	fprintf(fp, "<html>\n<head>\n");
	fprintf(fp, "  <meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\">\n");
	fprintf(fp, "  <title>Belgian Electronic Id Card (%s)</title>\n", fname);
	fprintf(fp, "</head><body style=\"background-color:rgb(140,208,165)\">\n");
	for (ucp = idp, i = 0; i < nelem(id_tags); i++)
		{
		if (id_tags[*ucp])
			{
			fprintf(fp, "<br><b>%s : </b>", id_tags[*ucp]);
			fwrite(ucp+2, ucp[1], 1, fp);
			}
		else	fxdump(fp, "<br>binary data : ", ucp+2, ucp[1]);
		ucp += ucp[1]+2;	/* skip */
		}
	fprintf(fp, "<hr>");
	for (ucp = adp, i = 0; i < nelem(ad_tags); i++)
		{
		if (ad_tags[*ucp])
			{
			fprintf(fp, "<br><b>%-20s</b>", ad_tags[*ucp]);
			fwrite(ucp+2, ucp[1], 1, fp);
			}
		ucp += ucp[1]+2;	/* skip */
		}
	fprintf(fp, "<hr><center><img src=\"%s\">\n", nn_fname(idp, ".jpg"));
	fprintf(fp, "</body>\n</html>\n");
	if (fclose(fp) != 0)
		return(eid_error(fname));
	return(0);
	}
jpeg_dump(char *fname, uchar *ucp, int size)
	{
	FILE	*fp;

	if ((fp = fopen(fname, "w+")) == NULL)
		return(eid_error(fname));
	if (fwrite(ucp, size, 1, fp) != 1)
		return(eid_error(fname));
	if (fclose(fp) != 0)
		return(eid_error(fname));
	}
/*----------- Doing the job -----------*/
int BEID_dump()
	{
	int	n, ofs = 0;
	uchar	buf_id[1024], buf_ad[256];
	uchar	buf_pic[5120];

	/*
	** get Identity file
	*/
	fprintf(stderr, "Reading data.\n");
	if (MCU_select_belpicfile_by_num(0xdf01, 0x4031) < 0) /* Id data */
		return(eid_error("selecting Id file"));
	for (ofs = 0; (n = MCU_read(ofs, &buf_id[ofs], 0xf0)) > 2; ofs += n)
		continue; /* partial read == EOF */
	if (n < 0 && card_status != 0x6b00)
		return(eid_error("reading Id file"));
	/*
	** get Address file
	*/
	fprintf(stderr, "Reading address.\n");
	if (MCU_select_belpicfile_by_num(0xdf01, 0x4033) < 0) /* Address */
		return(eid_error("selecting Address file"));
	if (MCU_read(0, buf_ad, 0xf0) < 0) /* enough : file length is 0x75 */
		return(eid_error("reading Address file"));
	html_dump(nn_fname(buf_id, ".html"), buf_id, buf_ad);
	fprintf(stderr, "Writing %s.\n", nn_fname(buf_id, ".html"));
	/*
	** get the photo
	*/
	fprintf(stderr, "Reading picture.");
	if (MCU_select_belpicfile_by_num(0xdf01, 0x4035) < 0) /* Photo */
		return(eid_error("selecting Photo file"));
	for (ofs = 0; (n = MCU_read(ofs, &buf_pic[ofs], 0xf0)) > 2; ofs += n)
		{
		fprintf(stderr, ".");
		ofs -= 2;
		continue;
		}
	fprintf(stderr, "\n");
	if (n < 0 && card_status != 0x6b00)
		return(eid_error("reading Photo file"));

	fprintf(stderr, "Writing %s.\n", nn_fname(buf_id, ".jpg"));
	jpeg_dump(nn_fname(buf_id, ".jpg"), buf_pic, ofs);
	return(0);
	}
/*----------------------------------------------------------------*/
main()
	{
	int	x;

	if (init() < 0)
		exit(-1);

	if ((x = ICC_status()) == 2)
		{
		libusb_handle_events(uctxp);
		}
	while((x = ICC_status()) == 2)
		{ /* no card present */
		fprintf(stderr, "Insert BEID card, please.\n");
		sleep(1);
		}
	if (x == 1)
		{ /* card present C& inactive */
		ICC_power(1);
		while (ICC_status())
			{
			fprintf(stderr, "Waiting card ready.\n");
			sleep(1);
			}
		BEID_dump(udevp);
		ICC_power(0);
		}

	finish();
	}	

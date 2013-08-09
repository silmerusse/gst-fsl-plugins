/*
 * Copyright (c) 2005-2012, Freescale Semiconductor, Inc. 
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Module Name:    mfw_gst_vorbisdec.c
 *
 * Description:    Gstreamer plugin for Ogg Vorbis decoder
                   capable of decoding Vorbis raw data.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 *
 * Mar. 9th 2010 Lyon Wang
 * - Inital version.
 *
 */
     /*===============================================================================
                                 INCLUDE FILES
     =============================================================================*/

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <string.h>
#include "oggvorbis_dec_api.h"
#include "mfw_gst_vorbisdec.h"
#include <gst/audio/multichannel.h>
#include "mfw_gst_utils.h"


     /*=============================================================================
                                 LOCAL CONSTANTS
     =============================================================================*/
#define MULT_FACTOR 4

#define TIMESTAMP_DIFFRENCE_MAX_IN_NS 200000000
#define TIMESTAMP_DIFF_MAX_IN_NS 400000000

#define CHANNEL_SIZE 8192*2 // each channel max 8192 sample * 2 byte/sample
#define CHUNK_SIZE 8192*6*2 //max 8192 sample * max channel * sizeof(short)



     /*=============================================================================
                     LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
     =============================================================================*/

     static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE("sink",
                                        GST_PAD_SINK,
                                        GST_PAD_ALWAYS,
                                        GST_STATIC_CAPS
                                        ("audio/x-vorbis")
                                         );
     static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE("src",
                                       GST_PAD_SRC,
                                       GST_PAD_ALWAYS,
                                       GST_STATIC_CAPS("audio/x-raw-int, \
                                         " "endianness   = (gint)    " G_STRINGIFY(G_BYTE_ORDER) ",\
                                         " "signed   = (boolean) true, \
                                         " "width = (gint) 16,\
                                         " "depth = (gint) 16, \
                                         " "rate =   (gint) {8000, 11025, 12000, 16000,\
                                         22050, 24000, 32000,44100, 48000, 64000, 88200, 96000 }"));


     /*=============================================================================
                                     LOCAL MACROS
     =============================================================================*/
#define LONG_BOUNDARY 4
#define GST_CAT_DEFAULT    mfw_gst_vorbisdec_debug

#define VORBISD_INPUT_BUFFER_SIZE (8*1024)
#define BS_BUF_SIZE VORBISD_INPUT_BUFFER_SIZE
#define MAX_ENC_BUF_SIZE 400*BS_BUF_SIZE
#define LONG_BOUNDARY 4

#define INFORMATION_HEADER  0x01
#define COMMENT_HEADER      0x03
#define CODEC_SETUP_HEADER  0x05
#define CHANS   6

 int sampling_frequency[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 0, 0, 0, 0};
 unsigned int lastGoodBufferFullness,lastGoodBufferFullness,firstBufferFullness;

#define	GST_TAG_MFW_VORBIS_CHANNELS		    "channels"
#define GST_TAG_MFW_VORBIS_SAMPLING_RATE	"sampling_frequency"

     /*=============================================================================
                             STATIC FUNCTION PROTOTYPES
     =============================================================================*/
     GST_DEBUG_CATEGORY_STATIC(mfw_gst_vorbisdec_debug);
     static void mfw_gst_vorbisdec_class_init(MFW_GST_VORBISDEC_CLASS_T *
                           klass);
     static void mfw_gst_vorbisdec_base_init(MFW_GST_VORBISDEC_CLASS_T *
                          klass);
     static void mfw_gst_vorbisdec_init(MFW_GST_VORBISDEC_INFO_T *
                         vordec_info);
     static void mfw_gst_vorbisdec_set_property(GObject * object,
                             guint prop_id,
                             const GValue * value,
                             GParamSpec * pspec);
     static void mfw_gst_vorbisdec_get_property(GObject * object,
                             guint prop_id, GValue * value,
                             GParamSpec * pspec);
     static gboolean mfw_gst_vorbisdec_set_caps(GstPad * pad, GstCaps * caps);
     static GstFlowReturn mfw_gst_vorbisdec_chain(GstPad * pad,
                               GstBuffer * buf);
     static gboolean mfw_gst_vorbisdec_sink_event(GstPad *, GstEvent *);
     static gboolean mfw_gst_vorbisdec_src_event(GstPad *, GstEvent *);
     static gboolean plugin_init(GstPlugin * plugin);
     static gboolean mfw_gst_vorbisdec_convert(GstPad * pad,
                                GstFormat src_format,
                                gint64 src_value,
                                GstFormat * dest_format,
                                gint64 * dest_value);
     static gboolean mfw_gst_vorbisdec_src_query(GstPad *, GstQuery *);
     static const GstQueryType *mfw_gst_vorbisdec_get_query_types(GstPad * pad);
     static gboolean mfw_gst_vorbisdec_mem_flush(MFW_GST_VORBISDEC_INFO_T *);
      void *vorbisd_alloc_fast(gint size);
/*=============================================================================
                            STATIC VARIABLES
=============================================================================*/
static GstElementClass *parent_class_vorbis = NULL;
static MFW_GST_VORBISDEC_INFO_T *vorbisdec_global_ptr = NULL;

/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/


#define COPY_BLOCK_TIMESTAMP(des, src) \
    do { \
        des->buflen = src->buflen; \
        des->timestamp = src->timestamp; \
    }while(0)

void init_tsmanager(Timestamp_Manager *tm)
{
    memset(tm, 0, sizeof(Timestamp_Manager));
}

void deinit_tsmanager(Timestamp_Manager *tm)
{
    if (tm->allocatedbuffer){
        g_free(tm->allocatedbuffer);
    }
    memset(tm, 0, sizeof(Timestamp_Manager));
}

void clear_tsmanager(Timestamp_Manager *tm)
{
    int i;
    Block_Timestamp * bt = tm->allocatedbuffer;
    tm->freelist = tm->head = tm->tail = NULL;
    for (i=0;i<tm->allocatednum;i++){
        bt->next = tm->freelist;
        tm->freelist = bt;
        bt++;
    }
}

Block_Timestamp * new_block_timestamp(Timestamp_Manager *tm)
{
    Block_Timestamp * newbuffer;
    if (tm->freelist){
        newbuffer = tm->freelist;
        tm->freelist = newbuffer->next;
        return newbuffer;
    }
    if (tm->allocatednum)
        tm->allocatednum <<=1;
    else
        tm->allocatednum = 4;
    if (newbuffer=g_malloc(sizeof(Block_Timestamp)*tm->allocatednum)) {
        Block_Timestamp *oldhead, *nb;
        int i = 0;

        oldhead = tm->head;
        nb = newbuffer;
        tm->freelist = tm->head = tm->tail = NULL;
        for (i=0;i<(tm->allocatednum-1);i++){
            if (oldhead){
                COPY_BLOCK_TIMESTAMP(nb, oldhead);
                nb->next = NULL;
                if (tm->tail){
                    (tm->tail)->next = nb;
                    tm->tail = nb;
                }else{
                    tm->head = tm->tail = nb;
                }
                oldhead = oldhead->next;
            }else{
                nb->next = tm->freelist;
                tm->freelist = nb;
            }
            nb++;
        }
        if (tm->allocatedbuffer){
            g_free(tm->allocatedbuffer);
        }
        tm->allocatedbuffer = newbuffer;
        return nb;
    }else{
        return newbuffer;
    }
}

gboolean push_block_with_timestamp(Timestamp_Manager *tm, guint blen, GstClockTime timestamp)
{
    Block_Timestamp * bt;
    if (bt = new_block_timestamp(tm)){
        bt->buflen = blen;
        bt->timestamp = timestamp;
        bt->next = NULL;
        if (tm->tail){
            (tm->tail)->next = bt;
            tm->tail = bt;
        }else{
            tm->head = tm->tail = bt;
        }
        return TRUE;
    }else{
        return FALSE;
    }
}

GstClockTime get_timestamp_with_length(Timestamp_Manager *tm, guint length)
{
    GstClockTime ts = -1;
    Block_Timestamp * bt = tm->head;
    if (bt){
        ts = bt->timestamp;
        while(length>=bt->buflen){
            length-=bt->buflen;
            if (bt==tm->tail){
                tm->tail=NULL;
            }
            tm->head = bt->next;
            bt->next = tm->freelist;
            tm->freelist = bt;
            bt = tm->head;
            if (!bt) break;
        }
        if (bt){
            bt->buflen-=length;
        }
    }
    return ts;
}

/***************************************************************************
*
*   FUNCTION NAME - vorbisd_free
*
*   DESCRIPTION     This function frees the memory allocated previously
*   ARGUMENTS
*       mem       - memory address to be freed
*
*   RETURN VALUE
*       None
*
***************************************************************************/

static void vorbisd_free(void *mem)
{
    g_free(mem);
    return;
}

/***************************************************************************
*
*   FUNCTION NAME - mfw_gst_vorbisdec_memclean
*
*   DESCRIPTION
*                   This function frees all the memory allocated for the
*                   plugin;
*   ARGUMENTS
*       vordec_info    - pointer to the plugin context
*
*   RETURN VALUE
*       None
*
***************************************************************************/
static void mfw_gst_vorbisdec_memclean(MFW_GST_VORBISDEC_INFO_T *
					vordec_info)
{

    sOggVorbisDecObj *psOVDecObj = NULL;
    GST_DEBUG("in mfw_gst_vorbisdec_memclean");
    psOVDecObj = vordec_info->psOVDecObj;
    if (psOVDecObj != NULL) {
        vorbisd_free(psOVDecObj->decoderbuf);
    	vorbisd_free(psOVDecObj);
	}
    GST_DEBUG("out of mfw_gst_aacdec_memclean");
}

/*=============================================================================
FUNCTION: mfw_gst_vorbisdec_set_property

DESCRIPTION: sets the property of the element

ARGUMENTS PASSED:
        object     - pointer to the elements object
        prop_id    - ID of the property;
        value      - value of the property set by the application
        pspec      - pointer to the attributes of the property

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/
static void
mfw_gst_vorbisdec_set_property(GObject * object, guint prop_id,
                const GValue * value, GParamSpec * pspec)
{

    GST_DEBUG("in mfw_gst_vorbisdec_set_property routine");
    GST_DEBUG("out of mfw_gst_vorbisdec_set_property routine");
}

/*=============================================================================
FUNCTION: mfw_gst_vorbisdec_get_property

DESCRIPTION: gets the property of the element

ARGUMENTS PASSED:
        object     - pointer to the elements object
        prop_id    - ID of the property;
        value      - value of the property got from the application
        pspec      - pointer to the attributes of the property

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/
static void
mfw_gst_vorbisdec_get_property(GObject * object, guint prop_id,
                GValue * value, GParamSpec * pspec)
{
    GST_DEBUG("in mfw_gst_vorbisdec_get_property routine");
    GST_DEBUG("out of mfw_gst_vorbisdec_get_property routine");
}


/***************************************************************************
*
*   FUNCTION NAME - alloc_fast
*
*   DESCRIPTION
*          This function simulates to allocate memory in the internal
*          memory. This function when used by the application should
*          ensure that the memory address returned in always aligned
*          to long boundry.
*
*   ARGUMENTS
*       size              - Size of the memory requested.
*
*   RETURN VALUE
*       base address of the memory chunck allocated.
*
***************************************************************************/
void *vorbisd_alloc_fast(gint size)
{
    void *ptr = NULL;
    ptr = (void *) g_malloc(size + 4);
    ptr =
    (void *) (((long) ptr + (long) (LONG_BOUNDARY - 1)) &
          (long) (~(LONG_BOUNDARY - 1)));
    return ptr;

}


/***************************************************************************
*
*   FUNCTION NAME - mfw_gst_vorbisdec_set_caps_channel_pos
*
*   DESCRIPTION
*          This function simulates to allocate memory in the internal
*          memory. This function when used by the application should
*          ensure that the memory address returned in always aligned
*          to long boundry.
*
*   ARGUMENTS
*       size              - Size of the memory requested.
*
*   RETURN VALUE
*       base address of the memory chunck allocated.
*
***************************************************************************/
void mfw_gst_vorbisdec_set_caps_channel_pos(GstCaps *caps, gint channum)
{
    GValue chanpos = { 0 };
    GValue pos = { 0 };

    g_value_init (&chanpos, GST_TYPE_ARRAY);
    g_value_init (&pos, GST_TYPE_AUDIO_CHANNEL_POSITION);

    if(channum == 1){
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER);
        gst_value_array_append_value (&chanpos, &pos);
    }
    else if (channum == 2) {
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
    }
    else if (channum == 3){ /* FL,FC,FR */
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
    }
    else if (channum == 4){ /* FL,FR,BL,BR */
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
    }
    else if (channum == 5){ /* FL,FC,FR,BL,BR */
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
    }
    else if (channum == 6){ /* FL,FC,FR,BL,BR,LFE */
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_LFE);
        gst_value_array_append_value (&chanpos, &pos);
    }

    gst_structure_set_value (gst_caps_get_structure (caps, 0), "channel-positions", &chanpos);

    g_value_unset (&pos);
    g_value_unset (&chanpos);
}


/***************************************************************************
*
*   FUNCTION NAME - mfw_gst_vorbisdec_data
*
*   DESCRIPTION
*                   This function decodes data in the input buffer and
*                   pushes the decode pcm output to the next element in the
*                   pipeline
*   ARGUMENTS
*       vordec_info    - pointer to the plugin context
*       inbuffsize         - pointer to the input buffer size
*
*   RETURN VALUE
*       TRUE               - decoding is succesful
*       FALSE              - error in decoding
***************************************************************************/
static gint
mfw_gst_vorbisdec_data(MFW_GST_VORBISDEC_INFO_T * vordec_info,
            gint inbuffsize)

{
    gint rc= VORBIS_DATA_OK;
    GstCaps *src_caps = NULL;
    GstCaps *caps = NULL;
    GstBuffer *outbuffer = NULL;
    guint8 *inbuffer = NULL;
    GstFlowReturn res = GST_FLOW_ERROR;
    GstClockTime ts;
    guint64 time_duration = 0;
    sOggVorbisDecObj * psOVDecObj = NULL;
    guint8 *pcmout;
    gint consumelen = 0;
    gint packtype, offset, data_left, i, byte;
    char buffer[6];

    psOVDecObj = vordec_info->psOVDecObj;

    inbuffer = (guint8*)gst_adapter_peek(vordec_info->pAdapter, inbuffsize);

    /* when decode first 3 header packet,in case byte_consumed is
            not equal to packet size, seeking for "vorbis" sync word  */
    if (psOVDecObj->mPacketCount<3){
        offset = 0;
        data_left = inbuffsize;

        while(data_left>0){
            packtype = inbuffer[offset];


            if (packtype == INFORMATION_HEADER ||
                packtype == COMMENT_HEADER ||
                packtype == CODEC_SETUP_HEADER){
                byte = offset+1;
                for (i=0; i<6; i++)
                    buffer[i] = inbuffer[byte++];
                if(memcmp(buffer,"vorbis",6)==0){
                    consumelen += offset;
                    break;
                }
            }
            offset++;
            data_left--;
            if(data_left==0){
                GST_DEBUG("decode head packet error!");
                return -1;
            }
        }
    }
    psOVDecObj->initial_buffer = inbuffer + consumelen;
    psOVDecObj->buffer_length = inbuffsize - consumelen;

    GST_DEBUG("Begin to decode vorbis frame");

    //g_print(RED_STR("PacketCount = %d \n", psOVDecObj->mPacketCount));
    res = gst_pad_alloc_buffer_and_set_caps(vordec_info->srcpad,
        (guint64)0, CHANNEL_SIZE*psOVDecObj->NoOfChannels, GST_PAD_CAPS(vordec_info->srcpad), &outbuffer);

    if (res != GST_FLOW_OK) {
        GST_DEBUG("Error in allocating output buffer");
        return -1;
    }
    pcmout = (guint8 *)GST_BUFFER_DATA(outbuffer);

    psOVDecObj->pcmout = pcmout;
    psOVDecObj->output_length = CHANNEL_SIZE * psOVDecObj->NoOfChannels;

    GST_DEBUG(RED_STR("begin OggVorbisDecode!"));
    rc = OggVorbisDecode(psOVDecObj);
    GST_DEBUG(RED_STR("finish OggVorbisDecode!"));

    GST_DEBUG("return val of decdoe = %d", rc);

    GST_DEBUG(RED_STR("rc= %d, consumlen=%d"), rc, psOVDecObj->byteConsumed);

    if (rc != VORBIS_DATA_OK && rc != VORBIS_HEADER_OK &&
        rc != VORBIS_COMMENT_OK && rc != VORBIS_CODEBOOK_OK ){
        gst_adapter_flush(vordec_info->pAdapter, psOVDecObj->byteConsumed);
        return -1;
    }

    vordec_info->total_sample += psOVDecObj->outNumSamples;
    consumelen += psOVDecObj->byteConsumed;

    GST_BUFFER_SIZE(outbuffer) = psOVDecObj->NoOfChannels * psOVDecObj->outNumSamples * 2;

    time_duration = gst_util_uint64_scale_int(psOVDecObj->outNumSamples, GST_SECOND, psOVDecObj->SampleRate);
    /* The timestamp in nanoseconds     of the data     in the buffer. */
    GST_DEBUG(RED_STR("len = %d, samplerate = %d",psOVDecObj->outNumSamples, psOVDecObj->SampleRate));

    ts = get_timestamp_with_length(&vordec_info->tsMgr, consumelen);
    if (GST_CLOCK_TIME_IS_VALID(ts)){
        //g_print(RED_STR("consumelen = %d time_offset = %lld ts = %lld \n", consumelen, vordec_info->time_offset, ts));
        if ((ts>vordec_info->time_offset) && (ts-vordec_info->time_offset>TIMESTAMP_DIFF_MAX_IN_NS)){
            GST_DEBUG(RED_STR("error timestamp"));
            GST_DEBUG(RED_STR("time_offset=%lld, ts=%lld ",vordec_info->time_offset,ts));
            vordec_info->time_offset = ts;
        }
    }

    GST_BUFFER_TIMESTAMP(outbuffer) = vordec_info->time_offset;
   // g_print("set time:%lld\n", vordec_info->time_offset);
    /* The duration     in nanoseconds of the data in the buffer */
    GST_BUFFER_DURATION(outbuffer) = time_duration;
    /* The offset in the source file of the  beginning of this buffer */
    /*record next timestamp */
    vordec_info->time_offset += time_duration;

    /* Output PCM samples are pushed on to the next element in the pipeline */
    GST_DEBUG(GREEN_STR("gst pad push data: %d", GST_BUFFER_SIZE(outbuffer)));

    if(psOVDecObj->mPacketCount>3){
        res = gst_pad_push(vordec_info->srcpad, outbuffer);

        if (res != GST_FLOW_OK) {
            GST_WARNING("not able to push the data");
            return consumelen;
        }
    }
    return consumelen;
}
/*=============================================================================
FUNCTION: mfw_gst_vorbisdec_chain

DESCRIPTION: Initializing the decoder and calling the actual decoding function

ARGUMENTS PASSED:
        pad     - pointer to pad
        buffer  - pointer to received buffer

RETURN VALUE:
        GST_FLOW_OK     - Frame decoded successfully
        GST_FLOW_ERROR  - Failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/
static GstFlowReturn
mfw_gst_vorbisdec_chain(GstPad * pad, GstBuffer * buf)
{
    GST_DEBUG("in mfw_gst_vorbisdec_chain routine");
    MFW_GST_VORBISDEC_INFO_T *vordec_info;
    GstCaps *caps = NULL;
    GstBuffer *outbuffer = NULL;
    GstFlowReturn result = GST_FLOW_OK;
    guint8 *inbuffer;
    gint inbuffsize;
    gint FileType;
    gboolean ret;
    guint64 time_duration = 0;
    sOggVorbisDecObj * psOVDecObj = NULL;
    gint query_size = 0;
    gint i = 0;
    vordec_info = MFW_GST_VORBISDEC(GST_OBJECT_PARENT(pad));

    vordec_info->buffer_time = GST_BUFFER_TIMESTAMP(buf);

    if (!vordec_info->init_done) {
        psOVDecObj = vordec_info->psOVDecObj;
        if (psOVDecObj->pvOggDecObj != NULL) {
            vorbisd_free(psOVDecObj->pvOggDecObj);
        }
        if (psOVDecObj->decoderbuf != NULL) {
            vorbisd_free(psOVDecObj->decoderbuf);
        }

        if (!GST_CLOCK_TIME_IS_VALID(vordec_info->time_offset)) {
            if (GST_CLOCK_TIME_IS_VALID(vordec_info->buffer_time)) {
                vordec_info->time_offset = vordec_info->buffer_time;
            }
            else {
                /* timestamp of the first buffer is invalid, set time offset to 0 */
                vordec_info->time_offset = 0;
            }
            GST_DEBUG(GREEN_STR("time_offset = %lld", vordec_info->time_offset));
        }
        if (vordec_info->codec_data){
            gst_adapter_push(vordec_info->pAdapter, gst_buffer_ref(vordec_info->codec_data));
            vordec_info->codec_data = NULL;
        }
        gst_adapter_push(vordec_info->pAdapter, buf);
        push_block_with_timestamp(&vordec_info->tsMgr, GST_BUFFER_SIZE(buf),
                                   GST_BUFFER_TIMESTAMP(buf));

        if ((inbuffsize = gst_adapter_available(vordec_info->pAdapter))<BS_BUF_SIZE)
            return GST_FLOW_OK;
        GST_DEBUG(RED_STR("inbuffsize: %d", inbuffsize));

        inbuffer = (guint8*)gst_adapter_peek(vordec_info->pAdapter, inbuffsize);

        GST_DEBUG(RED_STR("inbuffer[0]=0x%x, inbuffer[0]=0x%x", inbuffer[0], inbuffer[1]));

        psOVDecObj->datasource = inbuffer;
        psOVDecObj->buffer_length= inbuffsize;

        query_size = OggVorbisQueryMem(psOVDecObj); //the internal buffer needed for decoder
        psOVDecObj->buf_size = query_size;

        psOVDecObj->pvOggDecObj = (void *)vorbisd_alloc_fast(psOVDecObj->OggDecObjSize);
        if (NULL == psOVDecObj->pvOggDecObj){
            GST_ERROR("error in allocation of internal decoder structure");
            return GST_FLOW_ERROR;
        }
        memset(psOVDecObj->pvOggDecObj,0,psOVDecObj->OggDecObjSize);

        /*decoder internal buffer allocation*/
        psOVDecObj->decoderbuf = (guint8*)vorbisd_alloc_fast(query_size);
        if (NULL ==  psOVDecObj->decoderbuf){
            GST_ERROR("error in allocation of internal Decoder buffer");
            return GST_FLOW_ERROR;
        }
        memset(psOVDecObj->decoderbuf,0,query_size);
        /* Initialization of Vorbis decoder */
        GST_DEBUG(RED_STR("query_size = %d, psOVDecObj = 0x%x", query_size, psOVDecObj));

        OggVorbisDecoderInit(psOVDecObj);

        GST_DEBUG(RED_STR("decoder init finished"));

        vordec_info->number_of_channels = psOVDecObj->NoOfChannels;
        vordec_info->sampling_freq = psOVDecObj->SampleRate;
        vordec_info->bit_rate = psOVDecObj->ave_bitrate;

        caps = gst_caps_new_simple("audio/x-raw-int",
        "endianness", G_TYPE_INT, G_BYTE_ORDER,
        "signed", G_TYPE_BOOLEAN, TRUE,
        "width", G_TYPE_INT, 16,
        "depth", G_TYPE_INT, 16,
        "rate", G_TYPE_INT, psOVDecObj->SampleRate,
        "channels", G_TYPE_INT, psOVDecObj->NoOfChannels,
        NULL);
        mfw_gst_vorbisdec_set_caps_channel_pos(caps, psOVDecObj->NoOfChannels);

        gst_pad_set_caps(vordec_info->srcpad, caps);
        gst_caps_unref(caps);

        GstTagList  *list = gst_tag_list_new();
        gchar  *codec_name = "Vorbis";
        gst_tag_list_add(list,GST_TAG_MERGE_APPEND, GST_TAG_AUDIO_CODEC,
            codec_name,NULL);
        gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_BITRATE,
            (guint)(psOVDecObj->ave_bitrate),NULL);
        gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_MFW_VORBIS_SAMPLING_RATE,
                (guint)vordec_info->sampling_freq,NULL);
        gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_MFW_VORBIS_CHANNELS,
                (guint)vordec_info->number_of_channels,NULL);
        gst_element_found_tags(GST_ELEMENT(vordec_info),list);

        vordec_info->caps_set= TRUE;
        vordec_info->init_done = TRUE;
        return GST_FLOW_OK;
    }
    gst_adapter_push(vordec_info->pAdapter, buf);
    push_block_with_timestamp(&vordec_info->tsMgr, GST_BUFFER_SIZE(buf),
                               GST_BUFFER_TIMESTAMP(buf));

    while ((inbuffsize = gst_adapter_available(vordec_info->pAdapter))>(BS_BUF_SIZE)){
        gint flushlen;
        flushlen = mfw_gst_vorbisdec_data(vordec_info, inbuffsize);

        GST_DEBUG(RED_STR("chain flushlen = %d", flushlen));

        if (flushlen == -1)
            break;
        gst_adapter_flush(vordec_info->pAdapter, flushlen);
    }

    GST_DEBUG("out of mfw_gst_vorbisdec_chain routine");
    return GST_FLOW_OK;

}
/*=============================================================================
FUNCTION:   mfw_gst_vorbisdec_change_state

DESCRIPTION: this function keeps track of different states of pipeline.

ARGUMENTS PASSED:
        element     -   pointer to element
        transition  -   state of the pipeline

RETURN VALUE:
        GST_STATE_CHANGE_FAILURE    - the state change failed
        GST_STATE_CHANGE_SUCCESS    - the state change succeeded
        GST_STATE_CHANGE_ASYNC      - the state change will happen
                                        asynchronously
        GST_STATE_CHANGE_NO_PREROLL - the state change cannot be prerolled

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static GstStateChangeReturn
mfw_gst_vorbisdec_change_state(GstElement * element,
                GstStateChange transition)
{
    MFW_GST_VORBISDEC_INFO_T *vordec_info;
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    sOggVorbisDecObj *psOVDecObj = NULL;

    vordec_info = MFW_GST_VORBISDEC(element);

    GST_DEBUG("in mfw_gst_vorbisdec_change_state routine");
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        /* allocate memory for config structure */
        vordec_info->init_done = FALSE;
        vordec_info->caps_set= FALSE;
        vordec_info->eos = FALSE;
        vordec_info->flow_error = FALSE;
        vordec_info->time_offset = GST_CLOCK_TIME_NONE;
        vordec_info->inbuffer1 = NULL;
        vordec_info->inbuffer2 = NULL;
        vordec_info->corrupt_bs = FALSE;
        vordec_info->codec_data = NULL;

        psOVDecObj = (sOggVorbisDecObj *)vorbisd_alloc_fast(sizeof(sOggVorbisDecObj));
        if (NULL == psOVDecObj){
            GST_ERROR("error in allocation of decoder config structure");
            return GST_STATE_CHANGE_FAILURE;
        }
        vordec_info->psOVDecObj = psOVDecObj;
        memset(psOVDecObj, 0, sizeof(sOggVorbisDecObj));

        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
       gst_tag_register (GST_TAG_MFW_VORBIS_CHANNELS, GST_TAG_FLAG_DECODED,G_TYPE_UINT,
           "number of channels","number of channels", NULL);
       gst_tag_register (GST_TAG_MFW_VORBIS_SAMPLING_RATE, GST_TAG_FLAG_DECODED,G_TYPE_UINT,
           "sampling frequency (Hz)","sampling frequency (Hz)", NULL);

        vordec_info->pAdapter = gst_adapter_new();
        init_tsmanager(&vordec_info->tsMgr);

        break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:

        break;
    default:
        break;
    }

    ret = parent_class_vorbis->change_state(element, transition);

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        break;

    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        break;

    case GST_STATE_CHANGE_PAUSED_TO_READY:
        GST_DEBUG("GST_STATE_CHANGE_PAUSED_TO_READY");

        vordec_info->inbuffer1 = NULL;
        vordec_info->inbuffer2 = NULL;
        mfw_gst_vorbisdec_memclean(vordec_info);

        vordec_info->total_frames = 0;


        gst_adapter_clear(vordec_info->pAdapter);
        g_object_unref(vordec_info->pAdapter);
        deinit_tsmanager(&vordec_info->tsMgr);

        break;

    case GST_STATE_CHANGE_READY_TO_NULL:
        GST_DEBUG("GST_STATE_CHANGE_READY_TO_NULL");
        break;

    default:
        break;

    }

    GST_DEBUG("out of mfw_gst_vorbisdec_change_state routine");

    return ret;

}

/*=============================================================================
FUNCTION: mfw_gst_vorbisdec_get_query_types

DESCRIPTION: gets the different types of query supported by the plugin

ARGUMENTS PASSED:
        pad     - pad on which the function is registered

RETURN VALUE:
        query types ssupported by the plugin

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/
static const GstQueryType *mfw_gst_vorbisdec_get_query_types(GstPad * pad)
{
    static const GstQueryType src_query_types[] = {
    GST_QUERY_POSITION,
    GST_QUERY_DURATION,
    GST_QUERY_CONVERT,
    0
    };

    return src_query_types;
}
/*==================================================================================================

FUNCTION:   mfw_gst_vorbisdec_src_query

DESCRIPTION:    performs query on src pad.

ARGUMENTS PASSED:
        pad     -   pointer to GstPad
        query   -   pointer to GstQuery

RETURN VALUE:
        TRUE    -   success
        FALSE   -   failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/
static gboolean mfw_gst_vorbisdec_src_query(GstPad * pad,
                         GstQuery * query)
{

    gboolean res = TRUE;
    GstPad *peer;

    MFW_GST_VORBISDEC_INFO_T *vordec_info;
    vordec_info = MFW_GST_VORBISDEC(GST_OBJECT_PARENT(pad));

    //g_print(RED_STR(" in gst src query \n"));

    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_DURATION:
    {
        GstPad *peer;
        GST_DEBUG(RED_STR("coming in GST_QUERY_DURATION"));

        if (!(peer = gst_pad_get_peer (vordec_info->sinkpad))) {
            GST_DEBUG ("sink pad %" GST_PTR_FORMAT " is not linked",
                vordec_info->sinkpad);
            goto error;
        }

        res = gst_pad_query (peer, query);
        gst_object_unref (peer);
        if (!res)
             goto error;


        break;
    }
    case GST_QUERY_CONVERT:
    {
        GstFormat src_fmt, dest_fmt;
        gint64 src_val, dest_val;

        GST_DEBUG(RED_STR("coming in GST_QUERY_CONVERT"));

        gst_query_parse_convert(query, &src_fmt, &src_val, &dest_fmt,
                    &dest_val);
        if (!
        (res =
         mfw_gst_vorbisdec_convert(pad, src_fmt, src_val,
                        &dest_fmt, &dest_val)))
        goto error;

        gst_query_set_convert(query, src_fmt, src_val, dest_fmt,
                  dest_val);
        break;
    }
    default:
        res = gst_pad_query_default (pad, query);
        break;
    }
    return res;

  error:
    GST_ERROR("error handling query");
    return FALSE;
}
/*==================================================================================================

FUNCTION:   mfw_gst_vorbisdec_sink_query

DESCRIPTION:    performs query on sink pad.

ARGUMENTS PASSED:
        pad     -   pointer to GstPad
        query   -   pointer to GstQuery

RETURN VALUE:
        TRUE    -   success
        FALSE   -   failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/

static gboolean
mfw_gst_vorbisdec_sink_query (GstPad * pad, GstQuery * query)
{
    MFW_GST_VORBISDEC_INFO_T *vordec_info;
    gboolean res;

    vordec_info = MFW_GST_VORBISDEC(GST_OBJECT_PARENT(pad));

    switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
    {
        GstFormat src_fmt, dest_fmt;
        gint64 src_val, dest_val;

        gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
        if (!(res = mfw_gst_vorbisdec_convert(pad, src_fmt, src_val, &dest_fmt, &dest_val)))
            goto error;
        gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);

        break;
    }
    default:
      res = gst_pad_query_default (pad, query);
      break;
    }

done:
  return res;

  /* ERRORS */
error:
  {
    GST_DEBUG_OBJECT (vordec_info, "error converting value");
    goto done;
  }
}


/*==================================================================================================
FUNCTION:   mfw_gst_vorbisdec_convert

DESCRIPTION:    converts the format of value from src format to destination format on pad .

ARGUMENTS PASSED:
        pad         -   pointer to GstPad
        src_format  -   format of source value
        src_value   -   value of soure
        dest_format -   format of destination value
        dest_value  -   value of destination

RETURN VALUE:
        TRUE    -   sucess
        FALSE   -   failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None

==================================================================================================*/
static gboolean
mfw_gst_vorbisdec_convert(GstPad * pad, GstFormat src_format,
                   gint64 src_value, GstFormat * dest_format,
                   gint64 * dest_value)
{
  gboolean res = TRUE;
  MFW_GST_VORBISDEC_INFO_T *vordec_info;
  guint64 scale = 1;

  if (src_format == *dest_format) {
    *dest_value = src_value;
    return TRUE;
  }
  vordec_info = MFW_GST_VORBISDEC(GST_OBJECT_PARENT(pad));

  GST_DEBUG("in convert src");

  if (!vordec_info->init_done)
    goto no_header;

  if (vordec_info->sinkpad == pad &&
      (src_format == GST_FORMAT_BYTES || *dest_format == GST_FORMAT_BYTES))
    goto no_format;

  switch (src_format) {
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          scale = 2 * vordec_info->number_of_channels; //pcm : 2 bytes
        case GST_FORMAT_DEFAULT:
          *dest_value =
              scale * gst_util_uint64_scale_int (src_value, vordec_info->sampling_freq,
              GST_SECOND);
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_DEFAULT:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          *dest_value = src_value * 2 * vordec_info->number_of_channels;
          break;
        case GST_FORMAT_TIME:
          *dest_value =
              gst_util_uint64_scale_int (src_value, GST_SECOND, vordec_info->sampling_freq);
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_DEFAULT:
          *dest_value = src_value / (sizeof (float) * vordec_info->number_of_channels);
          break;
        case GST_FORMAT_TIME:
          *dest_value = gst_util_uint64_scale_int (src_value, GST_SECOND,
              vordec_info->sampling_freq * 2 * vordec_info->number_of_channels);
          break;
        default:
          res = FALSE;
      }
      break;
    default:
      res = FALSE;
  }
done:

  return res;

  /* ERRORS */
no_header:
  {
    GST_DEBUG ("no header packets received");
    res = FALSE;
    goto done;
  }
no_format:
  {
    GST_DEBUG ("formats unsupported");
    res = FALSE;
    goto done;
  }
}

/*=============================================================================
FUNCTION:   mfw_gst_vorbisdec_src_event

DESCRIPTION: This functions handles the events that triggers the
             source pad of the vorbis decoder  element.

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
RETURN VALUE:
        TRUE       -    if event is sent to src properly
        FALSE      -    if event is not sent to src properly

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean
mfw_gst_vorbisdec_src_event(GstPad * pad, GstEvent * event)
{
  gboolean res = TRUE;
  MFW_GST_VORBISDEC_INFO_T *vordec_info;
  GST_DEBUG("in mfw_gst_vorbisdec_src_event routine");
  vordec_info = MFW_GST_VORBISDEC(GST_OBJECT_PARENT(pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      GstFormat format, tformat;
      gdouble rate;
      GstEvent *real_seek;
      GstSeekFlags flags;
      GstSeekType cur_type, stop_type;
      gint64 cur, stop;
      gint64 tcur, tstop;
      guint32 seqnum;

      gst_event_parse_seek (event, &rate, &format, &flags, &cur_type, &cur,
          &stop_type, &stop);
      seqnum = gst_event_get_seqnum (event);
      gst_event_unref (event);

      /* we have to ask our peer to seek to time here as we know
       * nothing about how to generate a granulepos from the src
       * formats or anything.
       *
       * First bring the requested format to time
       */
      tformat = GST_FORMAT_TIME;
      if (!(res = mfw_gst_vorbisdec_convert (pad, format, cur, &tformat, &tcur)))
        goto convert_error;
      if (!(res = mfw_gst_vorbisdec_convert (pad, format, stop, &tformat, &tstop)))
        goto convert_error;

      /* then seek with time on the peer */
      real_seek = gst_event_new_seek (rate, GST_FORMAT_TIME,
          flags, cur_type, tcur, stop_type, tstop);
      gst_event_set_seqnum (real_seek, seqnum);

      res = gst_pad_push_event (vordec_info->sinkpad, real_seek);
      break;
    }
    default:
      res = gst_pad_push_event (vordec_info->sinkpad, event);
      break;
  }
done:

  return res;

  /* ERRORS */
convert_error:
  {
    GST_DEBUG_OBJECT (vordec_info, "cannot convert start/stop for seek");
    goto done;
  }

}

/*=============================================================================
FUNCTION:   mfw_gst_vorbisdec_sink_event

DESCRIPTION:

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
RETURN VALUE:
        TRUE       -    if event is sent to sink properly
        FALSE      -    if event is not sent to sink properly

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean
mfw_gst_vorbisdec_sink_event(GstPad * pad, GstEvent * event)
{
    gboolean result = TRUE;
    MFW_GST_VORBISDEC_INFO_T *vordec_info;
    GstCaps *src_caps = NULL, *caps = NULL;
    GstBuffer *outbuffer = NULL;
    GstFlowReturn res = GST_FLOW_OK;
    guint8 *inbuffer;
    gint inbuffsize;
    guint64 time_duration = 0;

    vordec_info = MFW_GST_VORBISDEC(GST_OBJECT_PARENT(pad));

    GST_DEBUG("in mfw_gst_vorbisdec_sink_event function");
    switch (GST_EVENT_TYPE(event)) {
        case GST_EVENT_NEWSEGMENT:
        {
            GstFormat format;
            gint64 start, stop, position;
            gint64 nstart, nstop;
            GstEvent *nevent;

            GST_DEBUG("in GST_EVENT_NEWSEGMENT");

            gst_event_parse_new_segment(event, NULL, NULL, &format, &start,
                        &stop, &position);

            if (format == GST_FORMAT_BYTES) {
            format = GST_FORMAT_TIME;


            if (start != 0)
                result =
                mfw_gst_vorbisdec_convert(pad,
                                GST_FORMAT_BYTES,
                                start, &format,
                                &nstart);
            else
                nstart = start;
            if (stop != 0)
                result =
                mfw_gst_vorbisdec_convert(pad,
                                GST_FORMAT_BYTES,
                                stop, &format,
                                &nstop);
            else
                nstop = stop;

            GST_DEBUG(RED_STR("nstart= %ld, nstop = %ld", nstart, nstop));


            nevent =
                gst_event_new_new_segment(FALSE, 1.0, GST_FORMAT_TIME,
                              nstart, nstop, nstart);
            gst_event_unref(event);
            vordec_info->time_offset = (guint64) nstart;
            GST_DEBUG(RED_STR("nstart = %ld", nstart));
            GST_DEBUG(RED_STR("nevent = %s",nevent));

            result =
                gst_pad_push_event(vordec_info->srcpad, nevent);
            if (TRUE != result) {
                GST_ERROR
                ("Error in pushing the event,result  is %d",
                 result);

            }
            } else if (format == GST_FORMAT_TIME) {
            vordec_info->time_offset = (guint64) start;

            result =
                gst_pad_push_event(vordec_info->srcpad, event);
            if (TRUE != result) {
                GST_ERROR
                ("Error in pushing the event,result  is %d",
                 result);

            }
            }


            break;
        }
        case GST_EVENT_EOS:
        {
            GST_DEBUG("Decoder: Got an EOS from Demuxer");

             if (!vordec_info->init_done) {
                 sOggVorbisDecObj *psOVDecObj = NULL;
                 gint query_size = 0;
                 psOVDecObj = vordec_info->psOVDecObj;
                 if (psOVDecObj->pvOggDecObj != NULL) {
                     vorbisd_free(psOVDecObj->pvOggDecObj);
                 }
                 if (psOVDecObj->decoderbuf != NULL) {
                     vorbisd_free(psOVDecObj->decoderbuf);
                 }
                 inbuffsize = gst_adapter_available(vordec_info->pAdapter);
                 GST_DEBUG(RED_STR("inbuffsize: %d", inbuffsize));

                 inbuffer = gst_adapter_peek(vordec_info->pAdapter, inbuffsize);

                 psOVDecObj->datasource = inbuffer;       // here datasource is input buffer for ogg query mem
                 psOVDecObj->buffer_length= inbuffsize;   // here datasource is input buffer for ogg query mem

                 query_size = OggVorbisQueryMem(psOVDecObj); //the internal buffer needed for decoder
                 psOVDecObj->buf_size = query_size;

                 psOVDecObj->pvOggDecObj = (void *)vorbisd_alloc_fast(psOVDecObj->OggDecObjSize);
                 if (NULL == psOVDecObj->pvOggDecObj){
                     GST_ERROR("error in allocation of internal decoder structure");
                     return GST_FLOW_ERROR;
                 }
                 memset(psOVDecObj->pvOggDecObj,0,psOVDecObj->OggDecObjSize);

                 /*decoder internal buffer allocation*/
                 psOVDecObj->decoderbuf = (guint8*)vorbisd_alloc_fast(query_size);
                 if (NULL ==  psOVDecObj->decoderbuf){
                     GST_ERROR("error in allocation of internal Decoder buffer");
                     return GST_FLOW_ERROR;
                 }
                 memset(psOVDecObj->decoderbuf,0,query_size);
                 /* Initialization of Vorbis decoder */
                 GST_DEBUG(RED_STR("begin decoder init"));
                 GST_DEBUG(RED_STR("query_size = %d, psOVDecObj = 0x%x", query_size, psOVDecObj));

                 OggVorbisDecoderInit(psOVDecObj);

                 GST_DEBUG(RED_STR("decoder init finished"));

                 vordec_info->number_of_channels = psOVDecObj->NoOfChannels;
                 vordec_info->sampling_freq= psOVDecObj->SampleRate;

                 caps = gst_caps_new_simple("audio/x-raw-int",
                 "endianness", G_TYPE_INT, G_BYTE_ORDER,
                 "signed", G_TYPE_BOOLEAN, TRUE,
                 "width", G_TYPE_INT, 16,
                 "depth", G_TYPE_INT, 16,
                 "rate", G_TYPE_INT, psOVDecObj->SampleRate,
                 "channels", G_TYPE_INT, psOVDecObj->NoOfChannels,
                 NULL
                 );
                 mfw_gst_vorbisdec_set_caps_channel_pos(caps, psOVDecObj->NoOfChannels);
                 gst_pad_set_caps(vordec_info->srcpad, caps);
                 gst_caps_unref(caps);

                 GstTagList  *list = gst_tag_list_new();
                 gchar  *codec_name = "Vorbis";
                 gst_tag_list_add(list,GST_TAG_MERGE_APPEND, GST_TAG_AUDIO_CODEC,
                     codec_name,NULL);
                 gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_BITRATE,
                     (guint)(psOVDecObj->ave_bitrate),NULL);
                 gst_element_found_tags(GST_ELEMENT(vordec_info),list);

                 vordec_info->caps_set= TRUE;
                 vordec_info->init_done = TRUE;
             }

            vordec_info->eos = TRUE;
            while ((inbuffsize = gst_adapter_available(vordec_info->pAdapter))>0){
                gint flushlen;
                flushlen = mfw_gst_vorbisdec_data(vordec_info, inbuffsize);
                GST_DEBUG(RED_STR("EOS flushlen = %d", flushlen));
                if (flushlen == -1 )
                    break;
                if (flushlen>inbuffsize){
                    flushlen = inbuffsize;
                }
                gst_adapter_flush(vordec_info->pAdapter, flushlen);
            }
            if (result != TRUE) {
                GST_ERROR("Error in decoding the frame");
            }

            result = gst_pad_push_event(vordec_info->srcpad, event);
            if (TRUE != result) {
                GST_ERROR("Error in pushing the event,result  is %d", result);
            }

            break;
        }
        case GST_EVENT_FLUSH_STOP:
        {

            GST_DEBUG("GST_EVENT_FLUSH_STOP");

            gst_adapter_clear(vordec_info->pAdapter);
            clear_tsmanager(&vordec_info->tsMgr);

            result = gst_pad_push_event(vordec_info->srcpad, event);
            if (TRUE != result) {
                GST_ERROR("Error in pushing the event,result is %d", result);
            }
            break;
        }

        case GST_EVENT_FLUSH_START:
        default:
        {
            result = gst_pad_event_default(pad, event);
            break;
        }
    }

    GST_DEBUG("out of mfw_gst_vorbisdec_sink_event");

    return result;
}


/*=============================================================================
FUNCTION:   mfw_gst_vorbisdec_set_caps

DESCRIPTION:    this function handles the link with other plug-ins and used for
                capability negotiation  between pads

ARGUMENTS PASSED:
        pad        -    pointer to GstPad
        caps       -    pointer to GstCaps

RETURN VALUE:
        TRUE       -    if capabilities are set properly
        FALSE      -    if capabilities are not set properly
PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean mfw_gst_vorbisdec_set_caps(GstPad * pad, GstCaps * caps)
{

    MFW_GST_VORBISDEC_INFO_T *vordec_info;
    const gchar *mime;
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    vordec_info = MFW_GST_VORBISDEC(gst_pad_get_parent(pad));
    const GValue *codec_data                = NULL;

    GST_DEBUG("in mfw_gst_vorbisdec_set_caps routine");
    mime = gst_structure_get_name(structure);

    if (strcmp(mime, "audio/x-vorbis") != 0) {
    GST_WARNING
        ("Wrong mimetype %s provided, we only support %s",
         mime, "audio/x-vorbis");
        gst_object_unref(vordec_info);
        return FALSE;
    }

    gst_structure_get_int(structure, "bitrate",
              &vordec_info->bit_rate);

    codec_data = gst_structure_get_value(structure, "codec_data");
    if (codec_data) {
        GST_DEBUG("Get codec data!");
	    vordec_info->codec_data = gst_value_get_buffer(codec_data);
    }

    if (!gst_pad_set_caps(pad, caps)) {
        gst_object_unref(vordec_info);
        return FALSE;
    }

    GST_DEBUG("out of mfw_gst_vorbisdec_set_caps routine");
    gst_object_unref(vordec_info);
    return TRUE;
}

/*=============================================================================
FUNCTION:   mfw_gst_vorbisdec_init

DESCRIPTION:This function creates the pads on the elements and register the
            function pointers which operate on these pads.

ARGUMENTS PASSED:
        pointer the vorbis decoder element handle.

RETURN VALUE:
        None

PRE-CONDITIONS:
        _base_init and _class_init are called

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static void
mfw_gst_vorbisdec_init(MFW_GST_VORBISDEC_INFO_T * vordec_info)
{

    GstElementClass *klass = GST_ELEMENT_GET_CLASS(vordec_info);
    GST_DEBUG("in mfw_gst_vorbisdec_init routine");
    vordec_info->sinkpad =
    gst_pad_new_from_template(gst_element_class_get_pad_template
                  (klass, "sink"), "sink");

    vordec_info->srcpad =
    gst_pad_new_from_template(gst_element_class_get_pad_template
                  (klass, "src"), "src");

    gst_element_add_pad(GST_ELEMENT(vordec_info),
            vordec_info->sinkpad);
    gst_element_add_pad(GST_ELEMENT(vordec_info),
            vordec_info->srcpad);

    gst_pad_set_setcaps_function(vordec_info->sinkpad,
                 mfw_gst_vorbisdec_set_caps);
    gst_pad_set_chain_function(vordec_info->sinkpad,
                   mfw_gst_vorbisdec_chain);

    gst_pad_set_event_function(vordec_info->sinkpad,
                   GST_DEBUG_FUNCPTR
                   (mfw_gst_vorbisdec_sink_event));

    gst_pad_set_query_function (vordec_info->sinkpad,
        GST_DEBUG_FUNCPTR (mfw_gst_vorbisdec_sink_query));

    gst_pad_set_query_type_function(vordec_info->srcpad,
                    GST_DEBUG_FUNCPTR
                    (mfw_gst_vorbisdec_get_query_types));
    gst_pad_set_event_function(vordec_info->srcpad,
                   GST_DEBUG_FUNCPTR
                   (mfw_gst_vorbisdec_src_event));

    gst_pad_set_query_function (vordec_info->srcpad,
        GST_DEBUG_FUNCPTR (mfw_gst_vorbisdec_src_query));

    GST_DEBUG("out of mfw_gst_vorbisdec_init");

#define MFW_GST_VORBIS_PLUGIN VERSION
    PRINT_CORE_VERSION(OggVorbisVerInfo());
    PRINT_PLUGIN_VERSION(MFW_GST_VORBIS_PLUGIN);

}

/*=============================================================================
FUNCTION:   mfw_gst_vorbisdec_class_init

DESCRIPTION:Initialise the class only once (specifying what signals,
            arguments and virtual functions the class has and setting up
            global state)
ARGUMENTS PASSED:
        klass   - pointer to vorbis decoder's element class

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static void
mfw_gst_vorbisdec_class_init(MFW_GST_VORBISDEC_CLASS_T * klass)
{
    GObjectClass *gobject_class = NULL;
    GstElementClass *gstelement_class = NULL;
    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    parent_class_vorbis =
    (GstElementClass *) g_type_class_ref(GST_TYPE_ELEMENT);
    gobject_class->set_property = mfw_gst_vorbisdec_set_property;
    gobject_class->get_property = mfw_gst_vorbisdec_get_property;
    gstelement_class->change_state = mfw_gst_vorbisdec_change_state;
}

/*=============================================================================
FUNCTION:  mfw_gst_vorbisdec_base_init

DESCRIPTION:
            vorbis decoder element details are registered with the plugin during
            _base_init ,This function will initialise the class and child
            class properties during each new child class creation


ARGUMENTS PASSED:
        Klass   -   pointer to vorbis decoder plug-in class

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/

static void
mfw_gst_vorbisdec_base_init(MFW_GST_VORBISDEC_CLASS_T * klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(element_class,
                       gst_static_pad_template_get
                       (&src_factory));
    gst_element_class_add_pad_template(element_class,
                       gst_static_pad_template_get
                       (&sink_factory));
    
    FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "vorbis audio decoder",
        "Codec/Decoder/Audio", "Decode compressed vorbis audio to raw data");
    
}


/*=============================================================================
FUNCTION: mfw_gst_vorbisdec_get_type

DESCRIPTION:    intefaces are initiated in this function.you can register one
                or more interfaces  after having registered the type itself.

ARGUMENTS PASSED:
            None

RETURN VALUE:
                 A numerical value ,which represents the unique identifier of this
            element(vorbisdecoder)

PRE-CONDITIONS:
            None

POST-CONDITIONS:
            None

IMPORTANT NOTES:
            None
=============================================================================*/
GType mfw_gst_vorbisdec_get_type(void)
{
    static GType vorbisdec_type = 0;
    if (!vorbisdec_type) {
    static const GTypeInfo vordec_info = {
        sizeof(MFW_GST_VORBISDEC_CLASS_T),
        (GBaseInitFunc) mfw_gst_vorbisdec_base_init,
        NULL,
        (GClassInitFunc) mfw_gst_vorbisdec_class_init,
        NULL,
        NULL,
        sizeof(MFW_GST_VORBISDEC_INFO_T),
        0,
        (GInstanceInitFunc) mfw_gst_vorbisdec_init,
    };
    vorbisdec_type = g_type_register_static(GST_TYPE_ELEMENT,
                         "MFW_GST_VORBISDEC_INFO_T",
                         &vordec_info,
                         (GTypeFlags) 0);
    }
    GST_DEBUG_CATEGORY_INIT(mfw_gst_vorbisdec_debug, "mfw_vorbisdecoder",
                0, "Freescale's Vorbis Decoder's Log");
    return vorbisdec_type;
}

/*=============================================================================
FUNCTION:   plugin_init

DESCRIPTION:    special function , which is called as soon as the plugin or
                element is loaded and information returned by this function
                will be cached in central registry

ARGUMENTS PASSED:
        plugin     -    pointer to container that contains features loaded
                        from shared object module

RETURN VALUE:
        return TRUE or FALSE depending on whether it loaded initialized any
        dependency correctly

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean plugin_init(GstPlugin * plugin)
{
    return gst_element_register(plugin, "mfw_vorbisdecoder",
                GST_RANK_PRIMARY+2, MFW_GST_TYPE_VORBISDEC);
}


FSL_GST_PLUGIN_DEFINE("vorbisdec", "vorbis audio decoder", plugin_init);


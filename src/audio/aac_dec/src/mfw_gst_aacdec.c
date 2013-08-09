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
 * Module Name:    mfw_gst_aacdec.c
 *
 * Description:    Gstreamer plugin for AAC + LC decoder
                   capable of decoding AAC (with both ADIF and ADTS
                   header)..
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 *
 * May, 6 2009 Dexter JI<b01140@freescale.com>
 * - Add extra ADTS header for compatible with qtdemux.
 *
 */


/*===============================================================================
                            INCLUDE FILES
=============================================================================*/

#include <gst/gst.h>
#ifdef PUSH_MODE
#include <gst/base/gstadapter.h>
#endif
#include <string.h>
#include "aacd_dec_interface.h"
#include "mfw_gst_aacdec.h"
#include <gst/audio/multichannel.h>
#include "mfw_gst_utils.h"

/*=============================================================================
                            LOCAL CONSTANTS
=============================================================================*/
#define MULT_FACTOR 4

/* the below macros are used to calculate the
Bitrate by parsing the ADTS header */
#define SAMPLING_FREQ_IDX_MASk  0x3c
#define BITSPERFRAME_MASK 0x3ffe000
#define ADTS_HEADER_LENGTH 7
#ifdef PUSH_MODE
#define BS_BUF_SIZE AACD_INPUT_BUFFER_SIZE
#define TIMESTAMP_DIFFRENCE_MAX_IN_NS 200000000
#endif
#define MIN_SAMPLE_RATE 8000
#define MAX_SAMPLE_RATE 96000

#define	GST_TAG_MFW_AAC_CHANNELS		"channels"
#define GST_TAG_MFW_AAC_SAMPLING_RATE	"sampling_frequency"

/*=============================================================================
                LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg, " "mpegversion	= (gint){2, 4}"));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, \
                                    " "endianness	= (gint)	" G_STRINGIFY (G_BYTE_ORDER) ",\
                                    " "signed	= (boolean)	true, \
                                    " "width = (gint) 16,\
                                    " "depth = (gint) 16, \
                                    " "rate =	(gint) [8000, 96000]"));


/*=============================================================================
                                LOCAL MACROS
=============================================================================*/



/*=============================================================================
                        STATIC FUNCTION PROTOTYPES
=============================================================================*/
GST_DEBUG_CATEGORY_STATIC (mfw_gst_aacdec_debug);
static void mfw_gst_aacdec_class_init (MFW_GST_AACDEC_CLASS_T * klass);
static void mfw_gst_aacdec_base_init (MFW_GST_AACDEC_CLASS_T * klass);
static void mfw_gst_aacdec_init (MFW_GST_AACDEC_INFO_T * aacdec_info);
static void mfw_gst_aacdec_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void mfw_gst_aacdec_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean mfw_gst_aacdec_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn mfw_gst_aacdec_chain (GstPad * pad, GstBuffer * buf);
static gboolean mfw_gst_aacdec_sink_event (GstPad *, GstEvent *);
static gboolean mfw_gst_aacdec_src_event (GstPad *, GstEvent *);
static gboolean plugin_init (GstPlugin * plugin);
static gboolean mfw_gst_aacdec_seek (MFW_GST_AACDEC_INFO_T *,
    GstPad *, GstEvent *);
static gboolean mfw_gst_aacdec_convert_src (GstPad * pad,
    GstFormat src_format,
    gint64 src_value, GstFormat * dest_format, gint64 * dest_value);
static gboolean mfw_gst_aacdec_convert_sink (GstPad * pad,
    GstFormat src_format,
    gint64 src_value, GstFormat * dest_format, gint64 * dest_value);
static gboolean mfw_gst_aacdec_src_query (GstPad *, GstQuery *);
static const GstQueryType *mfw_gst_aacdec_get_query_types (GstPad * pad);
static gboolean mfw_gst_aacdec_mem_flush (MFW_GST_AACDEC_INFO_T *);

gint App_get_adts_header (AACD_Block_Params * params,
    MFW_GST_AACDEC_INFO_T * aacdec_info);
gulong App_bs_look_bits (gint nbits, MFW_GST_AACDEC_INFO_T * aacdec_info);
gulong App_bs_read_bits (gint nbits, MFW_GST_AACDEC_INFO_T * aacdec_info);
gint App_bs_byte_align (MFW_GST_AACDEC_INFO_T * aacdec_info);
gint App_bs_refill_buffer (MFW_GST_AACDEC_INFO_T * aacdec_info);
void App_bs_readinit (gchar * buf, gint bytes,
    MFW_GST_AACDEC_INFO_T * aacdec_info);
gint App_FindFileType (gint val, MFW_GST_AACDEC_INFO_T * aacdec_info);
gint App_get_prog_config (AACD_ProgConfig * p,
    MFW_GST_AACDEC_INFO_T * aacdec_info);
gint App_get_adif_header (AACD_Block_Params * params,
    MFW_GST_AACDEC_INFO_T * aacdec_info);
int App_init_raw (AACD_Block_Params * params, int channel_config,
    int sampling_frequency);
int ADTSBitrate (AACD_Decoder_info * dec_info,
    MFW_GST_AACDEC_INFO_T * aacdec_info);
void trnsptAdjustBitrate (unsigned int offset, unsigned int frameSize,
    unsigned int bufferFullness, MFW_GST_AACDEC_INFO_T * aacdec_info);
void *aacd_alloc_fast (gint size);

/*=============================================================================
                            STATIC VARIABLES
=============================================================================*/
static GstElementClass *parent_class_aac = NULL;

/*=============================================================================
                            LOCAL FUNCTIONS
=============================================================================*/





/***************************************************************
 *  FUNCTION NAME - App_bs_look_bits
 *
 *  DESCRIPTION
 *      Get the required number of bits from the bit-register
 *
 *  ARGUMENTS
 *      nbits - Number of bits required
 *
 *  RETURN VALUE
 *      Required bits
 *
 **************************************************************/
unsigned long
App_bs_look_bits (int nbits, MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  BitstreamParam *b = &(aacdec_info->app_params.bs_param);
  return b->bit_register;
}


/***************************************************************
 *  FUNCTION NAME - App_FindFileType
 *
 *  DESCRIPTION
 *      Find if the file is of type, ADIF or ADTS
 *
 *  ARGUMENTS
 *      val     -   First 4 bytes in the stream
 *
 *  RETURN VALUE
 *      0 - Success
 *     -1 - Error
 *
 **************************************************************/
int
App_FindFileType (int val, MFW_GST_AACDEC_INFO_T * aacdec_info)
{


  if (val == 0x41444946) {
    aacdec_info->app_params.App_adif_header_present = TRUE;
  } else {
      aacdec_info->app_params.App_adts_header_present = TRUE;
  }
  return (0);
}

int
ADTSBitrate (AACD_Decoder_info * dec_info, MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  AACD_Block_Params *params;
  params = &aacdec_info->app_params.BlockParams;
  unsigned int unSampleRate = sampling_frequency[aacdec_info->SampFreqIdx];
  GST_DEBUG ("Sampling Frequency=%d", unSampleRate);
  const unsigned int unFrameSamples = FRAMESIZE;
  dec_info->aacd_bit_rate =
      ((gfloat) aacdec_info->bitsPerFrame * unSampleRate) / unFrameSamples;
  aacdec_info->bit_rate = dec_info->aacd_bit_rate;

  GST_DEBUG ("Bitrate=%d", dec_info->aacd_bit_rate);
  return 1;
}





/***************************************************************
 *  FUNCTION NAME - App_bs_refill_buffer
 *
 *  DESCRIPTION
 *      Fill the bitstream buffer with new buffer.
 *
 *  ARGUMENTS
 *      None
 *
 *  RETURN VALUE
 *      0  - success
 *      -1 - error
 *
 **************************************************************/
int
App_bs_refill_buffer (MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  BitstreamParam *b = &(aacdec_info->App_bs_param);
  int bytes_to_copy;
  unsigned int len;


  bytes_to_copy = b->bs_end_ext - b->bs_curr_ext;
  if (bytes_to_copy <= 0) {
    if (aacdec_info->bitstream_count <= 0)
      return (-1);
    else {
      len =
          (aacdec_info->bitstream_count >
          BS_BUF_SIZE) ? BS_BUF_SIZE : aacdec_info->bitstream_count;

      b->bs_curr_ext =
          (unsigned char *) (aacdec_info->bitstream_buf +
          aacdec_info->bitstream_buf_index);
      b->bs_end_ext = b->bs_curr_ext + len;
      bytes_to_copy = len;

      aacdec_info->bitstream_buf_index += len;
      aacdec_info->bitstream_count -= len;
      aacdec_info->in_buf_done += len;
//        bytes_supplied = len;

      /* Set only if previous Seeking is done */
      /*
         if (b->bs_seek_needed == 0)
         b->bs_seek_needed = SeekFlag;
       */
    }

  }

  if (bytes_to_copy > INTERNAL_BS_BUFSIZE)
    bytes_to_copy = INTERNAL_BS_BUFSIZE;

  b->bs_curr = aacdec_info->App_ibs_buf;
  memcpy (b->bs_curr, b->bs_curr_ext, bytes_to_copy);
  b->bs_curr_ext += bytes_to_copy;
  b->bs_end = b->bs_curr + bytes_to_copy;

  return 0;

}


/***********************************************************************
 *
 *   FUNCTION NAME - App_bs_readinit
 *
 *   DESCRIPTION
 *      This module initializes the BitStream Parameteres.
 *
 *   ARGUMENTS
 *       buf       -  Buffer from which, data is to be copied
 *                    into internal-buffer and bit-register
 *       bytes     -  Size of the above buffer in bytes.
 *
 *   RETURN VALUE
 *      None
 **********************************************************************/
void
App_bs_readinit (char *buf, int bytes, MFW_GST_AACDEC_INFO_T * aacdec_info)
{

  BitstreamParam *b = &(aacdec_info->app_params.bs_param);
  unsigned int temp;
  int ret;

  b->bs_curr = (unsigned char *) buf;
  b->bs_end = b->bs_curr + bytes;
  b->bs_eof = 0;
  b->bs_seek_needed = 0;
  b->bit_register = 0;
  b->bit_counter = BIT_COUNTER_INIT;

  while (b->bit_counter >= 0) {
    if (b->bs_curr >= b->bs_end) {
      ret = App_bs_refill_buffer (aacdec_info);
      if (ret < 0)
        break;
    }


    temp = *b->bs_curr++;
    b->bit_register = b->bit_register | (temp << b->bit_counter);
    b->bit_counter -= 8;
  }

}

/***************************************************************
 *  FUNCTION NAME - App_bs_read_bits
 *
 *  DESCRIPTION
 *      Reads the given number of bits from the bit-register
 *
 *  ARGUMENTS
 *      nbits - Number of bits required
 *
 *  RETURN VALUE
 *      - Required bits
 *      - -1 in case of end of bitstream
 *
 **************************************************************/
unsigned long
App_bs_read_bits (int nbits, MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  BitstreamParam *b = &(aacdec_info->app_params.bs_param);
  unsigned long temp, temp1, temp_bit_register;
  long temp_bit_counter;
  int ret;

  aacdec_info->app_params.BitsInHeader += nbits;


  temp_bit_counter = b->bit_counter;
  temp_bit_register = b->bit_register;

  /* If more than available bits are requested,
   * return error
   */
  if ((MIN_REQD_BITS - temp_bit_counter) < nbits)
    return 0;


  temp = temp_bit_register >> (32 - nbits);
  temp_bit_register <<= nbits;
  temp_bit_counter += nbits;

  while (temp_bit_counter >= 0) {
    if (b->bs_curr >= b->bs_end) {
      ret = App_bs_refill_buffer (aacdec_info);
      if (ret < 0) {
        b->bit_register = temp_bit_register;
        b->bit_counter = temp_bit_counter;

        return (temp);
      }
    }

    temp1 = *b->bs_curr++;
    temp_bit_register = temp_bit_register | (temp1 << temp_bit_counter);
    temp_bit_counter -= 8;
  }

  b->bit_register = temp_bit_register;
  b->bit_counter = temp_bit_counter;


  return (temp);
}

/*******************************************************************************
 *
 *   FUNCTION NAME - App_bs_byte_align
 *
 *   DESCRIPTION
 *       This function makes the number of bits in the bit register
 *       to be a multiple of 8.
 *
 *   ARGUMENTS
 *       None
 *
 *   RETURN VALUE
 *       number of bits discarded.
*******************************************************************************/
int
App_bs_byte_align (MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  BitstreamParam *b = &(aacdec_info->app_params.bs_param);
  int nbits;

  nbits = MIN_REQD_BITS - b->bit_counter;
  nbits = nbits & 0x7;          /* LSB 3 bits */

  aacdec_info->app_params.BitsInHeader += nbits;


  b->bit_register <<= nbits;
  b->bit_counter += nbits;


  return (nbits);
}


/*******************************************************************************
 *
 *   FUNCTION NAME - App_get_ele_list
 *
 *   DESCRIPTION
 *       Gets a list of elements present in the current data block
 *
 *   ARGUMENTS
 *       p           -  Pointer to array of Elements.
 *       enable_cpe  _  Flag to indicate whether channel paired element is
 *                      present.
 *
 *   RETURN VALUE
 *       None
*******************************************************************************/
static void
App_get_ele_list (AACD_EleList * p, int enable_cpe,
    MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  int i, j;

  for (i = 0, j = p->num_ele; i < j; i++) {
    if (enable_cpe)
      p->ele_is_cpe[i] = App_bs_read_bits (LEN_ELE_IS_CPE, aacdec_info);
    else
      p->ele_is_cpe[i] = 0;
    p->ele_tag[i] = App_bs_read_bits (LEN_TAG, aacdec_info);
  }

}



/*******************************************************************************
 *
 *   FUNCTION NAME - App_get_prog_config
 *
 *   DESCRIPTION
 *       Read the program configuration element from the data block
 *
 *   ARGUMENTS
 *       p           -  Pointer to a structure to store the new program
 *                       configuration.
 *
 *   RETURN VALUE
 *         Success :  0
 *
 *         Error   : -1
 *
*******************************************************************************/
int
App_get_prog_config (AACD_ProgConfig * p, MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  int i, j;


  p->tag = App_bs_read_bits (LEN_TAG, aacdec_info);

  p->profile = App_bs_read_bits (LEN_PROFILE, aacdec_info);
  if (p->profile != 1) {
    return -1;
  }
  p->sampling_rate_idx = App_bs_read_bits (LEN_SAMP_IDX, aacdec_info);
  if (p->sampling_rate_idx >= 0xc) {
    return -1;
  }
  p->front.num_ele = App_bs_read_bits (LEN_NUM_ELE, aacdec_info);
  if (p->front.num_ele > FCHANS) {
    return -1;
  }
  p->side.num_ele = App_bs_read_bits (LEN_NUM_ELE, aacdec_info);
  if (p->side.num_ele > SCHANS) {
    return -1;
  }
  p->back.num_ele = App_bs_read_bits (LEN_NUM_ELE, aacdec_info);
  if (p->back.num_ele > BCHANS) {
    return -1;
  }
  p->lfe.num_ele = App_bs_read_bits (LEN_NUM_LFE, aacdec_info);
  if (p->lfe.num_ele > LCHANS) {
    return -1;
  }
  p->data.num_ele = App_bs_read_bits (LEN_NUM_DAT, aacdec_info);
  p->coupling.num_ele = App_bs_read_bits (LEN_NUM_CCE, aacdec_info);
  if (p->coupling.num_ele > CCHANS) {
    return -1;
  }
  if ((p->mono_mix.present = App_bs_read_bits (LEN_MIX_PRES, aacdec_info)) == 1)
    p->mono_mix.ele_tag = App_bs_read_bits (LEN_TAG, aacdec_info);
  if ((p->stereo_mix.present =
          App_bs_read_bits (LEN_MIX_PRES, aacdec_info)) == 1)
    p->stereo_mix.ele_tag = App_bs_read_bits (LEN_TAG, aacdec_info);
  if ((p->matrix_mix.present =
          App_bs_read_bits (LEN_MIX_PRES, aacdec_info)) == 1) {
    p->matrix_mix.ele_tag = App_bs_read_bits (LEN_MMIX_IDX, aacdec_info);
    p->matrix_mix.pseudo_enab = App_bs_read_bits (LEN_PSUR_ENAB, aacdec_info);
  }
  App_get_ele_list (&p->front, 1, aacdec_info);
  App_get_ele_list (&p->side, 1, aacdec_info);
  App_get_ele_list (&p->back, 1, aacdec_info);
  App_get_ele_list (&p->lfe, 0, aacdec_info);
  App_get_ele_list (&p->data, 0, aacdec_info);
  App_get_ele_list (&p->coupling, 1, aacdec_info);

  App_bs_byte_align (aacdec_info);

  j = App_bs_read_bits (LEN_COMMENT_BYTES, aacdec_info);


  /*
   * The comment bytes are overwritten onto the same location, to
   * save memory.
   */

  for (i = 0; i < j; i++)
    p->comments[0] = App_bs_read_bits (LEN_BYTE, aacdec_info);
  /* null terminator for string */
  p->comments[0] = 0;

  return 0;

}



/*******************************************************************************
 *
 *   FUNCTION NAME - App_get_adif_header
 *
 *   DESCRIPTION
 *       Gets ADIF header from the input bitstream.
 *
 *   ARGUMENTS
 *         params  -  place to store the adif-header data
 *
 *   RETURN VALUE
 *         Success :  1
 *         Error   : -1
*******************************************************************************/
int
App_get_adif_header (AACD_Block_Params * params,
    MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  int i, n, select_status;
  AACD_ProgConfig *tmp_config;
  ADIF_Header temp_adif_header;
  ADIF_Header *p = &(temp_adif_header);

  /* adif header */
  for (i = 0; i < LEN_ADIF_ID; i++)
    p->adif_id[i] = App_bs_read_bits (LEN_BYTE, aacdec_info);
  p->adif_id[i] = 0;            /* null terminated string */

#ifdef UNIX
  /* test for id */
  if (strncmp (p->adif_id, "ADIF", 4) != 0)
    return -1;                  /* bad id */
#else
  /* test for id */
  if (*((unsigned long *) p->adif_id) != *((unsigned long *) "ADIF"))
    return -1;                  /* bad id */
#endif

  /* copyright string */
  if ((p->copy_id_present =
          App_bs_read_bits (LEN_COPYRT_PRES, aacdec_info)) == 1) {
    for (i = 0; i < LEN_COPYRT_ID; i++)
      p->copy_id[i] = (char) App_bs_read_bits (LEN_BYTE, aacdec_info);

    /* null terminated string */
    p->copy_id[i] = 0;
  }
  p->original_copy = App_bs_read_bits (LEN_ORIG, aacdec_info);
  p->home = App_bs_read_bits (LEN_HOME, aacdec_info);
  p->bitstream_type = App_bs_read_bits (LEN_BS_TYPE, aacdec_info);
  p->bitrate = App_bs_read_bits (LEN_BIT_RATE, aacdec_info);

  /* program config elements */
  select_status = -1;
  n = App_bs_read_bits (LEN_NUM_PCE, aacdec_info) + 1;

  tmp_config =
      (AACD_ProgConfig *) aacd_alloc_fast (n * sizeof (AACD_ProgConfig));

  for (i = 0; i < n; i++) {
    tmp_config[i].buffer_fullness =
        (p->bitstream_type == 0) ? App_bs_read_bits (LEN_ADIF_BF,
        aacdec_info) : 0;


    if (App_get_prog_config (&tmp_config[i], aacdec_info)) {
      return -1;
    }

    select_status = 1;
  }

  App_bs_byte_align (aacdec_info);

  /* Fill in the AACD_Block_Params struct now */

  params->num_pce = n;
  params->pce = tmp_config;
  params->BitstreamType = p->bitstream_type;
  params->BitRate = p->bitrate;
  params->ProtectionAbsent = 0;
  return select_status;
}




/*******************************************************************************
 *
 *   FUNCTION NAME - App_get_adts_header
 *
 *   DESCRIPTION
 *       Searches and syncs to ADTS header from the input bitstream. It also
 *       gets the full ADTS header once sync is obtained.
 *
 *   ARGUMENTS
 *       params   - Place to store the header data
 *
 *   RETURN VALUE
 *       Success : 0
 *         Error : -1
            Error:  1   profile is not AAC LC
*******************************************************************************/
int
App_get_adts_header (AACD_Block_Params * params,
    MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  ADTS_Header App_adts_header;
  ADTS_Header *p = &(App_adts_header);

  int bits_used = 0;
  //// bitrate support for adts header - tlsbo79743
  int start_bytes = 0;
  int bits_consumed = 0;
  unsigned int unSampleRate;
  const unsigned int unFrameSamples = 1024;
  unsigned int bufferFullness;
  const unsigned char channelConfig2NCC[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
#ifdef OLD_FORMAT_ADTS_HEADER
  int emphasis;
#endif


  while (1) {
    /*
     * If we have used up more than maximum possible frame for finding
     * the ADTS header, then something is wrong, so exit.
     */
    if (bits_used > (LEN_BYTE * ADTS_FRAME_MAX_SIZE))
      return -1;

    /* ADTS header is assumed to be byte aligned */
    bits_used += App_bs_byte_align (aacdec_info);

#ifdef CRC_CHECK
    /* Add header bits to CRC check */
    UpdateCRCStructBegin ();
#endif

    p->syncword = App_bs_read_bits ((LEN_SYNCWORD - LEN_BYTE), aacdec_info);
    bits_used += LEN_SYNCWORD - LEN_BYTE;

    /* Search for syncword */
    while (p->syncword != ((1 << LEN_SYNCWORD) - 1)) {
      p->syncword =
          ((p->syncword << LEN_BYTE) | App_bs_read_bits (LEN_BYTE,
              aacdec_info));
      p->syncword &= (1 << LEN_SYNCWORD) - 1;
      bits_used += LEN_BYTE;
      /*
       * If we have used up more than maximum possible frame for finding
       * the ADTS header, then something is wrong, so exit.
       */
      if (bits_used > (LEN_BYTE * ADTS_FRAME_MAX_SIZE))
        return -1;
    }

    bits_consumed = bits_used - LEN_SYNCWORD;   // bitrate support for adts header - tlsbo79743

    p->id = App_bs_read_bits (LEN_ID, aacdec_info);
    bits_used += LEN_ID;

    /*
       Disabled version check in allow MPEG2/4 streams
       0 - MPEG4
       1 - MPEG2
       if (!p->id)
       {
       continue;
       }
     */

    p->layer = App_bs_read_bits (LEN_LAYER, aacdec_info);
    bits_used += LEN_LAYER;
    if (p->layer != 0) {
      continue;
    }

    p->protection_abs = App_bs_read_bits (LEN_PROTECT_ABS, aacdec_info);
    bits_used += LEN_PROTECT_ABS;

    p->profile = App_bs_read_bits (LEN_PROFILE, aacdec_info);
    bits_used += LEN_PROFILE;

    /* profile = 0 AAC Main
       profile = 1  AAC LC
       profile = 2 AAC SSR
       profile =3 AAC LTP (MPEG4) */
    if (p->profile != 1) {
      GST_WARNING ("AAC profile is not LC (Profile:%d)", p->profile);
      //  continue;
      aacdec_info->profile_not_support = TRUE;
      return 1;                 //engr125606 return 1 to indicate profile not support
    }

    p->sampling_freq_idx = App_bs_read_bits (LEN_SAMP_IDX, aacdec_info);
    bits_used += LEN_SAMP_IDX;
    if (p->sampling_freq_idx >= 0xc) {
      continue;
    }

    p->private_bit = App_bs_read_bits (LEN_PRIVTE_BIT, aacdec_info);
    bits_used += LEN_PRIVTE_BIT;

    //temp_channel_config = p->channel_config;
    p->channel_config = App_bs_read_bits (LEN_CHANNEL_CONF, aacdec_info);
    bits_used += LEN_CHANNEL_CONF;
    ///* Audio mode has changed, so config has to be built up again */
    //if (temp_channel_config != p->channel_config)
    //ptr->AACD_default_config = 1;
    p->original_copy = App_bs_read_bits (LEN_ORIG, aacdec_info);
    bits_used += LEN_ORIG;

    p->home = App_bs_read_bits (LEN_HOME, aacdec_info);
    bits_used += LEN_HOME;

#ifdef OLD_FORMAT_ADTS_HEADER
    params->Flush_LEN_EMPHASIS_Bits = 0;
    if (p->id == 0) {
      emphasis = App_bs_read_bits (LEN_EMPHASIS, aacdec_info);
      bits_used += LEN_EMPHASIS;
      params->Flush_LEN_EMPHASIS_Bits = 1;
    }
#endif

    p->copyright_id_bit = App_bs_read_bits (LEN_COPYRT_ID_ADTS, aacdec_info);
    bits_used += LEN_COPYRT_ID_ADTS;

    p->copyright_id_start = App_bs_read_bits (LEN_COPYRT_START, aacdec_info);
    bits_used += LEN_COPYRT_START;

    p->frame_length = App_bs_read_bits (LEN_FRAME_LEN, aacdec_info);
    bits_used += LEN_FRAME_LEN;

    p->adts_buffer_fullness =
        App_bs_read_bits (LEN_ADTS_BUF_FULLNESS, aacdec_info);
    bits_used += LEN_ADTS_BUF_FULLNESS;

    p->num_of_rdb = App_bs_read_bits (LEN_NUM_OF_RDB, aacdec_info);
    bits_used += LEN_NUM_OF_RDB;

    /*
       Removed, constraint: num_of_rdb == 0 because we can support more than
       one raw data block in 1 adts frame
       if (p->num_of_rdb != 0)
       {
       continue;
       }
     */

    /*
     * If this point is reached, then the ADTS header has been found and
     * CRC structure can be updated.
     */
#ifdef CRC_CHECK
    /*
     * Finish adding header bits to CRC check. All bits to be CRC
     * protected.
     */
    UpdateCRCStructEnd (0);
#endif
    /*
     * Adjust the received frame length to add the bytes used up to
     * find the ADTS header.
     */
    p->frame_length += (bits_used / LEN_BYTE) - ADTS_FRAME_HEADER_SIZE;

    if (p->protection_abs == 0) {
      p->crc_check = App_bs_read_bits (LEN_CRC, aacdec_info);
      bits_used += LEN_CRC;
    }

    /* Full header successfully obtained, so get out of the search */
    break;
  }

#ifndef OLD_FORMAT_ADTS_HEADER
  bits_used += App_bs_byte_align (aacdec_info);
#else
  if (p->id != 0)               // MPEG-2 style : Emphasis field is absent
  {
    bits_used += App_bs_byte_align (aacdec_info);
  } else                        //MPEG-4 style : Emphasis field is present; cancel its effect
  {
    aacdec_info->app_params.BitsInHeader -= LEN_EMPHASIS;
  }
#endif

  /* Fill in the AACD_Block_Params struct now */

  params->num_pce = 0;
  params->ChannelConfig = p->channel_config;
  params->SamplingFreqIndex = p->sampling_freq_idx;
  params->BitstreamType = (p->adts_buffer_fullness == 0x7ff) ? 1 : 0;



  params->BitRate = 0;          /* Caution ! */

  /* The ADTS stream contains the value of buffer fullness in units
     of 32-bit words. But the application is supposed to deliver this
     in units of bits. Hence we do a left-shift */

  params->BufferFullness = (p->adts_buffer_fullness) << 5;
  params->ProtectionAbsent = p->protection_abs;
  params->CrcCheck = p->crc_check;

  /* Dexter add for test */
  params->frame_length = p->frame_length;

  //ptr->AACD_mc_info.profile = p->profile;
  //ptr->AACD_mc_info.sampling_rate_idx = p->sampling_freq_idx;
  //AACD_infoinit(&(ptr->tbl_ptr_AACD_samp_rate_info[ptr->AACD_mc_info.sampling_rate_idx]), ptr);

  ///////////////////////  bitrate support for adts header - tlsbo79743 /////////////////////////////

  bufferFullness =
      p->adts_buffer_fullness * 32 * channelConfig2NCC[p->channel_config];

  trnsptAdjustBitrate (bits_consumed, 8 * p->frame_length, bufferFullness,
      aacdec_info);

  return 0;

}

void
trnsptAdjustBitrate (unsigned int offset,
    unsigned int frameSize,
    unsigned int bufferFullness, MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  if (!aacdec_info->nFramesReceived)    /* the first time around */
    firstBufferFullness = bufferFullness;
  else if (aacdec_info->nBitsReceived < (1UL << (30 - BITSPERFRAME_SCALE)))
    aacdec_info->nBitsReceived += frameSize + offset;

  if (!offset) {
    lastGoodBufferFullness = bufferFullness;
    /* stop bitrate calculation before we have to do long divisions */
    if (aacdec_info->nFramesReceived
        && aacdec_info->nBitsReceived < (1UL << (30 - BITSPERFRAME_SCALE)))
      aacdec_info->bitsPerFrame =
          ((aacdec_info->nBitsReceived + bufferFullness -
              firstBufferFullness) << BITSPERFRAME_SCALE) /
          aacdec_info->nFramesReceived;
    aacdec_info->nFramesReceived++;
    // fprintf(stderr,"bitsPerFrame = %d (received = %d)\n",h->bitsPerFrame>>BITSPERFRAME_SCALE,h->nFramesReceived);
  } else {
    /* if offset !=0, we skipped frames. Keep bitrate estimate and instead estimate
       number of frames skipped */
    if (aacdec_info->bitsPerFrame) {    /* only if we have a br estimate already */
      unsigned int nFramesSkipped =
          (((offset + bufferFullness -
                  lastGoodBufferFullness) << BITSPERFRAME_SCALE) +
          (aacdec_info->bitsPerFrame >> 1)) / aacdec_info->bitsPerFrame;
      aacdec_info->nFramesReceived += nFramesSkipped;
//            fprintf(stderr,"bitsPerFrame = %d (received = %d, skipped = %d)\n",h->bitsPerFrame>>BITSPERFRAME_SCALE,h->nFramesReceived,nFramesSkipped);
    }
  }
}


#ifdef PUSH_MODE

#define COPY_BLOCK_TIMESTAMP(des, src) \
    do { \
        des->buflen = src->buflen; \
        des->timestamp = src->timestamp; \
    }while(0)

void
init_tsmanager (Timestamp_Manager * tm)
{
  memset (tm, 0, sizeof (Timestamp_Manager));
}

void
deinit_tsmanager (Timestamp_Manager * tm)
{
  if (tm->allocatedbuffer) {
    g_free (tm->allocatedbuffer);
  }
  memset (tm, 0, sizeof (Timestamp_Manager));
}

void
clear_tsmanager (Timestamp_Manager * tm)
{
  int i;
  Block_Timestamp *bt = tm->allocatedbuffer;
  tm->freelist = tm->head = tm->tail = NULL;
  for (i = 0; i < tm->allocatednum; i++) {
    bt->next = tm->freelist;
    tm->freelist = bt;
    bt++;
  }
}

Block_Timestamp *
new_block_timestamp (Timestamp_Manager * tm)
{
  Block_Timestamp *newbuffer;
  if (tm->freelist) {
    newbuffer = tm->freelist;
    tm->freelist = newbuffer->next;
    return newbuffer;
  }
  if (tm->allocatednum)
    tm->allocatednum <<= 1;
  else
    tm->allocatednum = 4;
  if (newbuffer = g_malloc (sizeof (Block_Timestamp) * tm->allocatednum)) {
    Block_Timestamp *oldhead, *nb;
    int i = 0;

    oldhead = tm->head;
    nb = newbuffer;
    tm->freelist = tm->head = tm->tail = NULL;
    for (i = 0; i < (tm->allocatednum - 1); i++) {
      if (oldhead) {
        COPY_BLOCK_TIMESTAMP (nb, oldhead);
        nb->next = NULL;
        if (tm->tail) {
          (tm->tail)->next = nb;
          tm->tail = nb;
        } else {
          tm->head = tm->tail = nb;
        }
        oldhead = oldhead->next;
      } else {
        nb->next = tm->freelist;
        tm->freelist = nb;
      }
      nb++;
    }
    if (tm->allocatedbuffer) {
      g_free (tm->allocatedbuffer);
    }
    tm->allocatedbuffer = newbuffer;
    return nb;
  } else {
    return newbuffer;
  }
}

gboolean
push_block_with_timestamp (Timestamp_Manager * tm, guint blen,
    GstClockTime timestamp)
{
  Block_Timestamp *bt;
  if (bt = new_block_timestamp (tm)) {
    bt->buflen = blen;
    bt->timestamp = timestamp;
    bt->next = NULL;
    if (tm->tail) {
      (tm->tail)->next = bt;
      tm->tail = bt;
    } else {
      tm->head = tm->tail = bt;
    }
    return TRUE;
  } else {
    return FALSE;
  }
}

GstClockTime
get_timestamp_with_length (Timestamp_Manager * tm, guint length)
{
  GstClockTime ts = -1;
  Block_Timestamp *bt = tm->head;
  if (bt) {
    ts = bt->timestamp;
    while (length >= bt->buflen) {
      length -= bt->buflen;
      if (bt == tm->tail) {
        tm->tail = NULL;
      }
      tm->head = bt->next;
      bt->next = tm->freelist;
      tm->freelist = bt;
      bt = tm->head;
      if (!bt)
        break;
    }
    if (bt) {
      bt->buflen -= length;
    }
  }
  return ts;
}

#if 0
guint
get_tsmanager_length (Timestamp_Manager * tm)
{
  guint len = 0;
  Block_Timestamp *bt = tm->head;
  while (bt) {
    len += bt->buflen;
    bt = bt->next;
  }
  return len;
}
#endif

#endif



/*=============================================================================
FUNCTION: mfw_gst_aacdec_set_property

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
mfw_gst_aacdec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{

  GST_DEBUG (" in mfw_gst_aacdec_set_property routine");
  GST_DEBUG (" out of mfw_gst_aacdec_set_property routine");
}

/*=============================================================================
FUNCTION: mfw_gst_aacdec_set_property

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
mfw_gst_aacdec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GST_DEBUG (" in mfw_gst_aacdec_get_property routine");
  GST_DEBUG (" out of mfw_gst_aacdec_get_property routine");

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
void *
aacd_alloc_fast (gint size)
{
#if 1
  char *mem = NULL;
  void *ptr = NULL;
  ptr = (void *) g_malloc (size + LONG_BOUNDARY + 1);
  // g_print(CYAN_STR("alloc %p\n", ptr));

  if (ptr) {
    mem =
        (char *) ((long) ptr + (long) (LONG_BOUNDARY) &
        (long) (~(LONG_BOUNDARY - 1)));
    *(mem - 1) = (char) ((long) mem - (long) ptr);
    memset (mem, 0, size);
  }


  return mem;
#else
  void *ptr = NULL;
  ptr = (void *) g_malloc (size + 4);
  //g_print(CYAN_STR("malloc %p\n", ptr));

  memset (ptr, 0xff, 16);

  ptr =
      (void *) (((long) ptr + (long) (LONG_BOUNDARY - 1)) &
      (long) (~(LONG_BOUNDARY - 1)));
  // g_print(CYAN_STR("true malloc %p\n", ptr));

  return ptr;
#endif
}


/***************************************************************************
*
*   FUNCTION NAME - alloc_slow
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


static void *
aacd_alloc_slow (gint size)
{
#if 1
  return aacd_alloc_fast (size);
#else
  void *ptr = NULL;
  ptr = (void *) g_malloc (size);
  ptr =
      (void *) (((long) ptr + (long) LONG_BOUNDARY - 1) &
      (long) (~(LONG_BOUNDARY - 1)));
  return ptr;
#endif
}

/***************************************************************************
*
*   FUNCTION NAME - aacd_free
*
*   DESCRIPTION     This function frees the memory allocated previously
*   ARGUMENTS
*       mem       - memory address to be freed
*
*   RETURN VALUE
*       None
*
***************************************************************************/
#if 1
static void
aacd_free (void *ptr)
{
  char *mem;
  if (ptr) {
    mem = ptr;
    mem = mem - (*(mem - 1));
    //g_print(CYAN_STR("free %p\n", mem));

    g_free (mem);
    //   g_print(CYAN_STR("after free %p\n", mem));
  }
  return;
}
#else
//static void
void
aacd_free (void *mem)
{
  char *p;
  //p = (char *)mem - 16;
  //g_print(CYAN_STR("free %p\n", mem));
  //printbuf(p, 16);
  if (mem != NULL)
    g_free (mem);
  return;
}
#endif

/***************************************************************************
*
*   FUNCTION NAME - mfw_gst_interleave_samples
*
*   DESCRIPTION
*                   This function interleaves the decoded left and right
*                   channel output PCM samples
*   ARGUMENTS
*       data_ch0    - pointer to the decoded left channel output PCM samples
*       data_ch1    - pointer to the decoded right channel output PCM samples
*       data_out    - pointer to store the interleaved output
*       frameSize   - output frame size
*       channels    - number of output channels
*
*   RETURN VALUE
*       None
*
***************************************************************************/
static void
mfw_gst_interleave_samples (AACD_OutputFmtType * data_ch0,
    AACD_OutputFmtType * data_ch1,
    short *data_out, gint frameSize, gint channels)
{
  gint i;
  AACD_OutputFmtType tmp;

  for (i = 0; i < frameSize; i++) {
    *data_out++ = *data_ch0++;

    if (channels == 2)
      *data_out++ = *data_ch1++;
  }

}

static void
mfw_gst_2channel_32_to_16sample (AACD_OutputFmtType * data_in,
    short *data_out, gint frameSize, gint actualchannel)
{
  gint i;
  short tmp;
  gint loopcnt = frameSize * actualchannel;

  for (i = 0; i < loopcnt; i++) {

    *data_out++ = *data_in++;
  }
}


/***************************************************************************
*
*   FUNCTION NAME - AACD_writeout
*
*   DESCRIPTION
*                   This function processes the output depending on the
*                   number of channels and the stream type
*   ARGUMENTS
*       dec_info    - pointer to decoder output info structure
*       data        - output data
*       mip         - pointer to multi-channel information
*       outbuff     - output buffer
*
*   RETURN VALUE
*       0           - no error in formatting the output
*
***************************************************************************/
#if 0
static gint
AACD_writeout (AACD_Decoder_info * dec_info, AACD_Decoder_Config * dec_config,
    AACD_OutputFmtType data[][AAC_FRAME_SIZE], guint8 * outbuff)
#else
static gint
AACD_writeout (AACD_Decoder_info * dec_info, AACD_Decoder_Config * dec_config,
    AACD_OutputFmtType * data, guint8 * outbuff)
#endif
{

#if 0

  gint i, num_chans, j;
  gint localVar[CHANS];

  num_chans = CHANS;


  if (dec_info->aacd_num_channels == 1) {
    mfw_gst_interleave_samples (&data[0][0], &data[0][0],
        (short *) outbuff, dec_info->aacd_len, 2);
  }

  else if (dec_info->aacd_len != AAC_FRAME_SIZE) {


    /* this is possible for AAC LC test vectors
     * This check is done so that parametric stereo
     * data can be iWritten to file properly
     * in case of aac vectors stereo o/p will be present in
     * data[0] and data[1] whereas in PS it will be in
     * data[0] and data[1]
     */
    j = 0;
    for (i = 0; i < num_chans; i++) {
      if (!(*(dec_config->ch_info[i].present)))
        continue;
      localVar[j] = i;
      j++;
    }

    mfw_gst_interleave_samples (&data[localVar[1]][0],
        &data[localVar[2]][0], (short *) outbuff, dec_info->aacd_len, 2);
  } else {
    mfw_gst_interleave_samples (&data[1][0], &data[2][0],
        (short *) outbuff, dec_info->aacd_len, 2);
  }
#else
  mfw_gst_2channel_32_to_16sample (data, outbuff, dec_info->aacd_len,
      dec_info->aacd_num_channels);
#endif
  return 0;

}

/***************************************************************************
*
*   FUNCTION NAME - mfw_gst_aacdec_data
*
*   DESCRIPTION
*                   This function decodes data in the input buffer and
*                   pushes the decode pcm output to the next element in the
*                   pipeline
*   ARGUMENTS
*       aacdec_info    - pointer to the plugin context
*       inbuffsize         - pointer to the input buffer size
*
*   RETURN VALUE
*       TRUE               - decoding is succesful
*       FALSE              - error in decoding
***************************************************************************/
static gint
mfw_gst_aacdec_data (MFW_GST_AACDEC_INFO_T * aacdec_info, gint inbuffsize)
{

  AACD_RET_TYPE rc;
  GstCaps *src_caps = NULL;
  GstCaps *caps = NULL;
  GstBuffer *outbuffer = NULL;
  guint8 *inbuffer = NULL;
  GstFlowReturn res = GST_FLOW_ERROR;
  guint64 time_duration = 0;
  AACD_Decoder_Config *dec_config = NULL;
  AACD_Decoder_info dec_info;
  memset (&dec_info, 0, sizeof (AACD_Decoder_info));
  AACD_OutputFmtType *outbuf;
  dec_config = aacdec_info->app_params.dec_config;
  GstBuffer *residue = NULL;
  GstClockTime ts;
  gint consumelen = 0;
  guint framesinbuffer = 0;

  inbuffer = gst_adapter_peek (aacdec_info->pAdapter, (guint) inbuffsize);

  /* decoding of ADTS header happens here */
  if (aacdec_info->app_params.App_adts_header_present) {

    App_bs_readinit ((gchar *) inbuffer, inbuffsize, aacdec_info);

    aacdec_info->app_params.BitsInHeader = 0;
    if (App_get_adts_header (&(aacdec_info->app_params.BlockParams),
            aacdec_info) != 0) {
      if (TRUE == aacdec_info->profile_not_support) {
        GST_ERROR ("AAC profile not support");
        consumelen = -1;
        goto bail;
      }

      GST_DEBUG ("No sync found in this buffer");
      consumelen = inbuffsize;
      if (GST_CLOCK_TIME_IS_VALID (aacdec_info->buffer_time))
        aacdec_info->time_offset = aacdec_info->buffer_time;
      aacdec_info->corrupt_bs = FALSE;
      goto bail;
    }


    dec_config->params = &(aacdec_info->app_params.BlockParams);

    /* ENGR00113768 */
    /* Check the frame length in header, if it is wrong, flush 1 byte for next search. */
    if ((dec_config->params->frame_length < 0)
        || (dec_config->params->frame_length > AACD_6CH_FRAME_MAXLEN)) {
      GST_WARNING ("Get the wrong frame length from header(%d)",
          dec_config->params->frame_length);
      consumelen = 1;
      goto bail;
    }

    consumelen = (aacdec_info->app_params.BitsInHeader / 8);
    inbuffsize -= consumelen;
  }
  inbuffer += consumelen;
  GST_DEBUG (" Begin to decode aac frame");
  /* the decoder decodes the encoded data in the input buffer and outputs
     a frame of PCM samples */
  framesinbuffer++;

  src_caps = GST_PAD_CAPS (aacdec_info->srcpad);

  /* multiplication factor is obtained as a multiple of Bytes per
     sample and number of channels */

#if 0
  res =
      gst_pad_alloc_buffer_and_set_caps (aacdec_info->srcpad,
      0,
      CHANS * AAC_FRAME_SIZE *
      sizeof (AACD_OutputFmtType), src_caps, &outbuffer);

  if (res != GST_FLOW_OK) {
    GST_DEBUG ("Error in allocating output buffer");
    outbuffer = NULL;
    consumelen = -1;
    goto bail;
  }
#else
  outbuffer = gst_buffer_new_and_alloc (CHANS * AAC_FRAME_SIZE *
      sizeof (AACD_OutputFmtType));
  if (outbuffer == NULL) {
    aacdec_info->flow_error = TRUE;
    consumelen = -1;
    goto bail;
  }

  gst_buffer_set_caps (outbuffer, src_caps);
#endif
  outbuf = (AACD_OutputFmtType *) GST_BUFFER_DATA (outbuffer);

  rc = aacd_decode_frame (dec_config, &dec_info, outbuf, inbuffer, inbuffsize);
  GST_DEBUG (" return val of decdoe = %d", rc);
  if ((rc != AACD_ERROR_NO_ERROR && rc != AACD_ERROR_EOF)) {
    aacdec_info->error_cnt++;
    if (aacdec_info->error_cnt > 200) { //continue error over 100, assume its flow error
      g_print (RED_STR ("error over 200 times line: %d\n", __LINE__));
      aacdec_info->flow_error = TRUE;
      consumelen = -1;
      goto bail;
    }
    if (rc != AACD_ERROR_INIT
        && aacdec_info->app_params.App_adts_header_present) {
      dec_info.aacd_sampling_frequency = aacdec_info->sampling_freq;
      dec_info.aacd_num_channels = aacdec_info->number_of_channels;
      dec_info.aacd_len = AAC_FRAME_SIZE;
      aacdec_info->corrupt_bs = TRUE;

      GST_INFO("consume len %d %d %d\n", 
        consumelen, gst_adapter_available(aacdec_info->pAdapter), 
        dec_info.BitsInBlock / 8);
      consumelen=gst_adapter_available (aacdec_info->pAdapter);
      memset (outbuf, 0, AAC_FRAME_SIZE * CHANS * sizeof (AACD_OutputFmtType));
    } else {
      aacdec_info->flow_error = TRUE;
      consumelen = -1;
      goto bail;
    }
    GST_ERROR ("Error in decoding the frame error is %d", rc);
    GST_DEBUG ("inbuffsize = %d", inbuffsize);
  }
  else {
    aacdec_info->error_cnt = 0;
  }

  /* engr113747 : sometimes the audio sample rate is not common used, such 
   * as 41000, so there is no corresponding AAC sample rate index. The 
   * sample rate index in codec data buffer may be the nearest one or 
   * default sample rate. Use this nearest or default sample rate will 
   * cause A/V out-sync. For this reason, if the sample rate is originally 
   * from file container, we use the sample rate in container, but not 
   * the nearest or default one. */
  if ((MIN_SAMPLE_RATE <= aacdec_info->sample_rate_in_file) &&
      (MAX_SAMPLE_RATE >= aacdec_info->sample_rate_in_file) &&
      aacdec_info->sample_rate_in_file != 96000
      && aacdec_info->sample_rate_in_file != 88200
      && aacdec_info->sample_rate_in_file != 64000
      && aacdec_info->sample_rate_in_file != 48000
      && aacdec_info->sample_rate_in_file != 44100
      && aacdec_info->sample_rate_in_file != 32000
      && aacdec_info->sample_rate_in_file != 24000
      && aacdec_info->sample_rate_in_file != 22050
      && aacdec_info->sample_rate_in_file != 16000
      && aacdec_info->sample_rate_in_file != 12000
      && aacdec_info->sample_rate_in_file != 11025
      && aacdec_info->sample_rate_in_file != 8000
      && aacdec_info->sample_rate_in_file != 7350) {
    dec_info.aacd_sampling_frequency = aacdec_info->sample_rate_in_file;
  }

  aacdec_info->sampling_freq = dec_info.aacd_sampling_frequency;
  aacdec_info->number_of_channels = dec_info.aacd_num_channels;

  if (dec_info.BitsInBlock < 8)
    g_print ("consumed:%d\n", dec_info.BitsInBlock);
  
  consumelen += (dec_info.BitsInBlock / 8);

  if (consumelen>gst_adapter_available (aacdec_info->pAdapter))
      consumelen=gst_adapter_available (aacdec_info->pAdapter);

  if (*(dec_config->AACD_bno) < 2) {
    get_timestamp_with_length (&aacdec_info->tsMgr, consumelen);
    goto bail;
  }

  if (dec_info.aacd_len != 0 && dec_info.aacd_num_channels != 0) {


    /* capabailites of the src pad are set in accordance with the next osssink
       element in the pipeline */
    if (!aacdec_info->caps_set) {
      GValue chanpos = { 0 };
      GValue pos = { 0 };
      g_value_init (&chanpos, GST_TYPE_ARRAY);
      g_value_init (&pos, GST_TYPE_AUDIO_CHANNEL_POSITION);

      if (dec_info.aacd_num_channels == 1) {
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER);
        gst_value_array_append_value (&chanpos, &pos);
      } else if (dec_info.aacd_num_channels == 2) {
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
      } else if (dec_info.aacd_num_channels == 3) {
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_LFE);
        gst_value_array_append_value (&chanpos, &pos);
      } else if (dec_info.aacd_num_channels == 4) {
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
      } else if (dec_info.aacd_num_channels == 5) {
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER);
        gst_value_array_append_value (&chanpos, &pos);
      } else if (dec_info.aacd_num_channels == 6) {
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_LEFT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER);
        gst_value_array_append_value (&chanpos, &pos);
        g_value_set_enum (&pos, GST_AUDIO_CHANNEL_POSITION_LFE);
        gst_value_array_append_value (&chanpos, &pos);
      }
      g_value_unset (&pos);

      GstTagList *list = gst_tag_list_new ();
      gchar *codec_name;

      codec_name = "AAC decoder";

      gst_tag_list_add (list, GST_TAG_MERGE_APPEND,
          GST_TAG_AUDIO_CODEC, codec_name, NULL);
      gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_BITRATE,
          (guint) aacdec_info->bit_rate, NULL);
      gst_tag_list_add (list, GST_TAG_MERGE_APPEND,
          GST_TAG_MFW_AAC_SAMPLING_RATE,
          (guint) dec_info.aacd_sampling_frequency, NULL);
      gst_tag_list_add (list, GST_TAG_MERGE_APPEND,
          GST_TAG_MFW_AAC_CHANNELS, (guint) dec_info.aacd_num_channels, NULL);

      gst_element_found_tags (GST_ELEMENT (aacdec_info), list);

      aacdec_info->caps_set = TRUE;
      caps = gst_caps_new_simple ("audio/x-raw-int",
          "endianness", G_TYPE_INT,
          G_BYTE_ORDER, "signed",
          G_TYPE_BOOLEAN, TRUE, "width",
          G_TYPE_INT, 16, "depth",
          G_TYPE_INT, 16, "rate",
          G_TYPE_INT,
          dec_info.aacd_sampling_frequency,
          "channels", G_TYPE_INT, dec_info.aacd_num_channels, NULL);
      gst_structure_set_value (gst_caps_get_structure (caps, 0),
          "channel-positions", &chanpos);
      g_value_unset (&chanpos);
      gst_pad_set_caps (aacdec_info->srcpad, caps);

      gst_buffer_set_caps (outbuffer, caps);
      gst_caps_unref (caps);

      if (aacdec_info->app_params.App_adif_header_present) {
        aacdec_info->bit_rate = dec_info.aacd_bit_rate;
        GST_DEBUG (" ADIF FILE Bit Rate =  %d", aacdec_info->bit_rate);
      }


    }
    mfw_gst_2channel_32_to_16sample (outbuf, (short *) outbuf,
        dec_info.aacd_len, dec_info.aacd_num_channels);

    GST_BUFFER_SIZE (outbuffer) =
        dec_info.aacd_num_channels * dec_info.aacd_len * 2;
    if (dec_info.aacd_len == 0 || dec_info.aacd_sampling_frequency == 0)
      time_duration = 0;
    else
      time_duration =
          gst_util_uint64_scale_int (dec_info.aacd_len, GST_SECOND,
          dec_info.aacd_sampling_frequency);

    /* The timestamp in nanoseconds     of the data     in the buffer. */

    ts = get_timestamp_with_length (&aacdec_info->tsMgr, consumelen);
    if (GST_CLOCK_TIME_IS_VALID (ts)) {
      if ((ts > aacdec_info->time_offset)
          && (ts - aacdec_info->time_offset > TIMESTAMP_DIFFRENCE_MAX_IN_NS)) {
        GST_ERROR ("error timestamp");
        aacdec_info->time_offset = ts;
      }
    }

    DEMO_LIVE_CHECK (aacdec_info->demo_mode,
        aacdec_info->time_offset, aacdec_info->srcpad);
    //if (aacdec_info->demo_mode == 2)
    //    return -1;

    GST_BUFFER_TIMESTAMP (outbuffer) = aacdec_info->time_offset;

    /* The duration     in nanoseconds of the data in the buffer */
    GST_BUFFER_DURATION (outbuffer) = time_duration;
    /* The offset in the source file of the     beginning of this buffer */
    GST_BUFFER_OFFSET (outbuffer) = 0;
    /*record next timestamp */
    aacdec_info->time_offset += time_duration;

    /* Output PCM samples are pushed on to the next element in the pipeline */
    res = gst_pad_push (aacdec_info->srcpad, outbuffer);
    outbuffer = NULL;
    if (res != GST_FLOW_OK) {
      GST_WARNING ("not able to push the data");
#if 0
      if (res == GST_FLOW_NOT_LINKED)
        return -1;
      else
        return consumelen;
#else
      goto bail;
#endif
    }

  } else {
    aacdec_info->init_done = FALSE;
    goto bail;
  }

  if (aacdec_info->corrupt_bs) {
    aacdec_info->corrupt_bs = FALSE;
    if (GST_CLOCK_TIME_IS_VALID (aacdec_info->buffer_time))
      aacdec_info->time_offset = aacdec_info->buffer_time;
  }

bail:
  if (outbuffer) {
    gst_buffer_unref (outbuffer);
  }
  return consumelen;

}

/***************************************************************************
*
*   FUNCTION NAME - mfw_gst_aacdec_calc_average_bitrate
*
*   DESCRIPTION
*                   This function calulates the average bitrate by
                    parsing the input stream.
*   ARGUMENTS
*       aacdec_info    - pointer to the plugin context
*
*   RETURN VALUE
*       TRUE               - execution succesful
*       FALSE              - error in execution
***************************************************************************/

static gint
mfw_gst_aacdec_calc_average_bitrate (MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  GstPad *pad = NULL;
  GstPad *peer_pad = NULL;
  GstFormat fmt = GST_FORMAT_BYTES;
  pad = aacdec_info->sinkpad;
  GstBuffer *pullbuffer = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  guint pullsize = 0;
  guint64 offset = 0;
  gfloat avg_bitrate = 0;
  guint64 totalduration = 0;
  guint64 bitrate = 0;
  guint frames = 0;
  guint8 *inbuffer = NULL;
  AACD_Decoder_info dec_info;
  guint temp = 0;
  gint FileType = 0;
  GstClockTime file_duration;
  if (gst_pad_check_pull_range (pad)) {
    if (gst_pad_activate_pull (GST_PAD_PEER (pad), TRUE)) {
      peer_pad = gst_pad_get_peer (aacdec_info->sinkpad);
      gst_pad_query_duration (peer_pad, &fmt, &totalduration);
      gst_object_unref (GST_OBJECT (peer_pad));
      pullsize = 4;
      ret = gst_pad_pull_range (pad, offset, pullsize, &pullbuffer);
      App_bs_readinit ((gchar *) GST_BUFFER_DATA (pullbuffer),
          pullsize, aacdec_info);
      FileType = App_bs_look_bits (32, aacdec_info);
      if (App_FindFileType (FileType, aacdec_info) != 0) {
        GST_ERROR ("Input File is not AAC");
        return -1;
      }
      if (aacdec_info->app_params.App_adif_header_present == TRUE) {
        gst_pad_activate_push (GST_PAD_PEER (pad), TRUE);
        return 0;
      }
      pullsize = ADTS_HEADER_LENGTH;
      while (offset < totalduration) {
        ret = gst_pad_pull_range (pad, offset, pullsize, &pullbuffer);

        /* The sampling frequency index is of length 4 bits
           after 18 bits (i.e bit3 to bit6 of the 3rd byte )
           in the ADTS header */

        inbuffer = GST_BUFFER_DATA (pullbuffer);
        temp = *(inbuffer + 2);
        temp = temp & SAMPLING_FREQ_IDX_MASk;
        temp = temp >> 2;
        aacdec_info->SampFreqIdx = temp;

        /* The Frame Length is of length 13 bits
           after 30 bits (i.e bit7 to bit8 of the 4th byte +
           5th byte complete + bit1 to bit3 of the 6th byte)
           in the ADTS header */

        temp = ((*(inbuffer + 3)) << 24) | ((*(inbuffer + 4)) << 16)
            | ((*(inbuffer + 5)) << 8) | (*(inbuffer + 6));
        temp = temp & BITSPERFRAME_MASK;
        temp = temp >> 13;
        /* If frame length is 0, should quit the while cycle */
        if (temp == 0) {
          GST_DEBUG (" frame length is 0! ");
          break;
        }
        aacdec_info->bitsPerFrame = temp * 8;

        ADTSBitrate (&dec_info, aacdec_info);
        offset += temp;
        frames++;
        bitrate += dec_info.aacd_bit_rate;
      }
    }
    gst_pad_activate_push (GST_PAD_PEER (pad), TRUE);
    avg_bitrate = (gfloat) bitrate / frames;
    aacdec_info->bit_rate = ((avg_bitrate + 500) / 1000) * 1000;
    GST_DEBUG ("avg_bitrate=%d", aacdec_info->bit_rate);
    file_duration =
        gst_util_uint64_scale (totalduration, GST_SECOND * 8,
        aacdec_info->bit_rate);
    GST_DEBUG (" file_duration = %" GST_TIME_FORMAT,
        GST_TIME_ARGS (file_duration));

  }
  return 0;
}

/***************************************************************************
*
*   FUNCTION NAME - mfw_gst_aacdec_memclean
*
*   DESCRIPTION
*                   This function frees all the memory allocated for the
*                   plugin;
*   ARGUMENTS
*       aacdec_info    - pointer to the plugin context
*
*   RETURN VALUE
*       None
*
***************************************************************************/
static void
mfw_gst_aacdec_memclean (MFW_GST_AACDEC_INFO_T * aacdec_info)
{

  AACD_Decoder_Config *dec_config = NULL;
  AACD_Mem_Alloc_Info_Sub *mem = NULL;
  gint nr = 0;
  gint rec_no = 0;
  GST_DEBUG ("in mfw_gst_aacdec_memclean \n");
  dec_config = aacdec_info->app_params.dec_config;
  if (dec_config != NULL) {
    nr = dec_config->aacd_mem_info.aacd_num_reqs;
    for (rec_no = 0; rec_no < nr; rec_no++) {
      mem = &(dec_config->aacd_mem_info.mem_info_sub[rec_no]);

      if (mem->app_base_ptr) {
        aacd_free (mem->app_base_ptr);
        mem->app_base_ptr = 0;
      }

    }
    aacd_free (dec_config);
  }
  GST_DEBUG ("out of mfw_gst_aacdec_memclean");
}

void
hex_print (char *desc, gchar * buf, gint len)
{
  gint i = 0;
  g_print ("%s:dump buffer:%p,len:%d.\n", desc, buf, len);
  for (i = 0; i < len; i++)
    while (i < len) {
      g_print ("0x%0X:", *buf++);
      if ((i & 0xf) == 0xf)
        g_print ("\n");
      i++;
    }
  g_print ("\n");
  return;

}

/******************************************************************************
 * Function:    PutBits
 *
 * Description: Function to fill the number of bits required for the each
                variable.
 *
 * Argumnets:   source
 *              destination
 *
 * Return:      The error code.
 *
 * Notes:
 *****************************************************************************/
gint
PutBits (MFW_GST_AACDEC_INFO_T * aacdec_info, guint8 ** PositionDest,
    guint32 Data, guint32 NumberOfBits)
{
  guint32 dbit;
  guint32 word;
  guint32 bit;
  guint8 *dummyDest;


  GST_DEBUG ("Put data:%x, len:%d.", Data, NumberOfBits);
  /* Hold the Address of the passed Destination source address. */
  if (PositionDest == NULL) {
    dummyDest = NULL;
  } else {
    dummyDest = *PositionDest;
  }

  dbit = 1 << (NumberOfBits - 1);
  word = aacdec_info->bword;
  bit = aacdec_info->bbit;
  while (dbit != 0) {

    if (Data & dbit) {

      word |= bit;
    }

    dbit >>= 1;
    bit >>= 1;

    if (bit == 0) {

      if (dummyDest) {

        *dummyDest++ = (guint8) word;
      }

      word = 0;
      bit = 0x80;
    }
  }

  aacdec_info->bword = word;
  aacdec_info->bbit = bit;

  if (dummyDest) {
    *PositionDest = dummyDest;
    return 0;
  } else {
    return 1;
  }
}

/* Macros for the ADTS Header bit Allocation. */
#define BITS_FOR_SYNCWORD                                                   12
#define BITS_FOR_MPEG_ID                                                     1
#define BITS_FOR_MPEG_LAYER                                                  2
#define BITS_FOR_PROTECTION                                                  1
#define BITS_FOR_PROFILE                                                     2
#define BITS_FOR_SAMPLING_FREQ                                               4
#define BITS_FOR_PRIVATE_BIT                                                 1
#define BITS_FOR_CHANNEL_TYPE                                                3
#define BITS_FOR_ORIGINAL_COPY                                               1
#define BITS_FOR_HOME                                                        1
#define BITS_FOR_COPYRIGHT                                                   1
#define BITS_FOR_COPYRIGHT_START                                             1
#define BITS_FOR_FRAME_LENGTH                                               13
#define BITS_FOR_BUFFER_FULLNESS                                            11

#define EXTRA_AAC_HEADER_LEN 7


gboolean
create_codec_buffer (MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  guint32 val;
  guint8 *data;

  aacdec_info->extra_codec_data =
      gst_buffer_new_and_alloc (EXTRA_AAC_HEADER_LEN);
  if (aacdec_info->extra_codec_data == NULL) {
    GST_ERROR ("Could not alloc memories.");
    return FALSE;
  }
  data = (guint8 *) GST_BUFFER_DATA (aacdec_info->codec_data);

  val = (data[1] >> 7) & 1;

  aacdec_info->SampFreqIdx = (((data[0] << 5) | (val << 4)) >> 4) & 15;

  aacdec_info->number_of_channels = (data[1] & 120) >> 3;

  /* engr113747 : the sample rate is given by container, save this sample rate */
  {
    /* since the 'rate' field in sink pad caps is  same to the sample 
     * rate in container, we just parse and use it. */
    gint sampleRate;
    GstCaps *caps = GST_PAD_CAPS (aacdec_info->sinkpad);
    GstStructure *structure = gst_caps_get_structure (caps, 0);

    gst_structure_get_int (structure, "rate", &sampleRate);
    aacdec_info->sample_rate_in_file = sampleRate;
  }

  GST_INFO ("codec data freq :%d, channels:%d", aacdec_info->SampFreqIdx,
      aacdec_info->number_of_channels);

  return TRUE;


}

GstBuffer *
gen_codec_buffer (MFW_GST_AACDEC_INFO_T * aacdec_info, GstBuffer * buffer)
{
  guint8 *extra_data;
  guint8 *data;
  GstBuffer *newbuf;

  gint err;

  guint32 val;

  gint sample_idx, channel;

  if (!aacdec_info->extra_codec_data)
    return buffer;

  aacdec_info->bbit = 0x80;     /* Byte align for bit stream.              */
  aacdec_info->bword = 0;

  extra_data = (guint8 *) GST_BUFFER_DATA (aacdec_info->extra_codec_data);
  GST_BUFFER_SIZE (aacdec_info->extra_codec_data) = EXTRA_AAC_HEADER_LEN;
  /* Fill the number of bits required fixed and varibale. */
  /* Fill the sync word. */
  err = PutBits (aacdec_info, &extra_data, 0xfff, BITS_FOR_SYNCWORD);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");
  /* Fill the MPEG ID: 0 - MPEG4 1 - MPEG2 */
  err = PutBits (aacdec_info, &extra_data, 0, BITS_FOR_MPEG_ID);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");

  /*  */
  err = PutBits (aacdec_info, &extra_data, 0, BITS_FOR_MPEG_LAYER);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");

  err = PutBits (aacdec_info, &extra_data, 1, BITS_FOR_PROTECTION);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");

  err = PutBits (aacdec_info, &extra_data, 1, BITS_FOR_PROFILE);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");



  err = PutBits (aacdec_info, &extra_data, aacdec_info->SampFreqIdx,
      BITS_FOR_SAMPLING_FREQ);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");

  err = PutBits (aacdec_info, &extra_data, 0, BITS_FOR_PRIVATE_BIT);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");

  err = PutBits (aacdec_info, &extra_data, aacdec_info->number_of_channels,
      BITS_FOR_CHANNEL_TYPE);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");


  err = PutBits (aacdec_info, &extra_data, 0, BITS_FOR_ORIGINAL_COPY);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");

  err = PutBits (aacdec_info, &extra_data, 0, BITS_FOR_HOME);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");

  err = PutBits (aacdec_info, &extra_data, 0, BITS_FOR_COPYRIGHT);

  err = PutBits (aacdec_info, &extra_data, 0, BITS_FOR_COPYRIGHT_START);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");

  err =
      PutBits (aacdec_info, &extra_data, GST_BUFFER_SIZE (buffer),
      BITS_FOR_FRAME_LENGTH);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");

  err = PutBits (aacdec_info, &extra_data, 0x7FF, BITS_FOR_BUFFER_FULLNESS);
  if (err)
    GST_DEBUG ("Could not putbits to AAC extra header");

  /* Byte Align. */
  while (aacdec_info->bbit != 0x80) {
    PutBits (aacdec_info, &extra_data, 0, 1);
    if (err)
      GST_DEBUG ("Could not putbits to AAC extra header");
  }

  newbuf = gst_buffer_copy (aacdec_info->extra_codec_data);
  buffer = gst_buffer_join (newbuf, buffer);

  // hex_print("EXTRA BUFFER",GST_BUFFER_DATA(buffer), MIN(GST_BUFFER_SIZE(buffer), 16));

  return buffer;

}


void
free_extra_codec_data (MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  if (aacdec_info->extra_codec_data)
    gst_buffer_unref (aacdec_info->extra_codec_data);
  return;
}

static GstBuffer *
mfw_gst_aacdec_gen_firstdata (MFW_GST_AACDEC_INFO_T * aacdec_info,
    GstBuffer * buffer)
{
  GValue *value;
  GstBuffer *buf, *newbuf;
  gchar *mime;

  GstCaps *caps = GST_PAD_CAPS (aacdec_info->sinkpad);
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  GST_INFO ("caps:%s", gst_caps_to_string (caps));

  mime = gst_structure_get_name (structure);

  if ((value = gst_structure_get_value (structure, "framed")) &&
      g_value_get_boolean (value) == TRUE) {
    aacdec_info->packetised = TRUE;
    GST_INFO ("This is packetised frame.");
  } else {
    aacdec_info->packetised = FALSE;
  }

  if (gst_structure_has_field (structure, "codec_data")) {
    value = gst_structure_get_value (structure, "codec_data");
    buf = GST_BUFFER_CAST (gst_value_get_mini_object (value));

    aacdec_info->codec_data = gst_buffer_copy (buf);
    /*
       hex_print("codec_data",GST_BUFFER_DATA(aacdec_info->codec_data),
       MIN(GST_BUFFER_SIZE(aacdec_info->codec_data), 64));
     */
    if (aacdec_info->packetised) {
      create_codec_buffer (aacdec_info);
      buffer = gen_codec_buffer (aacdec_info, buffer);
    } else {
      guint32 val;
      guint8 *data;
      data = (guint8 *) GST_BUFFER_DATA (aacdec_info->codec_data);
      val = (data[1] >> 7) & 1;
      aacdec_info->SampFreqIdx = (((data[0] << 5) | (val << 4)) >> 4) & 15;
      aacdec_info->number_of_channels = (data[1] & 120) >> 3;
    }


  } else {
    GST_INFO ("codec_data filed not found");
  }

  return buffer;

}

void
mfw_gst_aacdec_getsampleratechannel (MFW_GST_AACDEC_INFO_T * aacdec_info,
    gint * channel, gint * sampleidx)
{
  gint sampleRate;

  GstCaps *caps = GST_PAD_CAPS (aacdec_info->sinkpad);
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  GST_INFO ("aacdec caps are %" GST_PTR_FORMAT, caps);

  if (TRUE == gst_structure_get_int (structure, "channels", channel) &&
      TRUE == gst_structure_get_int (structure, "rate", &sampleRate)) {

    if (sampleRate == 96000)
      *sampleidx = 0;
    else if (sampleRate == 88200)
      *sampleidx = 1;
    else if (sampleRate == 64000)
      *sampleidx = 2;
    else if (sampleRate == 48000)
      *sampleidx = 3;
    else if (sampleRate == 44100)
      *sampleidx = 4;
    else if (sampleRate == 32000)
      *sampleidx = 5;
    else if (sampleRate == 24000)
      *sampleidx = 6;
    else if (sampleRate == 22050)
      *sampleidx = 7;
    else if (sampleRate == 16000)
      *sampleidx = 8;
    else if (sampleRate == 12000)
      *sampleidx = 9;
    else if (sampleRate == 11025)
      *sampleidx = 10;
    else if (sampleRate == 8000)
      *sampleidx = 11;
    else if (sampleRate == 7350)
      *sampleidx = 12;
    else
      *sampleidx = 13;
  } else {
    *channel = aacdec_info->number_of_channels;
    *sampleidx = aacdec_info->SampFreqIdx;
  }
}


/*=============================================================================
FUNCTION: mfw_gst_aacdec_chain

DESCRIPTION: Initializing the decoder and calling the actual decoding function

ARGUMENTS PASSED:
        pad     - pointer to pad
        buffer  - pointer to received buffer

RETURN VALUE:
        GST_FLOW_OK		- Frame decoded successfully
		GST_FLOW_ERROR	- Failure

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/
static GstFlowReturn
mfw_gst_aacdec_chain (GstPad * pad, GstBuffer * buf)
{


  MFW_GST_AACDEC_INFO_T *aacdec_info;
  AACD_RET_TYPE rc = 0;
  GstCaps *src_caps = NULL, *caps = NULL;
  GstBuffer *outbuffer = NULL;
  GstFlowReturn result = GST_FLOW_OK;
  guint8 *inbuffer;
  gint inbuffsize;
  gint FileType;
  gboolean ret;
  guint64 time_duration = 0;
  AACD_Decoder_Config *dec_config = NULL;
  gint i = 0;
  aacdec_info = MFW_GST_AACDEC (GST_OBJECT_PARENT (pad));

  if (aacdec_info->demo_mode == 2)
    return GST_FLOW_ERROR;



  aacdec_info->buffer_time = GST_BUFFER_TIMESTAMP (buf);


  if (!aacdec_info->init_done) {
    gboolean raw_flag = FALSE;
    if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (buf))) {
      aacdec_info->time_offset = GST_BUFFER_TIMESTAMP (buf);
    }

    dec_config = aacdec_info->app_params.dec_config;
    aacdec_info->inbuffer1 = buf;

    GST_BUFFER_OFFSET (aacdec_info->inbuffer1) = 0;
    aacdec_info->inbuffer1 =
        mfw_gst_aacdec_gen_firstdata (aacdec_info, aacdec_info->inbuffer1);

    inbuffer = GST_BUFFER_DATA (aacdec_info->inbuffer1);
    inbuffsize = GST_BUFFER_SIZE (aacdec_info->inbuffer1);

    /* searching for sync word so that can give correct
       data to the decode library */
    if (aacdec_info->app_params.App_adts_header_present) {
      while (i<inbuffsize) {
        if ((inbuffer[i] == 0xFF) && ((inbuffer[i + 1] == 0xF9)
                || (inbuffer[i + 1] == 0xF1))) {
          break;
        }
        i++;
      }
      if (i >= inbuffsize) {
        aacdec_info->app_params.App_adts_header_present = FALSE;        
      }
      else {    
        inbuffer = inbuffer + i;
        inbuffsize = inbuffsize - i;
      }
    }
    
    GST_BUFFER_OFFSET (aacdec_info->inbuffer1) = 0;

    App_bs_readinit ((gchar *) inbuffer, inbuffsize, aacdec_info);
    FileType = App_bs_look_bits (32, aacdec_info);

    /* determine whether input stream has ADTS or ADIF header */
    if (App_FindFileType (FileType, aacdec_info) != 0) {
      GST_WARNING ("InputFile is no adif & adts header raw bistream");
      /*  neither have adif and adts header,
         then we need to get sample rate and channel config
         from caps, set raw aac bitstream flag on
         below code serve as adif header function */

      dec_config->params = &(aacdec_info->app_params.BlockParams);
      mfw_gst_aacdec_getsampleratechannel (aacdec_info,
          &(dec_config->params->
              ChannelConfig), &(dec_config->params->SamplingFreqIndex));
      dec_config->params->num_pce = 0;
      dec_config->params->BitstreamType = 0;
      dec_config->params->BitRate = 0;  /* Caution ! */
      dec_config->params->BufferFullness = 0;
      dec_config->params->ProtectionAbsent = 1;
      dec_config->params->CrcCheck = 0;
      aacdec_info->app_params.App_adif_header_present = TRUE;
      aacdec_info->app_params.App_adts_header_present = FALSE;
      raw_flag = TRUE;
    }


    /* decoding of ADIF header happens here */
    if (aacdec_info->app_params.App_adif_header_present) {
      GST_INFO ("the input stream is of ADIF format");


      /* ENGR63488: Don't support seek for ADIF streams */
      aacdec_info->seek_flag = FALSE;

      App_bs_readinit ((gchar *) inbuffer, inbuffsize, aacdec_info);
      aacdec_info->app_params.BitsInHeader = 0;
      if (!raw_flag) {
        /* must have adif or adts header  */
        if (App_get_adif_header
            (&(aacdec_info->app_params.BlockParams), aacdec_info) < 0) {

          GError *error = NULL;
          GQuark domain;
          domain = g_quark_from_string ("mfw_aacdecoder");
          error = g_error_new (domain, 10, "fatal error");
          gst_element_post_message (GST_ELEMENT (aacdec_info),
              gst_message_new_error
              (GST_OBJECT (aacdec_info),
                  error,
                  "Error while parsing the ADIF header in the "
                  " AAC LC decoder plug-in"));

        }
      }
      dec_config->params = &(aacdec_info->app_params.BlockParams);
      aacdec_info->bit_rate = dec_config->params->BitRate;

#ifndef PUSH_MODE
      GST_BUFFER_OFFSET (aacdec_info->inbuffer1) +=
          (aacdec_info->app_params.BitsInHeader / 8);
#else
      GST_BUFFER_DATA (aacdec_info->inbuffer1) +=
          (aacdec_info->app_params.BitsInHeader / 8);
#endif
      GST_BUFFER_SIZE (aacdec_info->inbuffer1) -=
          (aacdec_info->app_params.BitsInHeader / 8);

    }
#ifdef PUSH_MODE
    if (GST_BUFFER_SIZE (aacdec_info->inbuffer1) > 0) {
      gst_adapter_push (aacdec_info->pAdapter, aacdec_info->inbuffer1);
      push_block_with_timestamp (&aacdec_info->tsMgr,
          GST_BUFFER_SIZE (buf), GST_BUFFER_TIMESTAMP (buf));
    } else {
      gst_buffer_unref (buf);
    }
#endif

    aacdec_info->init_done = TRUE;
    return GST_FLOW_OK;
  }

  if (aacdec_info->packetised)
    buf = gen_codec_buffer (aacdec_info, buf);
  gst_adapter_push (aacdec_info->pAdapter, buf);
  push_block_with_timestamp (&aacdec_info->tsMgr, GST_BUFFER_SIZE (buf),
      GST_BUFFER_TIMESTAMP (buf));
  while ((inbuffsize = gst_adapter_available (aacdec_info->pAdapter))
      > (BS_BUF_SIZE + ADTS_HEADER_LENGTH) || (aacdec_info->packetised
          && inbuffsize > 0)) {
    gint flushlen;
    flushlen = mfw_gst_aacdec_data (aacdec_info, inbuffsize);

    if (flushlen == 0)
      g_print ("Warning: the flushlen:%d\n");
    if ((flushlen == -1) /*|| (flushlen == 0) */ )
      break;

    gst_adapter_flush (aacdec_info->pAdapter, flushlen);
  }

  if (aacdec_info->profile_not_support) {
    GstFlowReturn ret = GST_FLOW_OK;
    GST_ERROR ("AAC Profile not support");
    ret = gst_pad_push_event (aacdec_info->srcpad, gst_event_new_eos ());
    return ret;
  }

  if (aacdec_info->flow_error) {
    GST_ERROR ("flow error !");
    GError *error = NULL;
    GQuark domain;
    domain = g_quark_from_string ("mfw_aacdecoder");
    error = g_error_new (domain, 10, "fatal error");
    gst_element_post_message (GST_ELEMENT (aacdec_info),
        gst_message_new_error (GST_OBJECT
            (aacdec_info),
            error,
            "Flow error because the parsing frame length is 0 "
            " AAC decoder plug-in"));
    return GST_FLOW_ERROR;
  }
  GST_DEBUG ("out of mfw_gst_aacdec_chain routine");
  return GST_FLOW_OK;

}

/*=============================================================================
FUNCTION:   mfw_gst_aacdec_change_state

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
mfw_gst_aacdec_change_state (GstElement * element, GstStateChange transition)
{
  MFW_GST_AACDEC_INFO_T *aacdec_info;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  gint rec_no = 0;
  gint nr = 0, retval = 0;
  AACD_Decoder_Config *dec_config = NULL;
  AACD_Mem_Alloc_Info_Sub *mem;
  AACD_RET_TYPE rc = 0;
  gboolean res;
  aacdec_info = MFW_GST_AACDEC (element);

  GST_DEBUG ("in mfw_gst_aacdec_change_state routine");
  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      aacdec_info->caps_set = FALSE;
      aacdec_info->init_done = FALSE;
      aacdec_info->eos = FALSE;
      aacdec_info->flow_error = FALSE;
      aacdec_info->time_offset = 0;
      aacdec_info->inbuffer1 = NULL;
      aacdec_info->inbuffer2 = NULL;
      aacdec_info->corrupt_bs = FALSE;
      aacdec_info->error_cnt = 0;
      aacdec_info->profile_not_support = FALSE;

      memset (&aacdec_info->app_params, 0, sizeof (AACD_App_params *));

      /* allocate memory for config structure */
      dec_config = (AACD_Decoder_Config *)
          aacd_alloc_fast (sizeof (AACD_Decoder_Config));
      memset (dec_config, 0, sizeof (AACD_Decoder_Config));
      aacdec_info->app_params.dec_config = dec_config;
      if (dec_config == NULL) {
        GST_ERROR ("error in allocation of decoder config structure");
        return GST_STATE_CHANGE_FAILURE;
      }

      /* call query mem function to know mem requirement of library */
      if (aacd_query_dec_mem (dec_config) != AACD_ERROR_NO_ERROR) {
        GST_ERROR ("Failed to get the memory configuration for the decoder");
        return GST_STATE_CHANGE_FAILURE;
      }

      /* Number of memory chunk requests by the decoder */
      nr = dec_config->aacd_mem_info.aacd_num_reqs;

      for (rec_no = 0; rec_no < nr; rec_no++) {
        mem = &(dec_config->aacd_mem_info.mem_info_sub[rec_no]);
        if (mem->aacd_size == 0) {
          mem->app_base_ptr = NULL;
          continue;
        }
        if (mem->aacd_type == AACD_FAST_MEMORY) {
          mem->app_base_ptr = aacd_alloc_fast (mem->aacd_size);
          if (mem->app_base_ptr == NULL)
            return GST_STATE_CHANGE_FAILURE;
        } else {
          mem->app_base_ptr = aacd_alloc_slow (mem->aacd_size);
          if (mem->app_base_ptr == NULL)
            return GST_STATE_CHANGE_FAILURE;
        }
        memset (dec_config->aacd_mem_info.mem_info_sub[rec_no].
            app_base_ptr, 0,
            dec_config->aacd_mem_info.mem_info_sub[rec_no].aacd_size);

      }

      aacdec_info->app_params.BitsInHeader = 0;
      aacdec_info->app_params.App_adif_header_present = FALSE;
      aacdec_info->app_params.App_adts_header_present = FALSE;

      /* register the call-back function in the decoder context */
      //dec_config->app_swap_buf = app_swap_buffers_aac_dec;

#ifndef OUTPUT_24BITS
      dec_config->num_pcm_bits = AACD_16_BIT_OUTPUT;
#else
      dec_config->num_pcm_bits = AACD_24_BIT_OUTPUT;
#endif  /*OUTPUT_24BITS*/                  /*OUTPUT_24BITS */

      rc = aacd_decoder_init (dec_config);
      if (rc != AACD_ERROR_NO_ERROR) {
        GST_ERROR ("Error in initializing the decoder");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:

      gst_tag_register (GST_TAG_MFW_AAC_CHANNELS, GST_TAG_FLAG_DECODED,
          G_TYPE_UINT, "number of channels", "number of channels", NULL);
      gst_tag_register (GST_TAG_MFW_AAC_SAMPLING_RATE, GST_TAG_FLAG_DECODED,
          G_TYPE_UINT, "sampling frequency (Hz)",
          "sampling frequency (Hz)", NULL);

      aacdec_info->bitsPerFrame = 0;
      aacdec_info->bit_rate = 0;
      aacdec_info->nFramesReceived = 0;
      aacdec_info->bitstream_count = 0;
      aacdec_info->bitstream_buf_index = 0;
      aacdec_info->in_buf_done = 0;
      aacdec_info->nBitsReceived = 0;
      aacdec_info->total_time = 0;
      aacdec_info->seek_flag = FALSE;
      aacdec_info->sample_rate_in_file = 0;

#ifdef PUSH_MODE
      aacdec_info->pAdapter = gst_adapter_new ();
      init_tsmanager (&aacdec_info->tsMgr);
#endif
      break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;

    default:
      break;
  }

  ret = parent_class_aac->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      retval = mfw_gst_aacdec_calc_average_bitrate (aacdec_info);

      if (retval != 0) {
        GST_ERROR ("error in Calculating the average Bitrate");
        return GST_STATE_CHANGE_FAILURE;

      }
      if (gst_pad_check_pull_range (aacdec_info->sinkpad)) {
        gst_pad_set_query_function (aacdec_info->srcpad,
            GST_DEBUG_FUNCPTR (mfw_gst_aacdec_src_query));
        aacdec_info->seek_flag = TRUE;
      }
      aacdec_info->total_frames = 0;
      aacdec_info->time_offset = 0;
      break;

    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;

    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_DEBUG ("GST_STATE_CHANGE_PAUSED_TO_READY");
#ifndef PUSH_MODE
      /* Following is memory leak fix */
      if ((aacdec_info->inbuffer1)
          && (aacdec_info->inbuffer1 != aacdec_info->inbuffer2)) {

        gst_buffer_unref (aacdec_info->inbuffer1);
        aacdec_info->inbuffer1 = NULL;
      }
      if (aacdec_info->inbuffer2) {

        gst_buffer_unref (aacdec_info->inbuffer2);
        aacdec_info->inbuffer1 = NULL;
      }
#else
      aacdec_info->inbuffer1 = NULL;
      aacdec_info->inbuffer2 = NULL;
#endif
      mfw_gst_aacdec_memclean (aacdec_info);

      aacdec_info->total_frames = 0;

#ifdef PUSH_MODE
      if (aacdec_info->pAdapter) {
        gst_adapter_clear (aacdec_info->pAdapter);
        g_object_unref (aacdec_info->pAdapter);
        aacdec_info->pAdapter = NULL;
      }
      deinit_tsmanager (&aacdec_info->tsMgr);
#endif
      break;

    case GST_STATE_CHANGE_READY_TO_NULL:
      GST_DEBUG ("GST_STATE_CHANGE_READY_TO_NULL");
      free_extra_codec_data (aacdec_info);

      break;

    default:
      break;
  }
  GST_DEBUG ("out of mfw_gst_aacdec_change_state routine");

  return ret;

}

/*=============================================================================
FUNCTION: mfw_gst_aacdec_get_query_types

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
static const GstQueryType *
mfw_gst_aacdec_get_query_types (GstPad * pad)
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

FUNCTION:   mfw_gst_aacdec_src_query

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
static gboolean
mfw_gst_aacdec_src_query (GstPad * pad, GstQuery * query)
{
  gboolean res = TRUE;
  GstPad *peer;

  MFW_GST_AACDEC_INFO_T *aacdec_info;
  aacdec_info = MFW_GST_AACDEC (GST_OBJECT_PARENT (pad));


  switch (GST_QUERY_TYPE (query)) {

    case GST_QUERY_DURATION:
    {

      GST_DEBUG ("coming in GST_QUERY_DURATION");
      GstFormat format;
      GstFormat rformat;
      gint64 total, total_bytes;
      GstPad *peer;

      /* save requested format */
      gst_query_parse_duration (query, &format, NULL);
      if ((peer = gst_pad_get_peer (aacdec_info->sinkpad)) == NULL)
        goto error;

      if (format == GST_FORMAT_TIME && gst_pad_query (peer, query)) {
        gst_query_parse_duration (query, NULL, &total);
        GST_DEBUG_OBJECT (aacdec_info,
            "peer returned duration %" GST_TIME_FORMAT, GST_TIME_ARGS (total));

      }
      /* query peer for total length in bytes */
      gst_query_set_duration (query, GST_FORMAT_BYTES, -1);


      if (!gst_pad_query (peer, query)) {
        gst_object_unref (peer);
        goto error;
      }
      gst_object_unref (peer);

      /* get the returned format */
      gst_query_parse_duration (query, &rformat, &total_bytes);

      if (rformat == GST_FORMAT_BYTES) {
        GST_DEBUG ("peer pad returned total bytes=%d", total_bytes);
      } else if (rformat == GST_FORMAT_TIME) {
        GST_DEBUG ("peer pad returned total time=%",
            GST_TIME_FORMAT, GST_TIME_ARGS (total_bytes));
      }

      /* Check if requested format is returned format */
      if (format == rformat)
        return TRUE;


      if (total_bytes != -1) {
        if (format != GST_FORMAT_BYTES) {
          if (!mfw_gst_aacdec_convert_sink
              (pad, GST_FORMAT_BYTES, total_bytes, &format, &total))
            goto error;
        } else {
          total = total_bytes;
        }
      } else {
        total = -1;
      }
      aacdec_info->total_time = total;
      gst_query_set_duration (query, format, total);

      if (format == GST_FORMAT_TIME) {
        GST_DEBUG ("duration=%" GST_TIME_FORMAT, GST_TIME_ARGS (total));
      } else {
        GST_DEBUG ("duration=%" G_GINT64_FORMAT ",format=%u", total, format);
      }
      break;
    }
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;
      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      if (!
          (res =
              mfw_gst_aacdec_convert_src (pad, src_fmt, src_val,
                  &dest_fmt, &dest_val)))
        goto error;

      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      break;
    }
    case GST_QUERY_SEEKING:
    {
      GstFormat format;
      gboolean seekable;
      gint64 segment_start, segment_stop;

      gst_query_parse_seeking (query, &format, &seekable,
          &segment_start, &segment_stop);
      if (aacdec_info->seek_flag) {
        GST_DEBUG ("aac is seekable!");
        seekable = TRUE;
      } else {
        GST_DEBUG ("aac is not seekable!");
        seekable = FALSE;
      }

      gst_query_set_seeking (query, format, seekable, segment_start,
          segment_stop);
      res = TRUE;
      break;
    }
    default:
      res = FALSE;
      break;
  }
  return res;

error:
  GST_ERROR ("error handling query");
  return FALSE;
}

/*==================================================================================================
FUNCTION:   mfw_gst_aacdec_convert_src

DESCRIPTION:    converts the format of value from src format to destination format on src pad .

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
mfw_gst_aacdec_convert_src (GstPad * pad, GstFormat src_format,
    gint64 src_value, GstFormat * dest_format, gint64 * dest_value)
{
  gboolean res = TRUE;
  guint scale = 1;
  gint bytes_per_sample;

  MFW_GST_AACDEC_INFO_T *aacdec_info;
  aacdec_info = MFW_GST_AACDEC (GST_OBJECT_PARENT (pad));

  bytes_per_sample = aacdec_info->number_of_channels * 4;

  switch (src_format) {
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_DEFAULT:
          if (bytes_per_sample == 0)
            return FALSE;
          *dest_value = src_value / bytes_per_sample;
          break;
        case GST_FORMAT_TIME:
        {
          gint byterate = bytes_per_sample * aacdec_info->sampling_freq;
          if (byterate == 0)
            return FALSE;
          *dest_value = src_value * GST_SECOND / byterate;
          break;
        }
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_DEFAULT:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          *dest_value = src_value * bytes_per_sample;
          break;
        case GST_FORMAT_TIME:
          if (aacdec_info->sampling_freq == 0)
            return FALSE;
          *dest_value = src_value * GST_SECOND / aacdec_info->sampling_freq;
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          scale = bytes_per_sample;
          /* fallthrough */
        case GST_FORMAT_DEFAULT:
          *dest_value = src_value * scale *
              aacdec_info->sampling_freq / GST_SECOND;
          break;
        default:
          res = FALSE;
      }
      break;
    default:
      res = FALSE;
  }

  return res;
}

/*==================================================================================================

FUNCTION:   mfw_gst_aacdec_convert_sink

DESCRIPTION:    converts the format of value from src format to destination format on sink pad .


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
mfw_gst_aacdec_convert_sink (GstPad * pad, GstFormat src_format,
    gint64 src_value, GstFormat * dest_format, gint64 * dest_value)
{
  gboolean res = TRUE;
  float avg_bitrate = 0;
  MFW_GST_AACDEC_INFO_T *aacdec_info;
  GST_DEBUG ("in mfw_gst_aacdec_convert_sink");
  aacdec_info = MFW_GST_AACDEC (GST_OBJECT_PARENT (pad));

  if (aacdec_info->app_params.App_adif_header_present) {
    avg_bitrate = aacdec_info->app_params.BlockParams.BitRate;
  } else
    avg_bitrate = aacdec_info->bit_rate;

  switch (src_format) {
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_TIME:
          if (avg_bitrate) {

            *dest_value =
                gst_util_uint64_scale (src_value, 8 * GST_SECOND, avg_bitrate);
          } else {
            *dest_value = GST_CLOCK_TIME_NONE;
          }
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          if (avg_bitrate) {

            *dest_value = gst_util_uint64_scale (src_value, avg_bitrate,
                8 * GST_SECOND);

          } else {
            *dest_value = 0;
          }

          break;

        default:
          res = FALSE;
      }
      break;
    default:
      res = FALSE;
  }

  GST_DEBUG ("out of mfw_gst_aacdec_convert_sink");
  return res;
}

/*==================================================================================================

FUNCTION:   mfw_gst_aacdec_seek

DESCRIPTION:    performs seek operation

ARGUMENTS PASSED:
        aacdec_info -   pointer to decoder element
        pad         -   pointer to GstPad
        event       -   pointer to GstEvent

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
mfw_gst_aacdec_seek (MFW_GST_AACDEC_INFO_T * aacdec_info,
    GstPad * pad, GstEvent * event)
{
  gdouble rate;
  GstFormat format, conv;
  GstSeekFlags flags;
  GstSeekType cur_type, stop_type;
  gint64 cur = 0, stop = 0;
  gint64 time_cur = 0, time_stop = 0;
  gint64 bytes_cur = 0, bytes_stop = 0;
  gboolean flush;
  gboolean res;
  guint bytesavailable;
  gst_event_parse_seek (event, &rate, &format, &flags, &cur_type, &cur,
      &stop_type, &stop);

  GST_DEBUG ("seek from  %" GST_TIME_FORMAT "--------------- to %"
      GST_TIME_FORMAT, GST_TIME_ARGS (cur), GST_TIME_ARGS (stop));

  if (format != GST_FORMAT_TIME) {
    conv = GST_FORMAT_TIME;
    if (!mfw_gst_aacdec_convert_src (pad, format, cur, &conv, &time_cur))
      goto convert_error;
    if (!mfw_gst_aacdec_convert_src (pad, format, stop, &conv, &time_stop))
      goto convert_error;
  } else {
    time_cur = cur;
    time_stop = stop;
  }
  GST_DEBUG ("seek from  %" GST_TIME_FORMAT "--------------- to %"
      GST_TIME_FORMAT, GST_TIME_ARGS (time_cur), GST_TIME_ARGS (time_stop));

  /* shave off the flush flag, we'll need it later */
  flush = ((flags & GST_SEEK_FLAG_FLUSH) != 0);
  res = FALSE;
  conv = GST_FORMAT_BYTES;



  if (!mfw_gst_aacdec_convert_sink
      (pad, GST_FORMAT_TIME, time_cur, &conv, &bytes_cur))
    goto convert_error;

  if (!mfw_gst_aacdec_convert_sink
      (pad, GST_FORMAT_TIME, time_stop, &conv, &bytes_stop))
    goto convert_error;

  {
    GstEvent *seek_event;


    seek_event =
        gst_event_new_seek (rate, GST_FORMAT_BYTES, flags, cur_type,
        bytes_cur, stop_type, bytes_stop);

    /* do the seek */
    res = gst_pad_push_event (aacdec_info->sinkpad, seek_event);

  }


  return TRUE;

  /* ERRORS */
convert_error:
  {
    /* probably unsupported seek format */
    GST_ERROR ("failed to convert format %u into GST_FORMAT_TIME", format);
    return FALSE;
  }
}


/*=============================================================================
FUNCTION:   mfw_gst_aacdec_src_event

DESCRIPTION: This functions handles the events that triggers the
			 source pad of the mpeg4 decoder element.

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
RETURN VALUE:
        TRUE       -	if event is sent to src properly
	    FALSE	   -	if event is not sent to src properly

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean
mfw_gst_aacdec_src_event (GstPad * pad, GstEvent * event)
{

  gboolean res;
  MFW_GST_AACDEC_INFO_T *aacdec_info;
  GST_DEBUG ("in mfw_gst_aacdec_src_event routine");
  aacdec_info = MFW_GST_AACDEC (GST_OBJECT_PARENT (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      if (aacdec_info->seek_flag == TRUE) {
        res = mfw_gst_aacdec_seek (aacdec_info, pad, event);
        gst_event_unref (event);

      } else {
        res = gst_pad_push_event (aacdec_info->sinkpad, event);

      }
      break;

    default:
      res = gst_pad_push_event (aacdec_info->sinkpad, event);
      break;


  }
  GST_DEBUG ("out of mfw_gst_aacdec_src_event routine");

  return res;

}

/*=============================================================================
FUNCTION:   mfw_gst_aacdec_sink_event

DESCRIPTION:

ARGUMENTS PASSED:
        pad        -    pointer to pad
        event      -    pointer to event
RETURN VALUE:
        TRUE       -	if event is sent to sink properly
	    FALSE	   -	if event is not sent to sink properly

PRE-CONDITIONS:
        None

POST-CONDITIONS:
        None

IMPORTANT NOTES:
        None
=============================================================================*/
static gboolean
mfw_gst_aacdec_sink_event (GstPad * pad, GstEvent * event)
{
  gboolean result = TRUE;
  MFW_GST_AACDEC_INFO_T *aacdec_info;
  AACD_RET_TYPE rc;
  GstCaps *src_caps = NULL, *caps = NULL;
  GstBuffer *outbuffer = NULL;
  GstFlowReturn res = GST_FLOW_OK;
  guint8 *inbuffer;
  gint inbuffsize;
  guint64 time_duration = 0;



  aacdec_info = MFW_GST_AACDEC (GST_OBJECT_PARENT (pad));

  GST_DEBUG ("in mfw_gst_aacdec_sink_event function");
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
    {
      GstFormat format;
      gint64 start, stop, position;
      gint64 nstart, nstop;
      GstEvent *nevent;

      GST_DEBUG ("in GST_EVENT_NEWSEGMENT");
      gst_event_parse_new_segment (event, NULL, NULL, &format, &start,
          &stop, &position);

      if (format == GST_FORMAT_BYTES) {
        format = GST_FORMAT_TIME;
        if (start != 0)
          result =
              mfw_gst_aacdec_convert_sink (pad,
              GST_FORMAT_BYTES, start, &format, &nstart);
        else
          nstart = start;
        if (stop != 0) {
#if 0
          result =
              mfw_gst_aacdec_convert_sink (pad,
              GST_FORMAT_BYTES, stop, &format, &nstop);
#endif
          nstop = -1;
        } else {
          nstop = stop;
        }
        nevent =
            gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_TIME,
            nstart, nstop, nstart);
        gst_event_unref (event);
        aacdec_info->time_offset = (guint64) nstart;

        result = gst_pad_push_event (aacdec_info->srcpad, nevent);
        if (TRUE != result) {
          GST_ERROR ("Error in pushing the event,result	is %d", result);

        }
      } else if (format == GST_FORMAT_TIME) {
        aacdec_info->time_offset = (guint64) start;

        result = gst_pad_push_event (aacdec_info->srcpad, event);
        if (TRUE != result) {
          GST_ERROR ("Error in pushing the event,result	is %d", result);

        }
      } else {
        gst_event_unref (event);
      }
      break;
    }
    case GST_EVENT_EOS:
    {

      GST_DEBUG ("Decoder: Got an EOS from Demuxer");
      if (aacdec_info->init_done) {
#ifndef PUSH_MODE
        inbuffsize = GST_BUFFER_SIZE (aacdec_info->inbuffer1);
#endif
        aacdec_info->eos = TRUE;


        aacdec_info->inbuffer2 = NULL;
#ifndef PUSH_MODE
        result = mfw_gst_aacdec_data (aacdec_info, &inbuffsize);

#else
        while ((inbuffsize = gst_adapter_available (aacdec_info->pAdapter)) > 0) {
          gint flushlen;
          flushlen = mfw_gst_aacdec_data (aacdec_info, inbuffsize);
          if (flushlen == -1)
            break;
          if (flushlen > inbuffsize) {
            flushlen = inbuffsize;
          }
          gst_adapter_flush (aacdec_info->pAdapter, flushlen);
        }
#endif
        if (result != TRUE) {
          GST_ERROR ("Error in decoding the frame");
        }


      }

      result = gst_pad_push_event (aacdec_info->srcpad, event);
      if (TRUE != result) {
        GST_ERROR ("Error in pushing the event,result	is %d", result);

      }

      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {

      GST_DEBUG ("GST_EVENT_FLUSH_STOP");

      // result = mfw_gst_aacdec_mem_flush(aacdec_info);
#ifndef PUSH_MODE
      if (aacdec_info->inbuffer2) {
        gst_buffer_unref (aacdec_info->inbuffer2);
        aacdec_info->inbuffer2 = NULL;
      }
#else
      gst_adapter_clear (aacdec_info->pAdapter);
      clear_tsmanager (&aacdec_info->tsMgr);
#endif
      result = gst_pad_push_event (aacdec_info->srcpad, event);
      if (TRUE != result) {
        GST_ERROR ("Error in pushing the event,result	is %d", result);

      }
      break;
    }
    case GST_EVENT_TAG:
    {
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_FLUSH_START:
    default:
    {
      result = gst_pad_event_default (pad, event);
      break;
    }
  }

  GST_DEBUG ("out of mfw_gst_aacdec_sink_event");

  return result;
}


/*=============================================================================
FUNCTION:   mfw_gst_aacdec_set_caps

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
static gboolean
mfw_gst_aacdec_set_caps (GstPad * pad, GstCaps * caps)
{



  MFW_GST_AACDEC_INFO_T *aacdec_info;
  const gchar *mime;
  gint mpeg_version;
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  aacdec_info = MFW_GST_AACDEC (gst_pad_get_parent (pad));

  GST_DEBUG ("in mfw_gst_aacdec_set_caps routine");
  mime = gst_structure_get_name (structure);

  if (strcmp (mime, "audio/mpeg") != 0) {
    GST_WARNING
        ("Wrong	mimetype %s	provided, we only support %s",
        mime, "audio/mpeg");
    gst_object_unref (aacdec_info);
    return FALSE;
  }
  gst_structure_get_int (structure, "mpegversion", &mpeg_version);
  if (((mpeg_version != 2) && (mpeg_version != 4))) {
    GST_ERROR ("Caps negotiation: mpeg version not supprted");
    gst_object_unref (aacdec_info);
    return FALSE;
  }

  gst_structure_get_int (structure, "bitrate", &aacdec_info->bit_rate);

  if (!gst_pad_set_caps (pad, caps)) {
    gst_object_unref (aacdec_info);
    return FALSE;
  }

  GST_DEBUG ("out of mfw_gst_aacdec_set_caps routine");
  gst_object_unref (aacdec_info);
  return TRUE;
}

/*=============================================================================
FUNCTION:   mfw_gst_aacdec_init

DESCRIPTION:This function creates the pads on the elements and register the
			function pointers which operate on these pads.

ARGUMENTS PASSED:
        pointer the aac decoder element handle.

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
mfw_gst_aacdec_init (MFW_GST_AACDEC_INFO_T * aacdec_info)
{

  GstElementClass *klass = GST_ELEMENT_GET_CLASS (aacdec_info);
  GST_DEBUG (" \n in mfw_gst_aacdec_init routine \n");
  aacdec_info->sinkpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (klass, "sink"), "sink");

  aacdec_info->srcpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template
      (klass, "src"), "src");

  gst_element_add_pad (GST_ELEMENT (aacdec_info), aacdec_info->sinkpad);
  gst_element_add_pad (GST_ELEMENT (aacdec_info), aacdec_info->srcpad);

  gst_pad_set_setcaps_function (aacdec_info->sinkpad, mfw_gst_aacdec_set_caps);
  gst_pad_set_chain_function (aacdec_info->sinkpad, mfw_gst_aacdec_chain);

  gst_pad_set_event_function (aacdec_info->sinkpad,
      GST_DEBUG_FUNCPTR (mfw_gst_aacdec_sink_event));

  gst_pad_set_query_type_function (aacdec_info->srcpad,
      GST_DEBUG_FUNCPTR (mfw_gst_aacdec_get_query_types));
  gst_pad_set_event_function (aacdec_info->srcpad,
      GST_DEBUG_FUNCPTR (mfw_gst_aacdec_src_event));


  GST_DEBUG ("out of mfw_gst_aacdec_init");

#define MFW_GST_AAC_PLUGIN VERSION
  PRINT_CORE_VERSION (aacd_decode_versionInfo ());
  PRINT_PLUGIN_VERSION (MFW_GST_AAC_PLUGIN);

  INIT_DEMO_MODE (aacd_decode_versionInfo (), aacdec_info->demo_mode);


}

/*=============================================================================
FUNCTION:   mfw_gst_aacdec_class_init

DESCRIPTION:Initialise the class only once (specifying what signals,
            arguments and virtual functions the class has and setting up
            global state)
ARGUMENTS PASSED:
       	klass   - pointer to aac decoder's element class

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
mfw_gst_aacdec_class_init (MFW_GST_AACDEC_CLASS_T * klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  parent_class_aac = (GstElementClass *) g_type_class_ref (GST_TYPE_ELEMENT);
  gobject_class->set_property = mfw_gst_aacdec_set_property;
  gobject_class->get_property = mfw_gst_aacdec_get_property;
  gstelement_class->change_state = mfw_gst_aacdec_change_state;
}

/*=============================================================================
FUNCTION:  mfw_gst_aacdec_base_init

DESCRIPTION:
            aac decoder element details are registered with the plugin during
            _base_init ,This function will initialise the class and child
            class properties during each new child class creation


ARGUMENTS PASSED:
        Klass   -   pointer to aac decoder plug-in class

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
mfw_gst_aacdec_base_init (MFW_GST_AACDEC_CLASS_T * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
  
  FSL_GST_ELEMENT_SET_DETAIL_SIMPLE(element_class, "aac audio decoder",
      "Codec/Decoder/Audio", "Decode compressed aac audio to raw data");
}

/*=============================================================================
FUNCTION: mfw_gst_aacdec_get_type

DESCRIPTION:    intefaces are initiated in this function.you can register one
                or more interfaces  after having registered the type itself.

ARGUMENTS PASSED:
            None

RETURN VALUE:
                 A numerical value ,which represents the unique identifier of this
            element(aacdecoder)

PRE-CONDITIONS:
            None

POST-CONDITIONS:
            None

IMPORTANT NOTES:
            None
=============================================================================*/
GType
mfw_gst_aacdec_get_type (void)
{
  static GType aacdec_type = 0;

  if (!aacdec_type) {
    static const GTypeInfo aacdec_info = {
      sizeof (MFW_GST_AACDEC_CLASS_T),
      (GBaseInitFunc) mfw_gst_aacdec_base_init,
      NULL,
      (GClassInitFunc) mfw_gst_aacdec_class_init,
      NULL,
      NULL,
      sizeof (MFW_GST_AACDEC_INFO_T),
      0,
      (GInstanceInitFunc) mfw_gst_aacdec_init,
    };
    aacdec_type = g_type_register_static (GST_TYPE_ELEMENT,
        "MFW_GST_AACDEC_INFO_T", &aacdec_info, (GTypeFlags) 0);
  }
  GST_DEBUG_CATEGORY_INIT (mfw_gst_aacdec_debug, "mfw_aacdecoder",
      0, "Freescale's AAC Decoder's Log");

  return aacdec_type;
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
     static gboolean plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "mfw_aacdecoder",
      FSL_GST_DEFAULT_DECODER_RANK_LEGACY, MFW_GST_TYPE_AACDEC);
}

/*=============================================================================
FUNCTION: mfw_gst_aacdec_mem_flush

DESCRIPTION: this function flushes the current memory and allocate the new memory
                for decoder .

ARGUMENTS PASSED:
        mp3dec_info -   pointer to mp3decoder element structure

RETURN VALUE:
        None

PRE-CONDITIONS:
        None

POST-CONDITIONS:


IMPORTANT NOTES:
        None
=============================================================================*/

static gboolean
mfw_gst_aacdec_mem_flush (MFW_GST_AACDEC_INFO_T * aacdec_info)
{
  gint loopctr = 0;
  gboolean result = TRUE;
  gint num;
  gint rec_no = 0;
  gint nr;
  AACD_Decoder_Config *dec_config = NULL;
  AACD_Mem_Alloc_Info_Sub *mem;
  AACD_RET_TYPE rc = 0;

  GST_DEBUG ("in mfw_gst_aacdec_mem_flush");
  if (!aacdec_info->init_done)
    return FALSE;



  mfw_gst_aacdec_memclean (aacdec_info);

  memset (&aacdec_info->app_params, 0, sizeof (AACD_App_params *));

  /* allocate memory for config structure */
  dec_config = (AACD_Decoder_Config *)
      aacd_alloc_fast (sizeof (AACD_Decoder_Config));
  aacdec_info->app_params.dec_config = dec_config;
  if (dec_config == NULL) {
    GST_ERROR ("error in allocation of decoder config structure");
    return FALSE;
  }



  /* call query mem function to know mem requirement of library */

  if (aacd_query_dec_mem (dec_config) != AACD_ERROR_NO_ERROR) {
    GST_ERROR ("Failed to get the memory configuration for the decoder");
    return FALSE;
  }

  /* Number of memory chunk requests by the decoder */
  nr = dec_config->aacd_mem_info.aacd_num_reqs;

  for (rec_no = 0; rec_no < nr; rec_no++) {
    mem = &(dec_config->aacd_mem_info.mem_info_sub[rec_no]);

    if (mem->aacd_type == AACD_FAST_MEMORY) {
      mem->app_base_ptr = aacd_alloc_fast (mem->aacd_size);
      if (mem->app_base_ptr == NULL)
        return FALSE;
    } else {
      mem->app_base_ptr = aacd_alloc_slow (mem->aacd_size);
      if (mem->app_base_ptr == NULL)
        return FALSE;
    }
    memset (dec_config->aacd_mem_info.mem_info_sub[rec_no].app_base_ptr,
        0, dec_config->aacd_mem_info.mem_info_sub[rec_no].aacd_size);

  }

  aacdec_info->app_params.BitsInHeader = 0;
  aacdec_info->app_params.App_adif_header_present = FALSE;

  /* register the call-back function in the decoder context */
  //dec_config->app_swap_buf = app_swap_buffers_aac_dec;

#ifndef OUTPUT_24BITS
  dec_config->num_pcm_bits = AACD_16_BIT_OUTPUT;
#else
  dec_config->num_pcm_bits = AACD_24_BIT_OUTPUT;
#endif  /*OUTPUT_24BITS*/                  /*OUTPUT_24BITS */


  rc = aacd_decoder_init (dec_config);
  if (rc != AACD_ERROR_NO_ERROR) {
    GST_ERROR ("Error in initializing the decoder");
    return FALSE;
  }

  aacdec_info->init_done = FALSE;

  GST_DEBUG ("out of mfw_gst_aacdec_mem_flush\n");
  return result;
}

FSL_GST_PLUGIN_DEFINE("aacdec", "aac audio decoder", plugin_init);


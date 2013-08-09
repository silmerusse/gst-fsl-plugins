/*
 * Copyright (c) 2008-2012, Freescale Semiconductor, Inc. 
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
 * Module Name:    mfw_gst_audio_pp.h
 *
 * Description:    
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 * Nov 03 2008 Sario HU <b01138@freescale.com>
 * - Initial version
 */



/*=============================================================================
                            INCLUDE FILES
=============================================================================*/

#ifndef __MFW_GST_AUDIO_PP_H__
#define __MFW_GST_AUDIO_PP_H__

#include <gst/gst.h>



/*=============================================================================
                                           CONSTANTS
=============================================================================*/

/* None. */

/*=============================================================================
                                             ENUMS
=============================================================================*/

/* None. */
  
/*=============================================================================
                                            MACROS
=============================================================================*/
G_BEGIN_DECLS


/* #defines don't like whitespacey bits */
#define MFW_GST_TYPE_AUDIO_PP \
  (mfw_gst_audio_pp_get_type())
#define MFW_GST_AUDIO_PP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MFW_GST_TYPE_AUDIO_PP, MfwGstAudioPP))
#define MFW_GST_AUDIO_PP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),MFW_GST_TYPE_AUDIO_PP, MfwGstAudioPPClass))
#define MFW_GST_IS_AUDIO_PP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MFW_GST_TYPE_AUDIO_PP))
#define MFW_GST_IS_AUDIO_PP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),MFW_GST_TYPE_AUDIO_PP))

#define MAX_AUDIO_PP_FEATURE_INSTANCE 1
/*=============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/

typedef enum
{
    PP_DISABLED = 0,
    PP_ENABLED = 1,
}PP_STATUS;

typedef enum
{
    PP_EMPTY = 0,
    PP_IDLE,
    PP_BUSY,
}PP_STATE;


typedef struct {
    PEQ_PPP_Config peqconfig;
    PEQ_PL peqpl;
    PEQ_INFO pqeinfo;
}PEQ_INSTANCE_CONTEXT;


typedef struct {
    guint pmid;
    guint pmtype;
    void * pparameter;
}PPFeatureParameter;


typedef struct {
    guint numofparater;
    PPFeatureParameter parameters[1];
}PPFeatureParametersList;


typedef struct {
    PP_STATE state;
    void * priv; 
    gboolean newparameterapplied;
    PPFeatureParametersList * pmlist;
}PPFeatureInstance;


typedef struct _MfwGstAudioPP
{
    GstElement element;
    GstPad *sinkpad, *srcpad;
    gboolean caps_set;
    PP_STATUS status;
    gint32 premode;
    gint32 attenuation;   
    PPFeatureInstance ppfeatureinstance[MAX_AUDIO_PP_FEATURE_INSTANCE];

}MfwGstAudioPP;


typedef struct _MfwGstAudioPPClass 
{
     GstElementClass parent_class;
}MfwGstAudioPPClass;

/*=============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/* None. */

/*=============================================================================
                                     FUNCTION PROTOTYPES
=============================================================================*/

GType mfw_gst_audio_pp_get_type (void);

G_END_DECLS

/*===========================================================================*/

#endif /* __MFW_GST_AUDIO_PP_H__ */

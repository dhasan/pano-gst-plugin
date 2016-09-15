/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2016 Deniz <<user@hostname.org>>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
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

#ifndef __GST_PANORAMA_H__
#define __GST_PANORAMA_H__

#include <gst/gst.h>
#include <helper_math.h>

#define SINKPADCNT  (6)

#define DEFAULT_INBUFFERSIZE  (4*1920*1080)
#define DEFAULT_PHI         (90)
#define DEFAULT_THETA       (90)
#define DEFAULT_MATRIXAUTOUPDATE (TRUE)

#define DEFAULT_OUTWIDTH (640)
#define DEFAULT_OUTHEIGHT (480)

#define DEFAULT_XYMAPFILE "xymap.dat"
#define DEFAULT_BMAPFILE "bmap.dat"


enum{

    FRONTSINKID = 0,
    RIGHTSINKID,
    LEFTSINKID,
    BACKSINKID,
    TOPSINKID,
    BOTTOMSINKID

};

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_PANORAMA \
  (gst_panorama_get_type())
#define GST_PANORAMA(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PANORAMA,GstPanorama))
#define GST_PANORAMA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PANORAMA,GstPanoramaClass))
#define GST_IS_PANORAMA(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PANORAMA))
#define GST_IS_PANORAMA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PANORAMA))

typedef struct _GstPanorama      GstPanorama;
typedef struct _GstPanoramaClass GstPanoramaClass;

#if 1
typedef struct _GstPanoramaMeta  GstPanoramaMeta;
struct _GstPanoramaMeta {
    GstMeta       meta;
    gint          age;
    gchar *name;
};

GType gst_panorama_meta_api_get_type (void);
#define GST_PANORAMA_META_API_TYPE (gst_panorama_meta_api_get_type())
#define gst_buffer_get_panorama_meta(b) \
    ((GstPanoramaMeta *)gst_buffer_get_meta((b),GST_PANORAMA_META_API_TYPE))


const GstMetaInfo* gst_panorama_meta_get_info (void);
#define GST_PANORAMA_META_INFO (gst_panorama_meta_get_info())
GstPanoramaMeta *gst_buffer_add_panorama_meta (
                                                    GstBuffer *buffer,
                                                    gint age,
                                                    const gchar *name
                                              );
#endif

#if 1
typedef struct
{
    GstAllocator parent;
}PanoramaAllocator;

struct _PanoramaAllocatorClass
{
    GstAllocatorClass parent_class;
};
typedef struct _PanoramaAllocatorClass PanoramaAllocatorClass;

typedef struct
{
    GstMemory mem;
    gpointer data;

}CudaHostMemory;




#endif

struct PanoramaVector {
    gint phi;
    gint theta;
};

struct _GstPanorama
{
    GstElement element;
    GstAllocator *cudaallocator;
    void *cudapano;

    GstPad *sinkpads[SINKPADCNT];
    // GstPad *leftsinkpad;
    // GstPad *rightsinkpad;
    // GstPad *backsinkpad;
    // GstPad *topsinkpad;
    // GstPad *bottomsinkpad;
    GstPad *outsrcpad;

    /*Frame counters on sink pads, and current mask based on phi and theta*/
    volatile guint newframes;
    // volatile guint processingflag;
    // volatile guint matrixupdateflag;
    GMutex processing;

    guint newframemask;
    gboolean autoupdate;


    /*The angle of view*/
    gint phi;
    gint theta;
    struct PanoramaVector vector;
    guint inbuffersize;
    guint outbuffersize;
    gint inwidth;
    gint inheight;
    gint outwidth;
    gint outheight;

// gboolean silent;
};

struct _GstPanoramaClass
{
    GstElementClass parent_class;
};

GType gst_panorama_get_type (void);

G_END_DECLS

#endif /* __GST_PANORAMA_H__ */

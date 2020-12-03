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

/**
 * SECTION:element-panorama
 *
 * FIXME:Describe panorama here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! panorama ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <string.h>
#include "gstpanorama.h"

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>



/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_PHI =1,
  PROP_THETA,
  PROP_MATRIXUPDATE,
  PROP_MATRIXAUTOUPDATE,
  PROP_VECTOR,
  PROP_XYMAP,
  PROP_BMAP,
  PROP_INTERMAP
};


void gstcuda_update_source(void *priv, int sourceid, unsigned int *data, int id);
void gstcuda_process(void *priv, int id);
void gstcuda_get_output(void *priv, void *out, int id);
void gstcuda_set_dims(  void *priv,
                        int colorspace,
                        int yuvtogray8,
                        int pano_width,
                        int pano_height,
                        int source_width,
                        int source_height,
                        int dest_width,
                        int dest_height
                    );
void gstcuda_bmap_config(void *priv, const char *bmapname);
void gstcuda_intermap_config(void *priv, const char *intmapname);
void gstcuda_xymap_config(void *priv, const char *xymapname);
void gstcuda_update_matrix(void *priv, float fov, float phi, float theta);
void gstcuda_sync_all(void *priv);
void *gstcuda_host_alloc(void *priv, size_t size);
void gstcuda_host_free(void *priv, void *mem);

void gstcuda_source_alloc(
                        void *priv, 
                        int colorspace,
                        int yuvtogray8,
                        int source_width,
                        int source_height,
                        int id);
void gstpano_output_alloc(
                        void *priv,
                        int colorspace,
                        int yuvtogray8,
                        int dest_width,
                        int dest_height);

# define M_PI           3.14159265358979323846
static float deg_to_rad(float deg){
    float rad = deg*M_PI/180;
    return rad; 
}


static  const gchar *padnames[6] = {"frontsink", "rightsink", "leftsink", "backsink", "topsink", "bottomsink"};
static  guint padmasks[6] ={0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00000010, 0x00000020,};
/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    //GST_STATIC_CAPS ("ANY")
    GST_STATIC_CAPS (
        "video/x-raw, "
        "format = (string) {xBGR, I420, GRAY8}, "
        "width = (int) [ 320, 1920 ], "
        "height = (int) [ 240, 1080 ], "
        "framerate = (fraction)30/1"
    )

    );

static GstStaticPadTemplate outsrc_factory = GST_STATIC_PAD_TEMPLATE ("outsrc",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
   // GST_STATIC_CAPS ("ANY")
     GST_STATIC_CAPS (
        "video/x-raw, "
        "format = (string) {xBGR, I420, GRAY8, RGBA, RGB}, "
        "width = (int) [ 320, 1920 ], "
        "height = (int) [ 240, 1880 ], "
        "framerate = (fraction)30/1"
    )
    );

#define gst_panorama_parent_class parent_class
G_DEFINE_TYPE (GstPanorama, gst_panorama, GST_TYPE_ELEMENT);

GType panorama_allocator_get_type (void);
G_DEFINE_TYPE (PanoramaAllocator, panorama_allocator, GST_TYPE_ALLOCATOR);


static void gst_panorama_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_panorama_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_panorama_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_panorama_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

/* GObject vmethod implementations */

#if 1
static gpointer
_my_mem_map (CudaHostMemory * mem, gsize maxsize, GstMapFlags flags)
{
    gpointer res;

    while (TRUE) {
        if ((res = g_atomic_pointer_get (&mem->data)) != NULL)
            break;

        //res = g_malloc (maxsize);
        res = gstcuda_host_alloc(mem->cudapriv, maxsize);

        if (g_atomic_pointer_compare_and_exchange (&mem->data, NULL, res))
            break;

        //g_free (res);
        gstcuda_host_free(mem->cudapriv, res);
    }

    return res;
}

static gboolean
_my_mem_unmap (CudaHostMemory * mem)
{
  //GST_DEBUG ("%p: unmapped", mem);
  return TRUE;
}

static void
panorama_allocator_init (PanoramaAllocator * allocator)
{
    GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

    alloc->mem_type = "CudaHostMemory";
    g_print("....................................\n...............asdfasdf\n");
    alloc->mem_map = (GstMemoryMapFunction) _my_mem_map;
    alloc->mem_unmap = (GstMemoryUnmapFunction) _my_mem_unmap;
  //  alloc->mem_share = (GstMemoryShareFunction) _my_mem_share;
}




static GstMemory *
_my_alloc (GstAllocator * allocator, gsize size, GstAllocationParams * params){
    CudaHostMemory *mem;
    gsize maxsize = size + params->prefix + params->padding;

   // g_print ("alloc from allocator %p", allocator);
    PanoramaAllocator *pa = GST_PANORAMA_ALLOCATOR(allocator);
    mem = g_slice_new (CudaHostMemory);
   // mem->cudapriv = gpriv;
    mem->cudapriv = pa->cudapriv;


    gst_memory_init (GST_MEMORY_CAST (mem), params->flags, allocator, NULL,
        maxsize, params->align, params->prefix, size);

    mem->data = NULL;

    return (GstMemory *) mem;
}

static void
_my_free (GstAllocator * allocator, GstMemory * mem)
{
    CudaHostMemory *mmem = (CudaHostMemory *) mem;

    gstcuda_host_free(mmem->cudapriv, mmem->data);
    g_slice_free (CudaHostMemory, mmem);
}

static void
panorama_allocator_class_init (PanoramaAllocatorClass * klass){
    GstAllocatorClass *allocator_class;


    allocator_class = (GstAllocatorClass *) klass;
    allocator_class->alloc = _my_alloc;
    allocator_class->free = _my_free;
}

#endif


/* initialize the panorama's class */
static void
gst_panorama_class_init (GstPanoramaClass * klass){

    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;



    gobject_class->set_property = gst_panorama_set_property;
    gobject_class->get_property = gst_panorama_get_property;




    g_object_class_install_property (gobject_class, PROP_PHI,
        g_param_spec_int64 ("phi", "Phi", "Phi angle",
            0,180,DEFAULT_PHI, G_PARAM_READWRITE| G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_THETA,
        g_param_spec_int64 ("theta", "Theta", "Theta angle",
            0,360,DEFAULT_THETA, G_PARAM_READWRITE| G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_MATRIXUPDATE,
        g_param_spec_boolean ("matrixupdate", "Matrixupdate", "Trigger matrix updating",
            FALSE, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_MATRIXAUTOUPDATE,
        g_param_spec_boolean ("matrixautoupdate", "Matrixautoupdate", "Auto update the matrix when new angle is provided",
            DEFAULT_MATRIXAUTOUPDATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_VECTOR,
        g_param_spec_pointer ("vector", "Vector", "Structure to phi and theta",
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_XYMAP,
        g_param_spec_string ("xymap", "xymap", "Location of xymap file",
            DEFAULT_XYMAPFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_BMAP,
        g_param_spec_string ("bmap", "bmap", "Location of bmap file",
            DEFAULT_BMAPFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_INTERMAP,
        g_param_spec_string ("intermap", "intermap", "Location of xymap file",
            DEFAULT_INTERMAPFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gst_element_class_set_details_simple(gstelement_class,
        "Panorama",
        "FIXME:Generic",
        "FIXME:Generic Template Element",
        "Deniz <<user@hostname.org>>");

    gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&outsrc_factory));

    gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&sink_factory));






}


gint gst_pad_get_id(GstPad *pad){

    int i;
    for (i=0;i<6;i++){
        if (!(strcmp(GST_PAD_NAME(pad), padnames[i])))
            return i;
    }

    g_print("padname: %s not found!" ,GST_PAD_NAME(pad));

    return -1;
}


static void
gst_my_filter_loop (GstPad *pad)
{
    GstFlowReturn ret;
   // guint64 len;
   // GstFormat fmt = GST_FORMAT_BYTES;
    GstBuffer *buf = NULL;
    GstBuffer *outbuf;
//    GstBuffer *outbuffer;
    GstPanorama *filter = NULL;
//    gboolean padfound = FALSE;
    gint padid;
//    gpointer outdata;
    GstMapInfo info;

 // gobject_class = (GObjectClass *) klass;
 // gstelement_class = (GstElementClass *) klass;

 // gobject_class->set_property = gst_panorama_set_property;
 // gobject_class->get_property = gst_panorama_get_property;

    filter = GST_PANORAMA (gst_pad_get_parent_element (pad));

  // gst_element_class_set_details_simple(gstelement_class,
  //   "panorama",
  //   "FIXME:Generic",
  //   "FIXME:Generic Template Element",
  //   "Deniz <<user@hostname.org>>");

    padid = gst_pad_get_id(pad);

    if (filter->newframemask & padmasks[padid]){

        ret = gst_pad_pull_range (pad, 0, filter->inbuffersize, &buf);
        if (ret == GST_FLOW_OK) {

            if (g_mutex_trylock(&filter->processing[filter->ind])){
                g_mutex_unlock(&filter->processing[filter->ind]);
                g_atomic_int_or(&filter->newframes[filter->ind], padmasks[padid]);

                gst_buffer_map (buf, &info, GST_MAP_READ);
                //memset (info.data, 0xff, info.size);
                //update source
              //  g_print("updating source %d\n",padid);
                gstcuda_update_source(filter->cudapriv, padid, (unsigned int *)info.data, filter->ind);
                gst_buffer_unmap (buf, &info);


            }
        }else{
            g_print("error pulling on %s\n", GST_PAD_NAME(pad));
        }


    }else{
        //pause task
       // gst_pad_pause_task(pad);
        g_print("ok..\n");
        return;
    }

    //Just in case
    g_atomic_int_and(&filter->newframes[filter->ind], filter->newframemask);

    if (g_atomic_int_compare_and_exchange(&filter->newframes[filter->ind], filter->newframemask, 0)){
        g_mutex_lock(&filter->processing[filter->ind]);

        g_print("processing..\n");
        gstcuda_process(filter->cudapriv, !filter->ind);
        g_print("processing done.\n");

        outbuf = gst_buffer_new_allocate (filter->cudaallocator, filter->outbuffersize, NULL);
        gst_buffer_map (outbuf, &info, GST_MAP_WRITE);
        g_print("start memcpy\n");
        gstcuda_get_output(filter->cudapriv, info.data, !filter->ind);
        gst_buffer_unmap (outbuf, &info);
    
        gst_pad_push(filter->outsrcpad, outbuf);
        g_mutex_unlock(&filter->processing[filter->ind]);
    }else{

    }







#if 0
  if (!gst_pad_query_duration (filter->sinkpad, fmt, &len)) {
    GST_DEBUG_OBJECT (filter, "failed to query duration, pausing");
    goto stop;
  }

   if (filter->offset >= len) {
    GST_DEBUG_OBJECT (filter, "at end of input, sending EOS, pausing");
    gst_pad_push_event (filter->srcpad, gst_event_new_eos ());
    goto stop;
  }

  /* now, read BLOCKSIZE bytes from byte offset filter->offset */
  ret = gst_pad_pull_range (filter->sinkpad, filter->offset,
      BLOCKSIZE, &buf);

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (filter, "pull_range failed: %s", gst_flow_get_name (ret));
    goto stop;
  }

  /* now push buffer downstream */
  ret = gst_pad_push (filter->srcpad, buf);

  buf = NULL; /* gst_pad_push() took ownership of buffer */

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (filter, "pad_push failed: %s", gst_flow_get_name (ret));
    goto stop;
  }

  /* everything is fine, increase offset and wait for us to be called again */
  filter->offset += BLOCKSIZE;
  return;


stop:
  GST_DEBUG_OBJECT (filter, "pausing task");
  gst_pad_pause_task (filter->sinkpad);
#endif
}

static gboolean
gst_my_filter_activate_pull (GstPad    * pad,
                 GstObject * parent,
                 GstPadMode  mode,
                 gboolean    active)
{
    gboolean res;

    //GstPanorama *filter = GST_PANORAMA (parent);

    switch (mode) {
        case GST_PAD_MODE_PUSH:
            res = TRUE;
        break;
        case GST_PAD_MODE_PULL:
            if (active) {
                //filter->offset = 0;
                res = gst_pad_start_task (pad,
                    (GstTaskFunction) gst_my_filter_loop, pad, NULL);
            } else {
                res = gst_pad_stop_task (pad);
            }
        break;
        default:
            /* unknown scheduling mode */
            res = FALSE;
        break;
    }

    return res;
}


static gboolean
gst_my_filter_activate (GstPad *pad, GstObject *parent){
    GstQuery *query;
    gboolean pull_mode;
    /* first check what upstream scheduling is supported */
    query = gst_query_new_scheduling ();
    if (!gst_pad_peer_query (pad, query)) {
        gst_query_unref (query);
        goto activate_push;
    }
    /* see if pull-mode is supported */
    pull_mode = gst_query_has_scheduling_mode_with_flags (
        query,
        GST_PAD_MODE_PULL, /*GST_SCHEDULING_FLAG_SEQUENTIAL*/ GST_SCHEDULING_FLAG_SEEKABLE);
    gst_query_unref (query);
    if (!pull_mode)
        goto activate_push;

    /* now we can activate in pull-mode. GStreamer will also
     * activate the upstream peer in pull-mode*/
    g_print("OK: pull mode\n");
    return gst_pad_activate_mode (pad, GST_PAD_MODE_PULL, TRUE);
activate_push:

    {
        g_print("WARNING: push mode\n");
        /*something not right, we fallback to push-mode*/
        return gst_pad_activate_mode (pad, GST_PAD_MODE_PUSH, TRUE);
    }
}

#if 1
static gboolean gst_my_filter_sink_query (GstPad *pad, GstObject *parent, GstQuery *query){
    gboolean ret;
    GstPanorama *filter = GST_PANORAMA(parent);

    GstAllocationParams params;
    GstCaps *caps;
    size_t size,min,max;
//  GstQuery *query;
    GstStructure *structure;
  


    switch (GST_QUERY_TYPE (query)) {
        case GST_QUERY_CAPS:
        //    g_print("caps..a.......................kkkkkkkkkkkkkkkkkk...........");
        break;

        

///////////////////////

        case GST_QUERY_ALLOCATION:{
       
#if 1
            g_print("alocation query ----------------------------------------------------------------------------------\n");
            GstBufferPool *pool;
            GstStructure *config;
            GstCaps *caps;
            
            gboolean need_pool;
            guint size;
            GstAllocator *allocator;
            GstAllocationParams params;

            gst_allocation_params_init (&params);

            gst_query_parse_allocation (query, &caps, &need_pool);

            if (!caps) {
                GST_ERROR ("allocation query without caps");
                return TRUE;
            }

            // if (!gst_video_info_from_caps (&info, caps)) {
            //     g_print ("allocation query with invalid caps");
            //     return TRUE;
            // }

            //g_mutex_lock (state->queue_lock);
            pool = NULL;//state->pool ? gst_object_ref (state->pool) : NULL;
            //g_mutex_unlock (state->queue_lock);

            if (pool) {
                GstCaps *pcaps;

                /* we had a pool, check caps */

                config = gst_buffer_pool_get_config (pool);
                gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);
                g_print ("check existing pool caps %" GST_PTR_FORMAT
                        " with new caps %" GST_PTR_FORMAT, pcaps, caps);

                if (!gst_caps_is_equal (caps, pcaps)) {
                    g_print ("pool has different caps");
                    /* different caps, we can't use this pool */
                    gst_object_unref (pool);
                    pool = NULL;
                }
            
                gst_structure_free (config);
            }

            g_print ("pool %p", pool);
            if (pool == NULL && need_pool) {
                GstVideoInfo info;

                // if (!gst_video_info_from_caps (&info, caps)) {
                //     g_print ("allocation query has invalid caps %"
                //         GST_PTR_FORMAT, caps);
                //     return TRUE;
            
                // }

                g_print ("create new pool");
                pool = gst_buffer_pool_new();// (state, state->display);
                g_print ("done create new pool %p", pool);
                /* the normal size of a frame */
                //size = filter->inwidth*filter->inheight*4;
                size = filter->inbuffersize;

                config = gst_buffer_pool_get_config (pool);
                /* we need at least 2 buffer because we hold on to the last one */
                gst_buffer_pool_config_set_params (config, caps, size, 4, 4);
                gst_buffer_pool_config_set_allocator (config, NULL, &params);
                if (!gst_buffer_pool_set_config (pool, config)) {
                    gst_object_unref (pool);
                    g_print ("failed to set pool configuration");
                    return TRUE;
                }
            }

            if (pool) {
                /* we need at least 2 buffer because we hold on to the last one */
                gst_query_add_allocation_pool (query, pool, size, 2, 0);
                gst_object_unref (pool);
            }

            /* First the default allocator */
            // if (!gst_egl_image_memory_is_mappable ()) {
            //     allocator = gst_allocator_find (NULL);
            //     gst_query_add_allocation_param (query, allocator, &params);
            //     gst_object_unref (allocator);
            // }

            allocator = filter->cudaallocator; //gst_egl_image_allocator_obtain ();
            g_print ("Allocator obtained %p", allocator);
            gst_query_add_allocation_param (query, allocator, &params);

            // if (!gst_egl_image_memory_is_mappable ())
            //     params.flags |= GST_MEMORY_FLAG_NOT_MAPPABLE;
            //     gst_query_add_allocation_param (query, allocator, &params);
            //     gst_object_unref (allocator);

            //     gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
            //     gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);
            //     gst_query_add_allocation_meta (query,
            //     GST_VIDEO_GL_TEXTURE_UPLOAD_META_API_TYPE, NULL);

            //     g_print ("done alocation");
            //     return TRUE;
            // }
#endif
        }

        break;



        ///////////////////////////////

        default:
        /*just call the default handler*/
            ret = gst_pad_query_default (pad, parent, query);
        break;
        
        return ret;
    }
}

#endif


static gboolean
gst_panorama_src_event (GstPad * pad, GstObject * parent, GstEvent * event){
    GstPanorama *filter;
    gboolean ret;
    double x,y;
    char *type;
    filter = GST_PANORAMA (parent);
//g_info("event.. %d \n", GST_EVENT_TYPE (event));
#if 1
    switch (GST_EVENT_TYPE (event)) {
        case GST_EVENT_NAVIGATION:{
            const GstStructure *s = gst_event_get_structure (event);
            gint fps_n, fps_d;

            type = gst_structure_get_string (s, "event");
            if (g_str_equal (type, "mouse-move")) {
                gst_structure_get_double (s, "pointer_x", &x);
                gst_structure_get_double (s, "pointer_y", &y);
                g_error("x: %f y: %f\n", x,y);
                g_mutex_lock(&filter->processing[filter->ind]);
                gstcuda_update_matrix(filter->cudapriv, deg_to_rad(90), deg_to_rad(180.0f*(y/filter->outheight)), deg_to_rad(360.0f*(x/filter->outwidth)));
                g_mutex_unlock(&filter->processing[filter->ind]);
//ret = gst_pad_push_event (filter->src_pad, event);
            } else if (g_str_equal (type, "mouse-button-press")) {
               
            } else if (g_str_equal (type, "mouse-button-release")) {
               
            }
            break;
        }
#if 1
        case GST_EVENT_CAPS:{
            GstCaps *caps;
            const GstStructure *str;
            const gchar *format;
            gst_event_parse_caps (event, &caps);

            str = gst_caps_get_structure (caps, 0);
            if (!gst_structure_get_int (str, "width", &filter->outwidth) ||
                !gst_structure_get_int (str, "height", &filter->outheight)) {
                g_print ("No input width/height available\n");
                return FALSE;

            }else{
                g_print("sizes: %d %d\n", filter->outwidth, filter->outheight);
                format  = gst_structure_get_string(str, "format");
                if (format){
                    if (!strcmp(format, "xBGR")){
                        filter->outbuffersize = 4*filter->outwidth*filter->outheight;
                        filter->outcolorspace = 0;
                    }else if (!strcmp(format, "I420")){
                        filter->outcolorspace = 1;
                        if (filter->yuvtogray8){
                            filter->outbuffersize = filter->outwidth*filter->outheight;
                        }else{
                            filter->outbuffersize = 3*filter->outwidth*filter->outheight/2;
                        }
                        
                    }else if (!strcmp(format, "GRAY8")){
                        filter->outcolorspace = 2;
                        filter->outbuffersize = filter->outwidth*filter->outheight;
                    }else{
                        g_print ("format NOT supported\n");
                        return FALSE;
                    }
                }else{
                    g_print ("format NOT available\n");
                    return FALSE;
                }

                

            }
        }
        break;
#endif
        default:
        break;
    }
   // return gst_pad_event_default (pad, event);
#endif
    return gst_pad_push_event (filter->outsrcpad, event);;
}


/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_panorama_init (GstPanorama * filter){

    int i;
    for (i=0;i<SINKPADCNT;i++){

        filter->sinkpads[i] = gst_pad_new_from_static_template (&sink_factory, padnames[i]);
        gst_pad_set_query_function (filter->sinkpads[i], gst_my_filter_sink_query);
        gst_pad_set_event_function (filter->sinkpads[i], GST_DEBUG_FUNCPTR(gst_panorama_sink_event));
        gst_pad_set_activate_function (filter->sinkpads[i], gst_my_filter_activate);
        gst_pad_set_activatemode_function (filter->sinkpads[i], gst_my_filter_activate_pull);
        gst_pad_set_chain_function (filter->sinkpads[i], GST_DEBUG_FUNCPTR(gst_panorama_chain));
        gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpads[i]);
    }
    filter->ind = 0;

    filter->outsrcpad = gst_pad_new_from_static_template (&outsrc_factory, "outsrc");
    gst_pad_set_event_function (filter->outsrcpad, GST_DEBUG_FUNCPTR(gst_panorama_src_event));

    gst_element_add_pad (GST_ELEMENT (filter), filter->outsrcpad);
    g_atomic_int_set(&filter->newframes[filter->ind],0);




    filter->phi = deg_to_rad(DEFAULT_PHI);
    filter->theta = deg_to_rad(DEFAULT_THETA);
    filter->autoupdate = (DEFAULT_MATRIXAUTOUPDATE);
    filter->inbuffersize = (DEFAULT_INBUFFERSIZE);
    filter->yuvtogray8 = 0;
    filter->configured = FALSE;
    

    filter->cudapriv = gstcuda_priv_alloc();
 
    //udate static config
    gstcuda_bmap_config(filter->cudapriv, DEFAULT_BMAPFILE);
    gstcuda_xymap_config(filter->cudapriv, DEFAULT_XYMAPFILE);
    gstcuda_intermap_config(filter->cudapriv, DEFAULT_INTERMAPFILE);

    //get newmask
    g_atomic_int_set(&filter->newframemask,0x0000003F/*newmask*/);

    

    filter->cudaallocator = g_object_new (panorama_allocator_get_type (), NULL);
    PanoramaAllocator *pa = GST_PANORAMA_ALLOCATOR(filter->cudaallocator);
    pa->cudapriv = filter->cudapriv;


    gst_allocator_register ("CudaHostMemoryAllocator", filter->cudaallocator);

}

static void
gst_panorama_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec){

    GstPanorama *filter = GST_PANORAMA (object);
    struct PanoramaVector *vptr;

    switch (prop_id) {
        case PROP_PHI:
            filter->phi = deg_to_rad(g_value_get_int64 (value));
            if (filter->autoupdate){
                g_mutex_lock(&filter->processing[filter->ind]);
                //update matrix
                //gstcuda_update_matrix(deg_to_rad(120), filter->phi, filter->theta);
                //get newmask
                g_atomic_int_set(&filter->newframemask,0x0000001/*newmask*/);
                g_mutex_unlock(&filter->processing[filter->ind]);
            }
        break;

        case PROP_THETA:
            filter->theta = deg_to_rad(g_value_get_int64 (value));
            if (filter->autoupdate){
                g_mutex_lock(&filter->processing[filter->ind]);
                //update matrix
                //gstcuda_update_matrix(deg_to_rad(120), filter->phi, filter->theta);
                //get newmask
                g_atomic_int_set(&filter->newframemask,0x0000001/*newmask*/);
                g_mutex_unlock(&filter->processing[filter->ind]);
            }
        break;

        case PROP_MATRIXUPDATE:
            g_mutex_lock(&filter->processing[filter->ind]);
            //update matrix
            //gstcuda_update_matrix(deg_to_rad(120), filter->phi, filter->theta);
            //get newmask
            g_atomic_int_set(&filter->newframemask,0x0000001/*newmask*/);
            g_mutex_unlock(&filter->processing[filter->ind]);

        break;

        case PROP_MATRIXAUTOUPDATE:
            filter->autoupdate = g_value_get_boolean(value);
        break;

        case PROP_VECTOR:
            vptr = g_value_get_pointer(value);

            filter->phi = deg_to_rad(vptr->phi);
            filter->theta = deg_to_rad(vptr->theta);

            g_mutex_lock(&filter->processing[filter->ind]);
            //update matrix
            //gstcuda_update_matrix(deg_to_rad(120), filter->phi, filter->theta);
            //get newmask
            g_atomic_int_set(&filter->newframemask,0x0000001/*newmask*/);
            g_mutex_unlock(&filter->processing[filter->ind]);

        break;

        case PROP_BMAP:
            g_mutex_lock(&filter->processing[filter->ind]);
            //gstcuda_bmap_config( g_value_get_string(value));
            g_mutex_unlock(&filter->processing[filter->ind]);
        break;

        case PROP_XYMAP:
            g_mutex_lock(&filter->processing[filter->ind]);
            //gstcuda_xymap_config( g_value_get_string(value));
            g_mutex_unlock(&filter->processing[filter->ind]);
        break;
        case PROP_INTERMAP:
             g_mutex_lock(&filter->processing[filter->ind]);
            //gstcuda_xymap_config( g_value_get_string(value));
            g_mutex_unlock(&filter->processing[filter->ind]);
        break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gst_panorama_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
    GstPanorama *filter = GST_PANORAMA (object);

    switch (prop_id) {
        case PROP_PHI:
            g_value_set_int64 (value, filter->phi);
        break;

        case PROP_THETA:
            g_value_set_int64 (value, filter->theta);
        break;
        case PROP_MATRIXUPDATE:
        break;

        case PROP_MATRIXAUTOUPDATE:
            g_value_set_boolean(value, filter->autoupdate);
        break;

        case PROP_VECTOR:
            filter->vector.phi = filter->phi;
            filter->vector.theta = filter->theta;
            g_value_set_pointer(value, &filter->vector);
        break;

        case PROP_BMAP:
            
        break;

        case PROP_XYMAP:
          
        break;

        case PROP_INTERMAP:
        break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}
#if 0
static gboolean gst_panorama_setoutcaps (GstPanorama *filter, GstCaps *caps){

    GstStructure *structure, *othersstructure;

    //int rate, channels;
    gboolean ret;
    //GstCaps *outcaps;
    GstCaps *othercaps;//, *newcaps;
    gint width,height;
    const gchar *format;

    structure = gst_caps_get_structure (caps, 0);

    othercaps = gst_pad_get_allowed_caps (filter->outsrcpad);
    othersstructure = gst_caps_get_structure (othercaps, 0);

    format = gst_structure_get_string (othersstructure, "format");
    if (!format){
        g_print("peer format is missing!\n");

        format = "ARGB";
        gst_structure_set (othersstructure, "format", G_TYPE_STRING, format, NULL);

    }else{
        if (strcmp(format,"ARGB")){
            g_print("peer format is different = %s!\n", format);
            return FALSE;
        }
    }

    ret = gst_structure_get_int (othersstructure, "width", &width);
    if (!ret){
        width = (DEFAULT_OUTWIDTH);
        gst_structure_set (othersstructure, "width", G_TYPE_INT, width, NULL);
        g_print("width is missing!\n");
    }else{
        if ((width<320) || (width>1920)){
            g_print("peer width is different: %d\n",width);
            return FALSE;
        }
    }

    ret = gst_structure_get_int (othersstructure, "height", &height);
    if (!ret){
        height = (DEFAULT_OUTHEIGHT);
        gst_structure_set (othersstructure, "height", G_TYPE_INT, width, NULL);
        g_print("height is missing!\n");
    }else{
        if ((height<240) || (height>1080)){
            g_print("peer height is different: %d\n",height);
            return FALSE;
        }
    }

   // newcaps = gst_caps_merge_structure (othercaps, othersstructure);
    gst_caps_unref (othercaps);
    //gst_pad_fixate_caps (filter->outsrcpad, newcaps);
    if (!gst_pad_set_caps (filter->outsrcpad, othercaps))
        return FALSE;

    //update filter out sizes




    // if (!ret)
    //     return FALSE;

    // outcaps = gst_caps_new_simple ("video/x-raw",
    //     "format", G_TYPE_STRING, "ARGB",
    //      NULL);

    // ret = gst_pad_set_caps (filter->srcpad, outcaps);
    // gst_caps_unref (outcaps);
    g_print("everything is fine!\n");
    return TRUE;
}
#endif 

/* this function handles sink events */
static gboolean
gst_panorama_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstPanorama *filter;
  gboolean ret;
  //gint width,height;
  const GstStructure *str;
  const gchar *format;

  GstStructure *othersstructure;
  GstCaps *othercaps;

  filter = GST_PANORAMA (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

    switch (GST_EVENT_TYPE (event)) {
        case GST_EVENT_CAPS:{
            GstCaps *caps;
            gst_event_parse_caps (event, &caps);

            str = gst_caps_get_structure (caps, 0);
            if (!gst_structure_get_int (str, "width", &filter->inwidth) ||
                !gst_structure_get_int (str, "height", &filter->inheight)) {
                g_print ("No input width/height available\n");
                return FALSE;

            }else{
                g_print("sizes: %d %d\n", filter->inwidth, filter->inheight);
                format  = gst_structure_get_string(str, "format");
                if (format){
                    if (!strcmp(format, "xBGR")){
                        filter->inbuffersize = 4*filter->inwidth*filter->inheight;
                        filter->incolorspace = 0;
                    }else if (!strcmp(format, "I420")){
                        filter->incolorspace = 1;
                       // if (filter->yuvtogray8){
                       //     filter->inbuffersize = filter->inwidth*filter->inheight;
                       // }else{
                            filter->inbuffersize = 3*filter->inwidth*filter->inheight/2;
                       // }
                        
                    }else if (!strcmp(format, "GRAY8")){
                        filter->incolorspace = 2;
                        filter->inbuffersize = filter->inwidth*filter->inheight;
                    }else{
                        g_print ("format NOT supported\n");
                        return FALSE;
                    }
                }else{
                    g_print ("format NOT available\n");
                    return FALSE;
                }
            }

            //ret = gst_panorama_setoutcaps (filter, caps);
            if (filter->configured==FALSE){
                filter->configured=TRUE;
                othercaps = gst_pad_get_allowed_caps (filter->outsrcpad);
                othersstructure = gst_caps_get_structure (othercaps, 0);

                ret = gst_structure_get_int (othersstructure, "width", &filter->outwidth);
                if (ret){
                    g_print("peer width is: %d\n",filter->outwidth);
                }else{
                    filter->outwidth = DEFAULT_OUTWIDTH;
                    gst_structure_set (othersstructure, "width", G_TYPE_INT, filter->outwidth, NULL);
                }

                ret = gst_structure_get_int (othersstructure, "height", &filter->outheight);
                if (ret){
                    g_print("peer height is: %d\n",filter->outheight);
                }else{
                    filter->outheight = DEFAULT_OUTHEIGHT;
                    gst_structure_set (othersstructure, "height", G_TYPE_INT, filter->outheight, NULL);
                }

                format  = gst_structure_get_string(othersstructure, "format");
                if (format){
                    if (!strcmp(format, "xBGR")){
                        if ((filter->incolorspace==1) ||  (filter->incolorspace==2)) {
                            g_print("I420/GRAY8 to xBGR not supported");
                            return FALSE;
                        }
                        filter->outbuffersize = 4*filter->outwidth*filter->outheight;
                        filter->outcolorspace = 0;
                    }else if (!strcmp(format, "I420")){
                        if ((filter->incolorspace==0) || (filter->incolorspace==2)){
                            g_print("xBGR/GRAY8 to I420 not supported");
                            return FALSE;
                        }
                        filter->outcolorspace = 1;
                        //if (filter->yuvtogray8){
                        //    filter->outbuffersize = filter->outwidth*filter->outheight;
                       // }else{
                            filter->outbuffersize = 3*filter->outwidth*filter->outheight/2;
                       // }
                    }else if (!strcmp(format, "GRAY8")){
                        if (filter->incolorspace==0){
                            g_print("xBGR to GRAY8 not supported");
                            return FALSE;
                        }
                        filter->outcolorspace = 2;
                        filter->outbuffersize = filter->outwidth*filter->outheight;
                    }else if (!strcmp(format, "RGBA")){
                        
                        filter->outcolorspace = 3;
                        filter->outbuffersize = 4*filter->outwidth*filter->outheight;
                    }else if (!strcmp(format, "RGB")){
                        
                        filter->outcolorspace = 4;
                        filter->outbuffersize = 3*filter->outwidth*filter->outheight;
                    }else{
                        g_print ("format NOT supported\n");
                        return FALSE;
                    }
                }else{
                    g_print ("format NOT available\n");
                    return FALSE;
                }

                if ((filter->incolorspace==1) && (filter->outcolorspace==2))
                    filter->yuvtogray8=1;
                else
                    filter->yuvtogray8=0;

                gstpano_output_alloc(   filter->cudapriv,
                                        filter->outcolorspace,
                                        filter->yuvtogray8,
                                        filter->outwidth,
                                        filter->outheight);

                 g_print("outt readdddddddddddddddddyyyyyyyyyyyyyyyyyyyyyyyyyyy : %d\n",gst_pad_get_id(pad) );

                  //udate matrix
                gstcuda_update_matrix(filter->cudapriv, deg_to_rad(90), filter->phi, filter->theta);

                
            }

          //  g_print("out str: %s\n",gst_structure_to_string(othersstructure));


            ret = gst_pad_event_default (pad, parent, event);
            if (ret){
                gstcuda_source_alloc(   filter->cudapriv, 
                            filter->incolorspace, 
                            filter->yuvtogray8,
                            filter->inwidth, 
                            filter->inheight, 
                            gst_pad_get_id(pad) );

               

            }else
                g_print("error in pad : %d\n",gst_pad_get_id(pad) );

        }
        break;

        default:
            ret = gst_pad_event_default (pad, parent, event);
        break;
  }



  return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_panorama_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
    GstPanorama *filter;
    GstFlowReturn ret = GST_FLOW_OK;
    int i;
    static GstClockTime timestamp = 0; 
    static GstClockTime oldtime = 0;
    gint padid;
    GstMapInfo info,info2;
    GstBuffer *outbuf;
    gboolean padfound = FALSE;
   //    gpointer outdata;
    filter = GST_PANORAMA (parent);
  //  g_debug("chain on %s\n", GST_PAD_NAME(pad));
    for (i=0;i<6;i++){
        if (!(strcmp(GST_PAD_NAME(pad), padnames[i]))){
            padfound=TRUE;
            padid = gst_pad_get_id(pad);

            if (filter->newframemask & padmasks[i]){
                if (g_mutex_trylock(&filter->processing[filter->ind])){
                    g_mutex_unlock(&filter->processing[filter->ind]);

                    if (g_atomic_int_get(&filter->newframes[filter->ind]) & padmasks[padid]){
                        
                        gst_buffer_unref(buf);
                        
                        return GST_FLOW_OK;
                    }
                    

                    g_atomic_int_or(&filter->newframes[filter->ind], padmasks[padid]);
                    gst_buffer_map (buf, &info, GST_MAP_READ);
                    gstcuda_update_source(filter->cudapriv, padid, (void *)info.data, filter->ind );

                    gst_buffer_unmap (buf, &info);
                   // g_print("times: %lu %lu\n", GST_BUFFER_PTS(buf),GST_BUFFER_DTS(buf) );
                    
                    gst_buffer_unref(buf);
                    
                    break;
                    

               
                    
                }else{
                    gst_buffer_unref(buf);
                   // g_print("pr..flush 2\n");
                    return GST_FLOW_OK;
                }
                break;
            }else{
                //gst_pad_push (filter->outsrcpad, buf);
                gst_buffer_unref(buf);
              //  g_print("flush 3\n");
                return GST_FLOW_OK;
            }
        }
    }

    if (padfound==FALSE){
        g_error("Pad with name: %s not found\n", GST_PAD_NAME(pad));
        gst_buffer_unref(buf);
        return GST_FLOW_ERROR;
    }

    //Just in case
    g_atomic_int_and(&filter->newframes[filter->ind], filter->newframemask);

    if (g_atomic_int_compare_and_exchange(&filter->newframes[filter->ind], filter->newframemask, 0)){
       
        g_mutex_lock(&filter->processing[filter->ind]);
     
        gstcuda_process(filter->cudapriv, filter->ind);
        
        //g_print("processing done. %d\n",filter->outbuffersize);
        //gstcuda_sync_all(filter->cudapriv);
        outbuf = gst_buffer_new_allocate (filter->cudaallocator, filter->outbuffersize, NULL);
        gst_buffer_map (outbuf, &info2, GST_MAP_WRITE);
        //printf("start readout...........................................................kk\n");

        gstcuda_get_output(filter->cudapriv, (void *)info2.data, filter->ind);
        //gstcuda_sync_all(filter->cudapriv);
        gst_buffer_unmap (outbuf, &info2);

       //  GST_BUFFER_PTS (outbuf) = timestamp;
       // GST_BUFFER_DURATION (outbuf) = gst_util_uint64_scale_int (1, GST_SECOND, 30);
       // timestamp += GST_BUFFER_DURATION (outbuf); 
        printf("framerat11e: %d\n", timestamp - oldtime);
        oldtime = timestamp;

       gst_pad_push(filter->outsrcpad, outbuf);

        
        g_mutex_unlock(&filter->processing[filter->ind]);
    
static double x=0.0;
static double y=320.0;
if (x>640.0){
x=0.0;
} else x = x+1.0;
gstcuda_update_matrix(filter->cudapriv, deg_to_rad(90), deg_to_rad(180.0f*(y/filter->outheight)), deg_to_rad(360.0f*(x/filter->outwidth)));


}else{

    }

    return GST_FLOW_OK;
}


#if 1
GType
gst_panorama_meta_api_get_type (void){

    static volatile GType type;
    static const gchar *tags[] = { "foo", "bar", NULL };
    if (g_once_init_enter (&type)) {
        GType _type = gst_meta_api_type_register ("GstPanoramaMetaAPI", tags);
        g_once_init_leave (&type, _type);
    }
    return type;
}


static gboolean
gst_panorama_meta_init (GstMeta *meta, gpointer params, GstBuffer *buffer){
    GstPanoramaMeta *emeta = (GstPanoramaMeta *) meta;

    emeta->age = 0;
    emeta->name = NULL;
    return TRUE;
}

static gboolean gst_panorama_meta_transform (GstBuffer *transbuf, GstMeta * meta, GstBuffer *buffer, GQuark type, gpointer data){
    GstPanoramaMeta *emeta = (GstPanoramaMeta *) meta;

    /* we always copy no matter what transform */
    gst_buffer_add_panorama_meta (transbuf, emeta->age, emeta->name);
    return TRUE;
}

static void gst_panorama_meta_free (GstMeta *meta, GstBuffer *buffer){
    GstPanoramaMeta *emeta = (GstPanoramaMeta *) meta;

    g_free (emeta->name);
    emeta->name = NULL;
}

const GstMetaInfo *gst_panorama_meta_get_info (void){
    static const GstMetaInfo *meta_info = NULL;
    if (g_once_init_enter (&meta_info)) {
        const GstMetaInfo *mi = gst_meta_register (GST_PANORAMA_META_API_TYPE,"GstPanoramaMeta",
                        sizeof (GstPanoramaMeta),
                        gst_panorama_meta_init,
                        gst_panorama_meta_free,
                        gst_panorama_meta_transform);
        g_once_init_leave (&meta_info, mi);
    }
    return meta_info;
}

GstPanoramaMeta *gst_buffer_add_panorama_meta (GstBuffer *buffer, gint age, const gchar* name){

    GstPanoramaMeta *meta;
    g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
    meta = (GstPanoramaMeta *) gst_buffer_add_meta (buffer, GST_PANORAMA_META_INFO, NULL);
    meta->age = age;
    meta->name = g_strdup (name);

    return meta;
}
#endif
//pin  1721
/* gstreamer looks for this structure to register panoramas
 *
 * exchange the string 'Template panorama' with your panorama description
 */
// GST_PLUGIN_DEFINE (
//     GST_VERSION_MAJOR,
//     GST_VERSION_MINOR,
//     panorama,
//     "Template panorama",
//     panorama_init,
//     VERSION,
//     "LGPL",
//     "GStreamer",
//     "http://gstreamer.net/"
// )

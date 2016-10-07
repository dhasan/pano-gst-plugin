
// gst-launch-1.0 v4l2src device="/dev/video0" ! "video/x-raw-yuv, width=640, height=480, format=(fourcc)I420" ! xvimagesink -v -e

/////////////////////////////////////////////////

#include <gst/gst.h>
#ifdef HAVE_GTK
#include <gtk/gtk.h>
#endif
#include <stdlib.h>

#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
//#define CAPS "video/x-raw,format=I420,width=1024,height=768,pixel-aspect-ratio=1/1"
//#define CAPS "'video/x-raw(memory:NVMM), width=(int)1024, height=(int)768, format=(string)I420, framerate=(fraction)30/1, pixel-aspect-ratio=1/1'"

#include 	<fcntl.h>
#include 	<stdio.h>

struct panodata{

	GMainLoop *loop;
	GstElement *pipeline;
	GstBus *bus;

	GstElement *panorama;
	GstElement *videoconvert;
	GstElement *xvimagesink;

	GstElement *v4l2source[6];
	GstElement *jpegparse[6];
	GstElement *jpegdec[6];

};

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
	struct panodata *pano = (struct panodata *)data;
  	GMainLoop *loop = pano->loop;

  	switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
    	g_print ("End of stream\n");
      	g_main_loop_quit (loop);
	break;

    case GST_MESSAGE_ERROR: {
 		gchar  *debug;
      	GError *error;

      	gst_message_parse_error (msg, &error, &debug);
      	g_free (debug);

     	g_printerr ("Error: %s\n", error->message);
      	g_error_free (error);

     	g_main_loop_quit (loop);
    break;
    }

    default:
    	g_print ("Bus call :%d\n", GST_MESSAGE_TYPE (msg));
    break;
  }

  return TRUE;
}

int main (int argc, char *argv[]){

	const gchar *v4l2srcname[6] = {"v4l2src0", "v4l2src1", "v4l2src2", "v4l2src3", "v4l2src4", "v4l2src5"};
	const gchar *v4l2loc[6] = {"/dev/video0", "/dev/video1", "/dev/video2", "/dev/video3", "/dev/video4", "/dev/video5"};
	const gchar *jpegdecname[6] ={"jpegdec0", "jpegdec1", "jpegdec2", "jpegdec3", "jpegdec4", "jpegdec5"};
	const gchar *jpegparsename[6] = {"jpegparse0", "jpegparse1", "jpegparse2", "jpegparse3", "jpegparse4", "jpegparse5"};

	const gchar *panopadnames[6] ={"frontsink", "rightsink", "leftsink", "backsink", "topsink", "bottomsink"};
	GstCaps *jcaps, *vcaps, *outcaps;

	guint i;
	guint bus_watch_id;
	struct panodata pano;

	gst_init (&argc, &argv);
	pano.loop = g_main_loop_new (NULL, FALSE);
	g_assert(pano.loop);
	pano.pipeline = gst_pipeline_new ("pipeline");
	g_assert(pano.pipeline);

	/* we add a message handler */
  	pano.bus = gst_pipeline_get_bus (GST_PIPELINE (pano.pipeline));
  	g_assert(pano.bus);
  	bus_watch_id = gst_bus_add_watch (pano.bus, bus_call, &pano);
  	gst_object_unref (pano.bus);

	g_print("start....\n");
	printf("start.\n");

	jcaps = gst_caps_new_simple ("image/jpeg",
			"width", G_TYPE_INT, 1920,
			"height", G_TYPE_INT, 1080,
			"framerate", GST_TYPE_FRACTION, 30, 1,
			NULL);

	vcaps = gst_caps_new_simple ("video/x-raw",
			"format", G_TYPE_STRING, "xBGR",
			"width", G_TYPE_INT, 1920,
			"height", G_TYPE_INT, 1080,
			"framerate", GST_TYPE_FRACTION, 30, 1,
			NULL);

	outcaps = gst_caps_new_simple ("video/x-raw",
			"format", G_TYPE_STRING, "xBGR",
			"width", G_TYPE_INT, 640,
			"height", G_TYPE_INT, 480,
			"framerate", GST_TYPE_FRACTION, 30, 1,
			NULL);

	pano.panorama = gst_element_factory_make ("panorama", "panorama0");
	g_assert(pano.panorama);
	gst_bin_add(GST_BIN (pano.pipeline), pano.panorama);

	pano.videoconvert = gst_element_factory_make ("videoconvert", "videoconvert0");
	g_assert(pano.videoconvert);
	gst_bin_add(GST_BIN (pano.pipeline), pano.videoconvert);

	pano.xvimagesink = gst_element_factory_make ("ximagesink", "ximagesink0");
	g_assert(pano.xvimagesink);
	gst_bin_add(GST_BIN (pano.pipeline), pano.xvimagesink);

#if 0
	for (i=0;i<6;i++){
		pano.v4l2source[i] = gst_element_factory_make ("v4l2src", v4l2srcname[i]);
		g_assert(pano.v4l2source[i]);
		//g_object_set(G_OBJECT (pano.v4l2source[i]), "location", "hd.jpg", NULL);
		g_object_set(G_OBJECT (pano.v4l2source[i]), "device", v4l2loc[i],NULL);
		gst_bin_add(GST_BIN (pano.pipeline), pano.v4l2source[i]);

		pano.jpegparse[i] = gst_element_factory_make ("jpegparse", jpegparsename[i]);
		g_assert(pano.jpegparse[i]);
		gst_bin_add(GST_BIN (pano.pipeline), pano.jpegparse[i]);


		pano.jpegdec[i] = gst_element_factory_make ("jpegdec", jpegdecname[i]);
		g_assert(pano.jpegdec[i]);
		gst_bin_add(GST_BIN (pano.pipeline), pano.jpegdec[i]);

		gst_element_link_filtered(pano.v4l2source[i], pano.jpegparse[i], jcaps);
		gst_element_link(pano.jpegparse[i], pano.jpegdec[i]);
		gst_element_link_pads_filtered (pano.jpegdec[i],NULL, pano.panorama, panopadnames[i], vcaps);
	}
#else
	for (i=0;i<6;i++){
		pano.v4l2source[i] = gst_element_factory_make ("videotestsrc", v4l2srcname[i]);
		g_assert(pano.v4l2source[i]);
//		g_object_set(G_OBJECT (pano.v4l2source[i]), "is-live", TRUE, NULL);

		gst_bin_add(GST_BIN (pano.pipeline), pano.v4l2source[i]);
		if (gst_element_link_pads_filtered (pano.v4l2source[i], "src", pano.panorama, panopadnames[i], vcaps)==FALSE){
			g_print("cant link %s\n", panopadnames[i]);
		}
	}

#endif

	gst_element_link_filtered(pano.panorama, pano.videoconvert,outcaps);
	gst_element_link(pano.videoconvert, pano.xvimagesink );


	// gst_caps_unref (jcaps);
	// gst_caps_unref (vcaps);
	// gst_caps_unref (outcaps);

	gst_element_set_state (pano.pipeline, GST_STATE_PLAYING);
	g_print("running...\n");
	g_main_loop_run (pano.loop);
    g_print ("Going out..\n");

	gst_element_set_state (pano.pipeline, GST_STATE_NULL);

	gst_object_unref (pano.pipeline);
	g_source_remove (bus_watch_id);
	exit(0);

}


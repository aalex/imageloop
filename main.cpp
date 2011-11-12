// sudo apt-get install libgstreamer0.10-dev build-essential 
// gst-launch -v filesrc location=example.jpg ! decodebin ! freeze ! ffmpegcolorspace ! xvimagesink

#include <gst/gst.h>
#include <glib.h>
#include <lo/lo.h>
#include <string>

static gboolean on_bus_message(GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop *) data;
    switch (GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_EOS:
            g_print("End-of-stream\n");
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_ERROR:
        {
            gchar *debug_string = NULL;
            GError *err = NULL;
            gst_message_parse_error(msg, &err, &debug_string);
            g_print("Error: %s\n", err->message);
            g_error_free(err);
            if (debug_string)
            {
                g_print("Debug details: %s\n", debug_string);
                g_free(debug_string);
            }
            g_main_loop_quit(loop);
            break;
         }
        default:
            break;
    }
    return TRUE;
}


struct App
{
    GstElement *pipeline;
    GstElement *source;
    GstElement *jpg_decoder;
    GstElement *freeze;
    GstElement *colorspace;
    GstElement *sink;
    GMainLoop *loop;
    lo_server server;
};

static void update_image(App *app, const char *filename)
{
    gst_element_set_state(app->pipeline, GST_STATE_READY);
    g_object_set(G_OBJECT(app->source), "location", filename, NULL);
    gst_element_set_state(app->pipeline, GST_STATE_PLAYING);
}

static gboolean on_idle(gpointer data)
{
    App *app = (App *) data;
    lo_server_recv_noblock(app->server, 0);
    return TRUE;
}

void error(int num, const char *msg, const char *path)
{
    g_print("liblo server error %d in path %s: %s\n", num, path, msg);
}

int imagename_handler(const char *path, const char *types, lo_arg **argv,
                int argc, void *data, void *user_data)
{
    App *app = (App *) user_data;
    update_image(app, static_cast<const char *>((&argv[0]->s)));
    fflush(stdout);
    return 0;
}

int main(int argc, char *argv[])
{
    App app;
    app.pipeline = NULL;
    app.source = NULL;
    app.jpg_decoder = NULL;
    app.freeze = NULL;
    app.colorspace = NULL;
    app.sink = NULL;
    app.loop = g_main_loop_new(NULL, FALSE);
    std::string initial_image("default.jpg");
  
    // INIT
    gst_init(&argc, &argv);

    // Create gstreamer elements
    app.pipeline = gst_pipeline_new("image-player");
    app.source = gst_element_factory_make("filesrc", "file-source");
    app.jpg_decoder = gst_element_factory_make("jpegdec", "jpg-decoder"); // TODO: decodebin
    app.freeze = gst_element_factory_make("imagefreeze", "freeze");
    app.colorspace = gst_element_factory_make("ffmpegcolorspace", "colorspace");
    app.sink = gst_element_factory_make("xvimagesink", "imagesink");

    gst_bin_add_many(GST_BIN(app.pipeline), app.source, app.jpg_decoder, app.freeze, app.colorspace, app.sink, NULL);
    gst_element_link_many(app.source, app.jpg_decoder, app.freeze, app.colorspace, app.sink, NULL);

    // watch for messages on the pipeline's bus (note that this will only
    // work like this when a GLib main loop is running)
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(app.pipeline));
    gst_bus_add_watch(bus, on_bus_message, app.loop);
    gst_object_unref(bus);

    // set initial image
    g_print("Now playing: %s\n", initial_image.c_str());
    gst_element_set_state(app.pipeline, GST_STATE_READY);
    g_object_set(G_OBJECT(app.source), "location", initial_image.c_str(), NULL);

    // Set the pipeline to "playing" state
    gst_element_set_state(app.pipeline, GST_STATE_PLAYING);

    // OSC
    app.server = lo_server_new("17777", error);
    lo_server_add_method(app.server, "/toon/playhead/imagename", "s", imagename_handler, &app);
    g_idle_add(on_idle, (gpointer) &app);

    // MAIN LOOP
    g_print("Let's do it!!!!\n");
    g_main_loop_run(app.loop);

    // CLEANUP
    gst_element_set_state(app.pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(app.pipeline));
    return 0;
}


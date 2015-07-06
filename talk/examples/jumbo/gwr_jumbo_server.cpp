#include <glibmm.h>
#include <giomm.h>
#include <webrtc/base/ssladapter.h>
#include <webrtc/base/json.h>
#include <glib-unix.h>
#include <libsoup/soup.h>
#include <webrtc/base/thread.h>

#include "umbo_debug.h"
#include "peer_manager.h"

static const Glib::ustring program_string = "- GWR Jumbo Server";
static Glib::ustring jumbo_server_address = "ss0000.umbocv.com";
static int jumbo_server_port = 3443;
static bool is_verbose_mode = false;

static Glib::RefPtr<Glib::MainLoop> main_loop;
static SoupWebsocketConnection *conn = NULL;
static PeerManager *peer_manager = NULL;

static bool parseOptionContext(int argc, char **argv)
{
    Glib::OptionContext option_context(program_string);
    Glib::OptionGroup option_group("application options", "description of application options");
    {
        Glib::OptionEntry entry0;
        entry0.set_short_name('a');
        entry0.set_long_name("address");
        entry0.set_description("Jumbo server address (default: " + jumbo_server_address + ")");
        option_group.add_entry(entry0, jumbo_server_address);

        Glib::OptionEntry entry1;
        entry1.set_short_name('p');
        entry1.set_long_name("port");
        entry1.set_description("Jumbo server port (default: 3001)");
        option_group.add_entry(entry1, jumbo_server_port);

        Glib::OptionEntry entry2;
        entry2.set_short_name('v');
        entry2.set_long_name("verbose");
        entry2.set_description("Enable verbose mode (default: disable)");
        option_group.add_entry(entry2, is_verbose_mode);
    }
    option_context.set_main_group(option_group);
    try {
        option_context.parse(argc, argv);
        return true;
    }
    catch (const Glib::Error &ex) {
        UMBO_CRITICAL("Options parsing failure: %s", ex.what().c_str());
        return false;
    }
}

static gboolean on_sig_handler(gpointer userdata)
{
    UMBO_DBG("Quit");
    main_loop->quit();
    return G_SOURCE_REMOVE;
}

static void process_jumbo_message(const std::string jumbo_message)
{
    Json::Reader reader;
    Json::Value  jmessage;
    if (!reader.parse(jumbo_message, jmessage)) {
        UMBO_WARN("Fail to parse jumbo message");
        return;
    }
    //UMBO_DBG("jmessage: %s", rtc::JsonValueToString(jmessage).c_str());

    std::string message_type;
    g_return_if_fail(rtc::GetStringFromJsonObject(jmessage, "message_type", &message_type));
    {
        if (message_type == "response") {
            std::string response_type;
            g_return_if_fail(rtc::GetStringFromJsonObject(jmessage, "response_type", &response_type));
            UMBO_DBG("response_type: %s", response_type.c_str());
        }
        else if (message_type == "request") {
            std::string request_type;
            g_return_if_fail(rtc::GetStringFromJsonObject(jmessage, "request_type", &request_type));
            {
                UMBO_DBG("request_type: %s", request_type.c_str());
                if (request_type == "sdp") {
                    std::string usersession_id;
                    g_return_if_fail(rtc::GetStringFromJsonObject(jmessage, "usersession_id", &usersession_id));

                    Json::Value jpayload;
                    g_return_if_fail(rtc::GetValueFromJsonObject(jmessage, "payload", &jpayload));

                    peer_manager->setOffser(usersession_id, rtc::JsonValueToString(jpayload));
                }
                else if (request_type == "hangup") {
                    std::string usersession_id;
                    g_return_if_fail(rtc::GetStringFromJsonObject(jmessage, "usersession_id", &usersession_id));

                    peer_manager->deletePeerConnection(usersession_id);
                }
            }
        }
        else {
            UMBO_WARN("Fail to parse jumbo message");
        }
    }
}

static void send_jumbo_message(const std::string &type, const Json::Value &jumbo_message)
{
    Json::Value jmessage;
    jmessage["message_type"] = "response";
    jmessage["response_type"] = type;
    jmessage["payload"] = jumbo_message;

    std::string init = rtc::JsonValueToString(jmessage);
    UMBO_DBG("msg: %s", init.c_str());
    soup_websocket_connection_send_text(conn, init.c_str());
}

static void on_message(SoupWebsocketConnection *conn, gint type, GBytes *message, gpointer data)
{
    if (type == SOUP_WEBSOCKET_DATA_TEXT) {
        gsize sz;
        const gchar *ptr;

        ptr = (const gchar *) g_bytes_get_data(message, &sz);
        process_jumbo_message(ptr);
    }
    else if (type == SOUP_WEBSOCKET_DATA_BINARY) {
        g_print("Received binary data (not shown)\n");
    }
    else {
        g_print("Invalid data type: %d\n", type);
    }
}

static void on_close(SoupWebsocketConnection *conn, gpointer data)
{
    soup_websocket_connection_close(conn, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
    g_print("WebSocket connection closed\n");
}

static void on_connection(SoupSession *session, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;

    conn = soup_session_websocket_connect_finish(session, res, &error);
    if (error) {
        g_print("Error: %s\n", error->message);
        g_error_free(error);
        main_loop->quit();
        return;
    }

    g_signal_connect(conn, "message", G_CALLBACK(on_message), NULL);
    g_signal_connect(conn, "closed",  G_CALLBACK(on_close),   NULL);

    // Register to jumbo server
    {
        Json::Value jmessage;
        jmessage["message_type"] = "request";
        jmessage["request_type"] = "initialize";
        jmessage["source_type"] = "Camera";
        jmessage["camera_id"] = "david0000";

        Json::StyledWriter writer;
        std::string init = writer.write(jmessage);
        soup_websocket_connection_send_text(conn, init.c_str());
    }
}

int main (int argc, char **argv)
{
    Gio::init();

    // Setup program command line context
    if (!parseOptionContext(argc, argv)) {
        return 1;
    }

    rtc::LogMessage::LogToDebug((is_verbose_mode) ? rtc::INFO : rtc::LERROR);
    rtc::LogMessage::LogTimestamps();
    rtc::LogMessage::LogThreads();
    rtc::InitializeSSL();

    // Create the soup session and message with WSS
    SoupSession *session = NULL;
    SoupMessage *msg = NULL;
    {
        gchar *uri = NULL;
        session = soup_session_new();

        // Trick to enable the wss support
        const gchar *wss_aliases[] = { "wss", NULL };
        g_object_set(session, SOUP_SESSION_HTTPS_ALIASES, wss_aliases, NULL);

        // Turn off CA checking
        g_object_set(session, SOUP_SESSION_SSL_STRICT, FALSE, NULL);

        uri = g_strdup_printf("%s://%s:%d", "wss", jumbo_server_address.c_str(), jumbo_server_port);
        msg = soup_message_new(SOUP_METHOD_GET, uri);
        g_free(uri);
    }

    soup_session_websocket_connect_async (
        session,
        msg,
        NULL, NULL, NULL,
        (GAsyncReadyCallback)on_connection,
        NULL
    );

    // webrtc server
    peer_manager = new PeerManager("stun.l.google.com:19302", send_jumbo_message);

    // Create and start the main loop
    main_loop = Glib::MainLoop::create();
    g_unix_signal_add(SIGINT, (GSourceFunc)on_sig_handler, NULL);
    main_loop->run();

    // Clean up
    delete peer_manager;

    rtc::CleanupSSL();

    return 0;
}
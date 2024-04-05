#include <CivetServer.h>
#include <string>
#include <mutex>
#include <unordered_set>
#include <cstring>

#include <unistd.h>

#define WS_PORT "7681"
#define DOCUMENT_ROOT "."

CivetServer *server; 

class WsStartHandler : public CivetHandler
{
public:
    bool
    handleGet(CivetServer *server, struct mg_connection *conn)
    {

        mg_printf(conn,
                  "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: "
                  "close\r\n\r\n");

        mg_printf(conn, "<!DOCTYPE html>\n");
        mg_printf(conn, "<html>\n<head>\n");
        mg_printf(conn, "<meta charset=\"UTF-8\">\n");
        mg_printf(conn, "<title>Embedded websocket example</title>\n");

        /* mg_printf(conn, "<script type=\"text/javascript\"><![CDATA[\n"); ...
         * xhtml style */
        mg_printf(conn, "<script>\n");
        mg_printf(
            conn,
            "var i=0\n"
            "function load() {\n"
            "  var wsproto = (location.protocol === 'https:') ? 'wss:' : 'ws:';\n"
            "  connection = new WebSocket(wsproto + '//' + window.location.host + "
            "'/websocket');\n"
            "  websock_text_field = "
            "document.getElementById('websock_text_field');\n"
            "  connection.onmessage = function (e) {\n"
            "    websock_text_field.innerHTML=e.data;\n"
            "    i=i+1;"
            "    connection.send(i);\n"
            "  }\n"
            "  connection.onerror = function (error) {\n"
            "    alert('WebSocket error');\n"
            "    connection.close();\n"
            "  }\n"
            "}\n");
        /* mg_printf(conn, "]]></script>\n"); ... xhtml style */
        mg_printf(conn, "</script>\n");
        mg_printf(conn, "</head>\n<body onload=\"load()\">\n");
        mg_printf(
            conn,
            "<div id='websock_text_field'>No websocket connection yet</div>\n");
        mg_printf(conn, "</body>\n</html>\n");

        return 1;
    }
};

class WebSocketHandler : public CivetWebSocketHandler
{
	virtual bool handleConnection(CivetServer *server,
	                              const struct mg_connection *conn) {
		printf("WS connected\n");
		return true;
	}

	virtual void handleReadyState(CivetServer *server,
	                              struct mg_connection *conn) {
		printf("WS ready\n");

		const char *text = "Hello from the websocket ready handler";
		mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, text, strlen(text));
	}

	virtual bool handleData(CivetServer *server,
	                        struct mg_connection *conn,
	                        int bits,
	                        char *data,
	                        size_t data_len) {
		printf("WS got %lu bytes: ", (long unsigned)data_len);
		fwrite(data, 1, data_len, stdout);
		printf("\n");

		mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, data, data_len);
		return (data_len<4);
	}

	virtual void handleClose(CivetServer *server,
	                         const struct mg_connection *conn) {
		printf("WS closed\n");
	}
};

WebSocketHandler h_websocket;

int main(){
     mg_init_library(MG_FEATURES_WEBSOCKET);
    if (mg_check_feature(MG_FEATURES_WEBSOCKET) > 0)
        fprintf(stderr, "WebSocket Enable\n");
    else
        fprintf(stderr, "ERROR : WebSocket is not enable\n");
    const char *options[] = {
        "document_root", DOCUMENT_ROOT, "listening_ports", WS_PORT, 0};

    std::vector<std::string> cpp_options;
    for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++)
    {
        cpp_options.push_back(options[i]);
    }
    server = new CivetServer(cpp_options);

    WsStartHandler h_ws;
    server->addHandler("/ws", h_ws);
    server->addWebSocketHandler("/websocket", h_websocket);

    while (true) sleep(1);

    return 0;
}

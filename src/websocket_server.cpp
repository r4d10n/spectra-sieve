#include <CivetServer.h>
#include <string>
#include <mutex>
#include <unordered_set>
#include <cstring>

#include <unistd.h>

#define WS_PORT "8000"
#define DOCUMENT_ROOT "./web"

CivetServer *server; 

class WebSocketHandler : public CivetWebSocketHandler
{

    private:
        /// Lock protecting \c connections_.
        std::mutex connectionsLock_;

        /// Set of connected clients to the entry point.
        std::unordered_set<mg_connection *> connections_;

        std::vector<std::string> update_params;

	virtual bool handleConnection(CivetServer *server,
	                              const struct mg_connection *conn) {
		fprintf(stderr,"WS connected\n");
		return true;
	}

	virtual void handleReadyState(CivetServer *server,
	                              struct mg_connection *conn) {
		fprintf(stderr,"WS ready\n");

        std::lock_guard<std::mutex> l(connectionsLock_);
        connections_.insert(conn);

		//const char *text = "Hello from the websocket ready handler";
        //const char *text = "{\"s\":[-68.0012,-64.5534,-65.1433,-68.9418,-66.0683,-61.6296,-60.9567,-63.6326,-66.8624,-68.6448,-76.8832,-74.2558,-77.083,-75.7519,-75.2008,-77.2711,-80.5329,-77.1424,-70.0737,-67.8056,-65.1909,-64.3818,-66.0104,-65.6986,-66.168,-67.6362,-68.1318,-72.1865,-68.6789,-63.2571,-62.3147,-67.5427,-82.5575,-89.2514,-79.3657,-72.7232,-68.699,-71.7241,-85.6541,-72.4469,-73.3334,-82.92,-69.7464,-66.0193,-66.1272,-71.496,-76.092,-80.3494,-69.354,-65.1169,-66.4534,-69.435,-67.9983,-66.4733,-69.0543,-74.8944,-78.2817,-73.2233,-69.3474,-66.5051,-63.5431,-64.9767,-70.9085,-73.7748,-79.1526,-75.2567,-67.2545,-67.9559,-77.6258,-77.2006,-70.63,-68.385,-71.9119,-94.4071,-72.8462,-68.3015,-68.4888,-69.3384,-68.8638,-68.4915,-69.9324,-76.5095,-79.1246,-73.453,-74.9975,-85.3698,-75.5896,-80.6621,-82.7451,-73.4122,-81.6242,-69.0919,-65.3815,-71.7654,-73.2777,-72.6207,-68.9982,-62.6431,-62.0495,-63.0839,-63.0239,-62.5248,-61.997,-62.4879,-68.2551,-72.0228,-65.8741,-70.6571,-78.022,-75.9528,-73.2846,-69.6501,-68.4687,-69.3426,-73.8754,-81.3765,-84.6268,-72.3497,-67.363,-66.7579,-69.4773,-73.614,-78.1701,-75.4444,-79.1229,-75.5527,-68.28,-66.9695,-66.8837,-64.6057,-64.011,-65.998,-69.2232,-67.9858,-67.5749,-69.4132,-69.1028,-69.4229,-71.388,-70.7565,-67.4837,-67.2566,-81.1765,-68.1504,-66.4147,-71.4454,-69.7478,-68.7446,-70.1184,-74.15,-99.1299,-73.515,-68.1163,-68.5201,-71.6456,-73.0594,-68.202,-69.0525,-74.65,-78.3394,-88.1116,-75.6595,-69.9047,-68.7697,-72.3158,-71.8488,-67.876,-68.0406,-68.2985,-64.7883,-64.6474,-70.8768,-77.3941,-75.5344,-75.6284,-72.8509,-74.3121,-75.6098,-71.1342,-73.793,-72.3632,-70.5671,-69.6626,-65.8249,-62.9705,-65.1585,-74.7366,-69.3781,-65.4645,-64.4002,-64.7881,-69.5401,-82.0813,-71.0211,-71.8966,-72.3212,-68.6766,-69.1643,-75.04,-75.6299,-77.9862,-75.5485,-65.3846,-63.5869,-64.2508,-66.0275,-71.2358,-69.3346,-68.0679,-74.6544,-72.0287,-68.4009,-70.6468,-71.61,-65.3215,-62.836,-64.3747,-66.5033,-65.2063,-64.8593,-69.1955,-71.4182,-67.3574,-66.5978,-69.5469,-66.0821,-65.8384,-68.0764,-69.0295,-73.5812,-69.385,-66.4737,-67.4335,-69.3095,-73.5456,-85.0922,-68.3172,-65.9259,-66.765,-65.9784,-67.5042,-85.9043,-68.9488,-66.3569,-67.4413,-67.1691,-67.4223,-75.7973,-68.9825,-64.4217,-71.6543,-68.1862,-66.3624,-74.4271,-71.9761,-66.8447,-69.1919,-61.2735,-56.6885,-59.0376,-70.2485,-72.5162,-61.0423,-52.8288,-48.778,-49.4876,-54.2556,-60.6855,-68.6933,-75.5846,-83.1972,-72.0647,-65.8284,-64.2935,-69.245,-63.8665,-59.0721,-62.4829,-71.7241,-60.1797,-60.6552,-70.3441,-82.9559,-76.9213,-63.5178,-57.8318,-58.0424,-65.6584,-68.8319,-80.0154,-64.0123,-59.5225,-57.643,-59.0215,-63.7025,-70.6456,-67.9556,-60.7013,-58.3198,-60.0929,-63.2674,-60.2831,-57.375,-58.2604,-59.7482,-57.3825,-58.3799,-66.3888,-69.5667,-63.3955,-61.4557,-61.9675,-65.4477,-72.8035,-72.5212,-62.0332,-57.2698,-56.3324,-57.9338,-59.9548,-59.2338,-57.6287,-57.7981,-60.0023,-68.6746,-67.4154,-64.6157,-63.7812,-68.3406,-72.5627,-61.5878,-58.7837,-59.5117,-57.9195,-56.5059,-58.0892,-64.9818,-70.4727,-66.8869,-66.2995,-71.5786,-63.8103,-57.3483,-57.0575,-57.6637,-58.0079,-60.888,-71.6656,-73.1277,-69.7051,-75.5351,-75.9108,-69.6147,-67.283,-68.3622,-74.9961,-74.2275,-69.1791,-66.9643,-67.357,-68.1653,-62.5798,-61.2036,-62.532,-62.8723,-63.5715,-61.9552,-62.5234,-63.9313,-65.218,-62.4959,-55.1066,-51.6837,-52.2978,-58.2625,-68.3815,-65.8523,-65.3027,-72.9921,-64.9857,-76.9814,-63.7182,-60.1248,-60.7531,-67.8225,-67.6252,-61.5538,-64.6461,-80.7448,-67.7025,-59.7678,-58.1896,-59.1546,-63.7374,-65.7459,-59.6716,-56.9197,-58.6022,-64.0654,-77.4222,-66.8538,-64.0025,-66.2579,-75.3277,-69.4969,-68.2025,-75.1772,-71.3108,-69.0948,-70.0803,-74.2117,-80.7494,-70.7881,-66.8853,-68.8052,-69.7663,-64.0793,-55.7656,-51.3749,-52.0244,-56.6578,-60.5878,-62.787,-65.6764,-70.0751,-75.6523,-74.153,-70.7803,-70.2851,-74.6187,-77.2664,-84.6713,-79.3253,-70.5948,-69.0513,-74.2583,-86.0703,-76.6903,-76.3013,-86.4168,-72.9776,-68.0746,-69.8167,-72.6977,-74.6637,-71.9543,-69.1692,-70.1677,-78.5423,-77.1525,-69.978,-71.9216,-69.2247,-66.1871,-64.5298,-63.9812,-64.8558,-70.9175,-88.8207,-80.4246,-77.4212,-73.1199,-74.4894,-79.6572,-75.388,-79.0036,-83.5516,-77.0123,-66.373,-61.3651,-62.3787,-68.9709,-77.3993,-73.1254,-76.5635,-73.9467,-67.4721,-68.5671,-76.6032,-84.7443,-77.2051,-73.3117,-69.841,-68.4262,-67.7441,-65.4812,-64.7395,-63.9122,-63.9693,-65.7769,-66.6178,-65.4841,-65.5859,-70.9469,-80.3124,-82.3755,-72.6599,-68.0677,-67.6653,-69.7877,-73.2747,-72.5507,-65.9088,-66.3486,-69.5849,-70.0397,-70.2283,-73.5015,-71.4401,-67.2665,-68.5146]}";
        //mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, text, strlen(text));
  	}

	virtual bool handleData(CivetServer *server,
	                        struct mg_connection *conn,
	                        int bits,
	                        char *data,
	                        size_t data_len) {
		fprintf(stderr,"WS got %lu bytes: ", (long unsigned)data_len);
		fwrite(data, 1, data_len, stderr);
		fprintf(stderr,"\n");

        update_params.push_back(data);

		//mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, data, data_len);
		return true;
	}

	virtual void handleClose(CivetServer *server,
	                         const struct mg_connection *conn) {

        std::lock_guard<std::mutex> l(connectionsLock_);

        auto it = connections_.find(const_cast<struct mg_connection *>(conn));
        if (it != connections_.end())
            connections_.erase(it);
		fprintf(stderr,"WS closed\n");
	}  

    public:
        void process_data(float *bin, int fftlen);
        void process_param(char *param);
        void send(char *data, int len, int MessageType);
        std::vector<std::string> get_update_params(); 
};

void WebSocketHandler::process_data(float *bin, int fftlen) {    
    send((char *)bin, fftlen * 4, MG_WEBSOCKET_OPCODE_BINARY);    
}

void WebSocketHandler::process_param(char *param) {
    //fprintf(stderr, "Param: %s\n", param);
    send(param, strlen(param), MG_WEBSOCKET_OPCODE_TEXT);
}

void WebSocketHandler::send(char *data, int len, int MessageType) {
    std::lock_guard<std::mutex> l(connectionsLock_);
    std::vector<struct mg_connection *> errors;
    for (auto it : connections_) {
        if (0 >= mg_websocket_write(it, MessageType, data, len))
            errors.push_back(it);
    }
    for (auto it : errors)
        connections_.erase(it);
}

std::vector<std::string> WebSocketHandler::get_update_params() {
    std::vector<std::string> params(update_params);
    update_params.clear();
    return params;
}; 

WebSocketHandler* init_websocket_server() {

    WebSocketHandler* h_websocket = new WebSocketHandler();

    if (mg_init_library(MG_FEATURES_WEBSOCKET) == 0)
    {
        fprintf(stderr, "Websocket Server: Error initializing... \n");        
    }
    
    if (mg_check_feature(MG_FEATURES_WEBSOCKET) > 0)
        fprintf(stderr, "Websocket Server: Websocket enabled on localhost:%s\n", WS_PORT);
    else {
        fprintf(stderr, "Websocket Server: Websocket is not enabled... \n");        
    }
    
    const char *options[] = {
        "document_root", DOCUMENT_ROOT, "listening_ports", WS_PORT, 0};

    std::vector<std::string> cpp_options;
    for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++)
    {
        cpp_options.push_back(options[i]);
    }
    
    server = new CivetServer(cpp_options);

    server->addWebSocketHandler("/websocket", h_websocket);

    //h_websocket->process_param("{\"s\":[1,2,3,4,5]");
    
    return h_websocket;
}

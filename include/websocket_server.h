#pragma once

#include <mutex>
#include <unordered_set>
#include <vector>
#include <civet/CivetServer.h>

class WebSocketHandler : public CivetWebSocketHandler {
    public:
        WebSocketHandler();
        virtual ~WebSocketHandler();

        virtual bool handleConnection(CivetServer *server, const struct mg_connection *conn);
        virtual void handleReadyState(CivetServer *server, struct mg_connection *conn);
        virtual bool handleData(CivetServer *server, struct mg_connection *conn, int bits, char *data, size_t data_len);
        virtual void handleClose(CivetServer *server, const struct mg_connection *conn);

    public:
        void process_data(float *bin, int fftlen);
        void process_param(char *param);
        void send(char *data, int len, int MessageType);
        std::vector<std::string> get_update_params();

    private:
        std::mutex connectionsLock_;
        std::unordered_set<struct mg_connection *> connections_;
        std::vector<std::string> update_params;
};

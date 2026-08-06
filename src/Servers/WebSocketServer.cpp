#include "WebSocketServer.h"
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <assert.h>
#include <esp_log.h>

static const char* TAG = "WebSocketServer";

WebSocketServer::WebSocketServer(httpd_handle_t sharedServer)
    : server(sharedServer)
{
    buffer = xStreamBufferCreate(1024, 1);
    assert(buffer != NULL);
}

void WebSocketServer::setupRoutes() {
    static httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = WebSocketServer::wsHandler,
        .user_ctx = this,
        .is_websocket = true
    };

    httpd_register_uri_handler(server, &ws_uri);
}

esp_err_t WebSocketServer::wsHandler(httpd_req_t *req) {
    WebSocketServer* self = static_cast<WebSocketServer*>(req->user_ctx);

    const int newClientFd = httpd_req_to_sockfd(req);
    assert(newClientFd >= 0);

    if (self->clientFd != newClientFd) {
        if (self->clientFd >= 0) {
            ESP_LOGI(TAG, "Closing previous client: fd=%d", self->clientFd);
            self->closeClient();
        }
        self->clientFd = newClientFd;
        ESP_LOGI(TAG, "New webSocket client connected: fd=%d, addr=%p", newClientFd, (void*)self);
    }

    assert(self->clientFd >= 0);
    
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Successfully established connection with client on get request: fd=%d", self->clientFd);
        return ESP_OK;
    }
    
    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = nullptr;

    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    frame.payload = (uint8_t*)malloc(frame.len + 1);
    if (!frame.payload) return ESP_ERR_NO_MEM;

    ret = httpd_ws_recv_frame(req, &frame, frame.len);
    if (ret != ESP_OK) {
        free(frame.payload);
        self->closeClient();
        return ret;
    }
    frame.payload[frame.len] = '\0';
    
    // Push chars one by one into buffer
    xStreamBufferSend(self->buffer, frame.payload, frame.len, portMAX_DELAY);
    free(frame.payload);

    return ESP_OK;
}

char WebSocketServer::readCharBlocking() {
    char c = 0x00;
    while (true) {
        const size_t total_read = xStreamBufferReceive(buffer, &c, sizeof(c), portMAX_DELAY);
        if (total_read != 1) continue;
        break;
    }
    return c;
}

char WebSocketServer::readCharNonBlocking() {
    char c = 0x00;
    const size_t total_read = xStreamBufferReceive(buffer, &c, sizeof(c), 10/portTICK_PERIOD_MS);
    if (total_read != 1) return KEY_NONE;
    return c;
}

void WebSocketServer::sendTextAsync(const std::string& msg) {
    httpd_ws_frame_t ws_pkt = {};
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t*)msg.c_str();
    ws_pkt.len = msg.length();

    esp_err_t err = httpd_ws_send_frame_async(server, clientFd, &ws_pkt);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to send message: fd=%d, msg='%s', err=%s", clientFd, msg.c_str(), esp_err_to_name(err));
        closeClient();
    }
}

struct SendTextTaskParams {
    WebSocketServer* server;
    std::string msg;
    SendTextTaskParams(WebSocketServer* _server, const std::string& _msg)
    : server(_server), msg(_msg)
    {}
};

// enqueue for httpd server
void WebSocketServer::sendTextAsyncTask(void *_params) {
    SendTextTaskParams* params = static_cast<struct SendTextTaskParams*>(_params);
    assert(params != nullptr);
    params->server->sendTextAsync(params->msg);
    delete params;
}

void WebSocketServer::sendText(const std::string& msg) {
    if (clientFd < 0) {
        ESP_LOGW(TAG, "No client connected, cannot send message: addr=%p, msg='%s'", (void*)this, msg.c_str());
        return;
    }

    // Sanitize UTF8
    std::string safeMsg = sanitizeUtf8(msg);

    #if 0
    struct SendTextTaskParams* params = new struct SendTextTaskParams(this, safeMsg);
    assert(params != nullptr);
    const esp_err_t status = httpd_queue_work(server, WebSocketServer::sendTextAsyncTask, params);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue work for sending message: %s", esp_err_to_name(status));
        delete params;
    }
    #else
    sendTextAsync(safeMsg);
    #endif
}

std::string WebSocketServer::sanitizeUtf8(const std::string& input) {
    std::string output;
    size_t i = 0;

    while (i < input.size()) {
        unsigned char c = input[i];

        if (c <= 0x7F) {  // ASCII
            output += c;
            i++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < input.size() &&
                   (input[i+1] & 0xC0) == 0x80) {
            output += input.substr(i, 2);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < input.size() &&
                   (input[i+1] & 0xC0) == 0x80 &&
                   (input[i+2] & 0xC0) == 0x80) {
            output += input.substr(i, 3);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < input.size() &&
                   (input[i+1] & 0xC0) == 0x80 &&
                   (input[i+2] & 0xC0) == 0x80 &&
                   (input[i+3] & 0xC0) == 0x80) {
            output += input.substr(i, 4);
            i += 4;
        } else {
            // Invalid byte or sequence, skip it
            i++;
        }
    }

    return output;
}

void WebSocketServer::closeClient() {
    if (clientFd < 0) {
        ESP_LOGW(TAG, "Tried to close when there was no client connected");
        return;
    }
    httpd_sess_trigger_close(server, clientFd);
    ESP_LOGI(TAG, "Closed client: fd=%d", clientFd);
    clientFd = -1;
    // buffer.clear();
    xStreamBufferReset(buffer);
}

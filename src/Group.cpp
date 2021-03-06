#include "Group.h"

namespace uWS {

template <bool isServer>
void Group<isServer>::setUserData(void *user) {
    this->userData = user;
}

template <bool isServer>
void *Group<isServer>::getUserData() {
    return userData;
}

template <bool isServer>
void Group<isServer>::timerCallback(uv_timer_t *timer) {
    Group<isServer> *group = (Group<isServer> *) timer->data;

    group->forEach([](uWS::WebSocket<isServer> ws) {
        typename uWS::WebSocket<isServer>::Data *webSocketData = (typename uWS::WebSocket<isServer>::Data *) ws.getSocketData();
        if (webSocketData->hasOutstandingPong) {
            ws.terminate();
        } else {
            webSocketData->hasOutstandingPong = true;
        }
    });

    if (group->userPingMessage.length()) {
        group->broadcast(group->userPingMessage.data(), group->userPingMessage.length(), OpCode::TEXT);
    } else {
        group->broadcast(nullptr, 0, OpCode::PING);
    }
}

template <bool isServer>
void Group<isServer>::startAutoPing(int intervalMs, std::string userMessage) {
    timer = new uv_timer_t;
    uv_timer_init(loop, timer);
    timer->data = this;
    uv_timer_start(timer, timerCallback, intervalMs, intervalMs);
    userPingMessage = userMessage;
}

template <bool isServer>
void Group<isServer>::addWebSocket(uv_poll_t *webSocket) {
    if (webSocketHead) {
        uS::SocketData *nextData = (uS::SocketData *) webSocketHead->data;
        nextData->prev = webSocket;
        uS::SocketData *data = (uS::SocketData *) webSocket->data;
        data->next = webSocketHead;
    }
    webSocketHead = webSocket;
}

template <bool isServer>
void Group<isServer>::removeWebSocket(uv_poll_t *webSocket) {
    uS::SocketData *socketData = (uS::SocketData *) webSocket->data;
    if (iterators.size()) {
        iterators.top() = socketData->next;
    }
    if (socketData->prev == socketData->next) {
        webSocketHead = (uv_poll_t *) nullptr;
    } else {
        if (socketData->prev) {
            ((uS::SocketData *) socketData->prev->data)->next = socketData->next;
        } else {
            webSocketHead = socketData->next;
        }
        if (socketData->next) {
            ((uS::SocketData *) socketData->next->data)->prev = socketData->prev;
        }
    }
}

template <bool isServer>
Group<isServer>::Group(int extensionOptions, Hub *hub, uS::NodeData *nodeData) : uS::NodeData(*nodeData), hub(hub), extensionOptions(extensionOptions) {
    connectionHandler = [](WebSocket<isServer>, UpgradeInfo) {};
    messageHandler = [](WebSocket<isServer>, char *, size_t, OpCode) {};
    disconnectionHandler = [](WebSocket<isServer>, int, char *, size_t) {};
    pingHandler = pongHandler = [](WebSocket<isServer>, char *, size_t) {};
    errorHandler = [](errorType) {};

    this->extensionOptions |= CLIENT_NO_CONTEXT_TAKEOVER | SERVER_NO_CONTEXT_TAKEOVER;
}

template <bool isServer>
void Group<isServer>::stopListening() {
    if (isServer) {
        uS::ListenData *listenData = (uS::ListenData *) user;
        if (listenData) {
            uS::Socket(listenData->listenPoll).close();
            delete listenData;
        }
    }

    if (async) {
        uv_close((uv_handle_t *) async, [](uv_handle_t *h) {
            delete (uv_async_t *) h;
        });
    }
}

template <bool isServer>
void Group<isServer>::onConnection(std::function<void (WebSocket<isServer>, UpgradeInfo)> handler) {
    connectionHandler = handler;
}

template <bool isServer>
void Group<isServer>::onMessage(std::function<void (WebSocket<isServer>, char *, size_t, OpCode)> handler) {
    messageHandler = handler;
}

template <bool isServer>
void Group<isServer>::onDisconnection(std::function<void (WebSocket<isServer>, int, char *, size_t)> handler) {
    disconnectionHandler = handler;
}

template <bool isServer>
void Group<isServer>::onPing(std::function<void (WebSocket<isServer>, char *, size_t)> handler) {
    pingHandler = handler;
}

template <bool isServer>
void Group<isServer>::onPong(std::function<void (WebSocket<isServer>, char *, size_t)> handler) {
    pongHandler = handler;
}

template <bool isServer>
void Group<isServer>::onError(std::function<void (typename Group::errorType)> handler) {
    errorHandler = handler;
}

template <bool isServer>
void Group<isServer>::broadcast(const char *message, size_t length, OpCode opCode) {
    typename WebSocket<isServer>::PreparedMessage *preparedMessage = WebSocket<isServer>::prepareMessage((char *) message, length, opCode, false);
    forEach([preparedMessage](uWS::WebSocket<isServer> ws) {
        ws.sendPrepared(preparedMessage);
    });
    WebSocket<isServer>::finalizeMessage(preparedMessage);
}

template <bool isServer>
void Group<isServer>::terminate() {
    forEach([](uWS::WebSocket<isServer> ws) {
        ws.terminate();
    });
    stopListening();
}

template <bool isServer>
void Group<isServer>::close(int code, char *message, size_t length) {
    forEach([code, message, length](uWS::WebSocket<isServer> ws) {
        ws.close(code, message, length);
    });
    stopListening();
    if (timer) {
        uv_timer_stop(timer);
        uv_close((uv_handle_t *) timer, [](uv_handle_t *handle) {
            delete (uv_timer_t *) handle;
        });
    }
}

template struct Group<true>;
template struct Group<false>;

}

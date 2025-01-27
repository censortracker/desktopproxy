#pragma once

#include <QObject>
#include <QHttpServer>
#include <QTcpServer>
#include <QWeakPointer>
#include "iproxyservice.h"

class HttpApi : public QObject {
    Q_OBJECT

public:
    explicit HttpApi(QWeakPointer<IProxyService> service, QObject* parent = nullptr);
    ~HttpApi();

    bool start(quint16 port);
    void stop();

private:
    void setupRoutes();
    
    // Route handlers
    QJsonObject handleGetConfig() const;
    QJsonObject handlePostConfig(const QHttpServerRequest& request);
    QJsonObject handlePostUp();
    QJsonObject handlePostDown();
    QJsonObject handleGetPing() const;

    QHttpServer m_server;
    QScopedPointer<QTcpServer> m_tcpServer;
    QWeakPointer<IProxyService> m_service;
}; 
#pragma once

#include <QObject>
#include <QHttpServer>
#include <QProcess>
#include <QScopedPointer>
#include <QJsonObject>
#include <QTcpServer>

class ProxyServer : public QObject
{
    Q_OBJECT

public:
    explicit ProxyServer(QObject *parent = nullptr);
    ~ProxyServer();

    bool start(quint16 port = 8080);
    void stop();

private:
    void setupRoutes();
    bool startXrayProcess();
    void stopXrayProcess();
    QString getConfigPath() const;
    QJsonObject readConfig() const;
    bool writeConfig(const QJsonObject &config);
    
    // Platform-specific methods
    QString getXrayExecutablePath() const;
    QStringList getXrayArguments(const QString &configPath) const;
    QString getPlatformName() const;

    QHttpServer m_server;
    QScopedPointer<QProcess> m_xrayProcess;
    QScopedPointer<QTcpServer> m_tcpServer;
};
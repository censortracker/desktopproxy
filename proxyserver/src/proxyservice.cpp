#include "proxyservice.h"

ProxyService::ProxyService(QObject* parent)
    : QObject(parent)
    , m_configManager(new ConfigManager())
    , m_xrayController(new XrayController())
{
}

QJsonObject ProxyService::getConfig() const
{
    return m_configManager->readConfig();
}

bool ProxyService::updateConfig(const QString& configStr)
{
    return m_configManager->updateConfigFromString(configStr);
}

bool ProxyService::startXray()
{
    return m_xrayController->start(m_configManager->getConfigPath());
}

bool ProxyService::stopXray()
{
    m_xrayController->stop();
    return true;
}

bool ProxyService::isXrayRunning() const
{
    return m_xrayController->isXrayRunning();
}

qint64 ProxyService::getXrayProcessId() const
{
    return m_xrayController->getProcessId();
}

QString ProxyService::getXrayError() const
{
    return m_xrayController->getError();
} 
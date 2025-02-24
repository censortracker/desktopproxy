#include "xraycontroller.h"
#include "logger.h"
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDebug>

XrayController::XrayController(QObject *parent)
    : QObject(parent), m_process(nullptr)
{
    Logger::getInstance().debug("XrayController initialized");
}

XrayController::~XrayController()
{
    stop();
}

bool XrayController::start(const QString &configPath)
{
    if (isXrayRunning())
    {
        Logger::getInstance().info("Xray process is already running");
        return true;
    }

    QString xrayPath = getXrayExecutablePath();
    Logger::getInstance().debug(QString("Xray executable path: %1").arg(xrayPath));

    if (!QFile::exists(xrayPath))
    {
        Logger::getInstance().error(QString("Xray binary not found at: %1").arg(xrayPath));
        return false;
    }

    if (!QFile::exists(configPath))
    {
        Logger::getInstance().error(QString("Config file not found at: %1").arg(configPath));
        return false;
    }

    Logger::getInstance().info("Starting Xray process");
    m_process.reset(new QProcess(this));
    m_process->setWorkingDirectory(QFileInfo(xrayPath).dir().absolutePath());
    m_process->setProgram(xrayPath);
    m_process->setArguments(getXrayArguments(configPath));

    m_process->start();
    if (!m_process->waitForStarted())
    {
        Logger::getInstance().error(QString("Failed to start Xray process: %1").arg(m_process->errorString()));
        m_process.reset();
        return false;
    }

    Logger::getInstance().info(QString("Xray process started successfully (PID: %1)").arg(m_process->processId()));
    return true;
}

void XrayController::stop()
{
    if (!m_process.isNull())
    {
        if (m_process->state() == QProcess::Running)
        {
            Logger::getInstance().info(QString("Killing Xray process (PID: %1)").arg(m_process->processId()));
            m_process->kill();
            m_process->waitForFinished(3000);
        }
        m_process.reset();
        Logger::getInstance().info("Xray process stopped");
    }
}

bool XrayController::isXrayRunning() const
{
    return !m_process.isNull() && m_process->state() == QProcess::Running;
}

qint64 XrayController::getProcessId() const
{
    return m_process ? m_process->processId() : -1;
}

QString XrayController::getError() const
{
    if (m_process && m_process->error() != QProcess::UnknownError)
    {
        QString error = m_process->errorString();
        Logger::getInstance().error(QString("Xray process error: %1").arg(error));
        return error;
    }
    return QString();
}

QString XrayController::getXrayExecutablePath() const
{
    QString xrayDir = QCoreApplication::applicationDirPath();
    QString xrayPath;

#if defined(Q_OS_WIN)
    xrayPath = QDir(xrayDir).filePath("xray.exe");
#else
    xrayPath = QDir(xrayDir).filePath("xray");
#endif

    Logger::getInstance().debug(QString("Resolved Xray executable path: %1").arg(xrayPath));
    return xrayPath;
}

QStringList XrayController::getXrayArguments(const QString &configPath) const
{
    QStringList args = QStringList() << "-c" << configPath << "-format=json";
    Logger::getInstance().debug(QString("Xray arguments: %1").arg(args.join(' ')));
    return args;
}
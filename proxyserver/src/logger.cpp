#include "logger.h"
#include <QDir>
#include <QTextStream>

Logger::Logger(QObject* parent) : QObject(parent), m_maxFileSize(0), m_currentLevel(LogLevel::INFO)
{
}

Logger::~Logger()
{
}

Logger& Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void Logger::init(const QString& logPath, qint64 maxFileSize)
{
    QMutexLocker locker(&m_mutex);
    m_logPath = logPath;
    m_maxFileSize = maxFileSize;
    
    // Create logs directory if it doesn't exist
    QDir dir = QFileInfo(m_logPath).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

void Logger::setLogLevel(LogLevel level)
{
    m_currentLevel = level;
}

void Logger::log(LogLevel level, const QString& message)
{
    logInternal(level, message);
}

void Logger::debug(const QString& message)
{
    logInternal(LogLevel::DEBUG, message);
}

void Logger::info(const QString& message)
{
    logInternal(LogLevel::INFO, message);
}

void Logger::warning(const QString& message)
{
    logInternal(LogLevel::WARNING, message);
}

void Logger::error(const QString& message)
{
    logInternal(LogLevel::ERROR, message);
}

void Logger::logInternal(LogLevel level, const QString& message)
{
    if (level < m_currentLevel) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    
    checkRotation();

    QFile file(m_logPath);
    if (!openLogFile(file)) {
        return;
    }

    QTextStream stream(&file);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    stream << QString("[%1] [%2] %3\n").arg(timestamp, levelToString(level), message);
    stream.flush();
    file.close();
}

QString Logger::levelToString(LogLevel level)
{
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default:               return "UNKNOWN";
    }
}

qint64 Logger::getCurrentFileSize() const
{
    QFile file(m_logPath);
    if (file.exists()) {
        return file.size();
    }
    return -1;
}

void Logger::checkRotation()
{
    if (m_maxFileSize > 0 && getCurrentFileSize() >= m_maxFileSize) {
        // Delete the oldest file
        QFile::remove(QString("%1.%2").arg(m_logPath).arg(MAX_BACKUP_FILES));

        // Shift existing files
        for (int i = MAX_BACKUP_FILES - 1; i >= 1; --i) {
            QString oldName = QString("%1.%2").arg(m_logPath).arg(i);
            QString newName = QString("%1.%2").arg(m_logPath).arg(i + 1);
            QFile::rename(oldName, newName);
        }

        // Rename current file
        QFile::rename(m_logPath, m_logPath + ".1");
    }
}

bool Logger::openLogFile(QFile& file)
{
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Failed to open log file:" << m_logPath;
        return false;
    }
    return true;
} 
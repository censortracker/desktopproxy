#include "httpapi.h"
#include "logger.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QHostAddress>
#include <QDebug>

HttpApi::HttpApi(QWeakPointer<IProxyService> service, QObject* parent)
    : QObject(parent)
    , m_tcpServer(new QTcpServer(this))
    , m_service(service)
{
    Logger::getInstance().debug("HttpApi initialized");
}

HttpApi::~HttpApi()
{
    stop();
}

bool HttpApi::start(quint16 port)
{
    Logger::getInstance().info(QString("Starting HTTP API server on port %1").arg(port));
    
    if (!m_tcpServer->listen(QHostAddress::LocalHost, port)) {
        Logger::getInstance().error(QString("Failed to start HTTP API server on port %1").arg(port));
        return false;
    }

    setupRoutes();
    m_server.bind(m_tcpServer.data());
    
    Logger::getInstance().info(QString("HTTP API server is running on localhost:%1").arg(m_tcpServer->serverPort()));
    Logger::getInstance().debug("Available endpoints:\n"
                              "  GET    /api/v1/configs\n"
                              "  POST   /api/v1/configs\n"
                              "  PUT    /api/v1/configs\n"
                              "  DELETE /api/v1/configs\n"
                              "  PUT    /api/v1/configs/activate\n"
                              "  GET    /api/v1/configs/active\n"
                              "  POST   /api/v1/up\n"
                              "  POST   /api/v1/down\n"
                              "  GET    /api/v1/ping");
    return true;
}

void HttpApi::stop()
{
    Logger::getInstance().info("Stopping HTTP API server");
    if (m_tcpServer) {
        m_tcpServer->close();
    }
}

void HttpApi::setupRoutes()
{
    Logger::getInstance().debug("Setting up HTTP API routes");

    // Config management routes
    m_server.route("/api/v1/configs", QHttpServerRequest::Method::Get, 
        [this](const QHttpServerRequest &request) { 
            Logger::getInstance().debug("Handling GET /api/v1/configs request");
            return handleGetConfigs(request); 
        });
    
    m_server.route("/api/v1/configs", QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest &request) { 
            Logger::getInstance().debug("Handling POST /api/v1/configs request");
            return handleAddConfigs(request); 
        });
    
    m_server.route("/api/v1/configs", QHttpServerRequest::Method::Put,
        [this](const QHttpServerRequest &request) { 
            Logger::getInstance().debug("Handling PUT /api/v1/configs request");
            return handleUpdateConfigs(request); 
        });
    
    m_server.route("/api/v1/configs", QHttpServerRequest::Method::Delete,
        [this](const QHttpServerRequest &request) { 
            Logger::getInstance().debug("Handling DELETE /api/v1/configs request");
            return handleDeleteConfig(request); 
        });
    
    m_server.route("/api/v1/configs/activate", QHttpServerRequest::Method::Put,
        [this](const QHttpServerRequest &request) { 
            Logger::getInstance().debug("Handling PUT /api/v1/configs/activate request");
            return handleActivateConfig(request); 
        });
    
    m_server.route("/api/v1/configs/active", QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest &request) { 
            Logger::getInstance().debug("Handling GET /api/v1/configs/active request");
            return handleGetActiveConfig(request); 
        });

    // Xray control routes
    m_server.route("/api/v1/up", QHttpServerRequest::Method::Post,
        [this] { 
            Logger::getInstance().debug("Handling POST /api/v1/up request");
            return handlePostUp(); 
        });

    m_server.route("/api/v1/down", QHttpServerRequest::Method::Post,
        [this] { 
            Logger::getInstance().debug("Handling POST /api/v1/down request");
            return handlePostDown(); 
        });

    m_server.route("/api/v1/ping", QHttpServerRequest::Method::Get,
        [this] { 
            Logger::getInstance().debug("Handling GET /api/v1/ping request");
            return handleGetPing(); 
        });
}

QJsonObject HttpApi::handlePostUp()
{
    QJsonObject response;
    
    if (auto service = m_service.lock()) {
        if (service->startXray()) {
            Logger::getInstance().info("Xray process started successfully");
            response["status"] = "success";
            response["message"] = "Xray process started successfully";
            
            // Try to get port from inbounds configuration
            QJsonObject config = service->getConfig();
            if (config.contains("inbounds") && config["inbounds"].isArray()) {
                QJsonArray inbounds = config["inbounds"].toArray();
                if (!inbounds.isEmpty() && inbounds[0].isObject()) {
                    QJsonObject firstInbound = inbounds[0].toObject();
                    if (firstInbound.contains("port")) {
                        int port = firstInbound["port"].toInt();
                        Logger::getInstance().info(QString("Xray listening on port %1").arg(port));
                        response["xray_port"] = port;
                    }
                }
            }
        } else {
            Logger::getInstance().error("Failed to start Xray process");
            response["status"] = "error";
            response["message"] = "Failed to start xray process";
        }
    } else {
        Logger::getInstance().error("Service unavailable while trying to start Xray");
        response["status"] = "error";
        response["message"] = "Service unavailable";
    }
    return response;
}

QJsonObject HttpApi::handlePostDown()
{
    if (auto service = m_service.lock()) {
        service->stopXray();
        QJsonObject response;
        response["status"] = "success";
        response["message"] = "Xray process stopped";
        return response;
    }
    return { {"status", "error"}, {"message", "Service unavailable"} };
}

QJsonObject HttpApi::handleGetPing() const
{
    QJsonObject response;
    response["status"] = "success";
    response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    if (auto service = m_service.lock()) {
        bool isRunning = service->isXrayRunning();
        response["xray_running"] = isRunning;
        
        if (isRunning) {
            qint64 pid = service->getXrayProcessId();
            Logger::getInstance().debug(QString("Xray is running (PID: %1)").arg(pid));
            response["xray_pid"] = pid;
            response["xray_state"] = "running";
            
            QJsonObject config = service->getConfig();
            if (config.contains("inbounds") && config["inbounds"].isArray()) {
                QJsonArray inbounds = config["inbounds"].toArray();
                if (!inbounds.isEmpty() && inbounds[0].isObject()) {
                    QJsonObject firstInbound = inbounds[0].toObject();
                    if (firstInbound.contains("port")) {
                        int port = firstInbound["port"].toInt();
                        Logger::getInstance().debug(QString("Xray port: %1").arg(port));
                        response["xray_port"] = port;
                    }
                }
            }
            
            QString error = service->getXrayError();
            if (!error.isEmpty()) {
                Logger::getInstance().warning(QString("Xray error: %1").arg(error));
                response["xray_error"] = error;
            }
        } else {
            Logger::getInstance().debug("Xray is not running");
            response["xray_state"] = "stopped";
        }
    } else {
        Logger::getInstance().error("Service unavailable while processing GET /api/v1/ping request");
        response["status"] = "error";
        response["message"] = "Service unavailable";
    }
    
    return response;
}

QHttpServerResponse HttpApi::handleGetConfigs(const QHttpServerRequest &request)
{
    if (auto service = m_service.lock()) {
        // Get UUIDs from query parameters if present
        QString uuidList = request.query().queryItemValue("uuid");
        Logger::getInstance().debug(QString("UUID filter: %1").arg(uuidList.isEmpty() ? "none" : uuidList));
        
        QMap<QString, QJsonObject> configs;

        if (uuidList.isEmpty()) {
            Logger::getInstance().debug("Retrieving all configs");
            configs = service->getAllConfigs();
        } else {
            QStringList uuids = uuidList.split(',', Qt::SkipEmptyParts);
            Logger::getInstance().debug(QString("Retrieving configs for UUIDs: %1").arg(uuids.join(", ")));
            configs = service->getConfigsByUuids(uuids);
        }

        QJsonObject response;
        response["status"] = "success";
        
        // Convert QMap to QJsonObject manually
        QJsonObject configsJson;
        for (auto it = configs.constBegin(); it != configs.constEnd(); ++it) {
            configsJson[it.key()] = it.value();
        }
        response["configs"] = configsJson;
        
        Logger::getInstance().info(QString("Successfully retrieved %1 configs").arg(configs.size()));
        return QHttpServerResponse(response);
    }

    Logger::getInstance().error("Service unavailable while processing GET /api/v1/configs request");
    return QHttpServerResponse(
        QJsonObject{{"status", "error"}, {"message", "Service unavailable"}},
        QHttpServerResponse::StatusCode::ServiceUnavailable
    );
}

QHttpServerResponse HttpApi::handleAddConfigs(const QHttpServerRequest &request)
{
    Logger::getInstance().info("Processing POST /api/v1/configs request");
    
    if (auto service = m_service.lock()) {
        // Parse request body
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(request.body(), &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            Logger::getInstance().error(QString("Invalid JSON format: %1").arg(parseError.errorString()));
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "Invalid JSON format"}
                },
                QHttpServerResponse::StatusCode::BadRequest
            );
        }

        if (!doc.isObject()) {
            Logger::getInstance().error("Request body is not a JSON object");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "Request body must be a JSON object"}
                },
                QHttpServerResponse::StatusCode::BadRequest
            );
        }

        QJsonObject root = doc.object();
        if (!root.contains("configs") || !root["configs"].isArray()) {
            Logger::getInstance().error("Request body missing 'configs' array");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "Request body must contain 'configs' array"}
                },
                QHttpServerResponse::StatusCode::BadRequest
            );
        }

        // Convert JSON array to string list
        QStringList configs;
        QJsonArray configsArray = root["configs"].toArray();
        Logger::getInstance().debug(QString("Processing %1 configs from request").arg(configsArray.size()));
        
        for (const auto &value : configsArray) {
            if (!value.isString()) {
                Logger::getInstance().error("Invalid config format: config must be a string");
                return QHttpServerResponse(
                    QJsonObject{
                        {"status", "error"},
                        {"message", "All configs must be strings"}
                    },
                    QHttpServerResponse::StatusCode::BadRequest
                );
            }
            configs.append(value.toString());
        }

        if (configs.isEmpty()) {
            Logger::getInstance().error("Empty configs array in request");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "Configs array cannot be empty"}
                },
                QHttpServerResponse::StatusCode::BadRequest
            );
        }

        // Add configs
        Logger::getInstance().info(QString("Adding %1 configs").arg(configs.size()));
        if (service->addConfigs(configs)) {
            Logger::getInstance().info("Successfully added configs");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "success"},
                    {"message", "Configs added successfully"}
                }
            );
        } else {
            Logger::getInstance().error("Failed to add configs");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "Failed to add configs"}
                },
                QHttpServerResponse::StatusCode::InternalServerError
            );
        }
    }

    Logger::getInstance().error("Service unavailable while processing POST /api/v1/configs request");
    return QHttpServerResponse(
        QJsonObject{{"status", "error"}, {"message", "Service unavailable"}},
        QHttpServerResponse::StatusCode::ServiceUnavailable
    );
}

QHttpServerResponse HttpApi::handleUpdateConfigs(const QHttpServerRequest &request)
{
    if (auto service = m_service.lock()) {
        // Parse request body
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(request.body(), &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            Logger::getInstance().error(QString("Invalid JSON format: %1").arg(parseError.errorString()));
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "Invalid JSON format"}
                },
                QHttpServerResponse::StatusCode::BadRequest
            );
        }

        if (!doc.isObject()) {
            Logger::getInstance().error("Request body is not a JSON object");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "Request body must be a JSON object"}
                },
                QHttpServerResponse::StatusCode::BadRequest
            );
        }

        QJsonObject root = doc.object();
        if (!root.contains("configs") || !root["configs"].isArray()) {
            Logger::getInstance().error("Request body missing 'configs' array");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "Request body must contain 'configs' array"}
                },
                QHttpServerResponse::StatusCode::BadRequest
            );
        }

        // Convert JSON array to string list
        QStringList configs;
        QJsonArray configsArray = root["configs"].toArray();
        Logger::getInstance().debug(QString("Processing %1 configs from request").arg(configsArray.size()));
        
        for (const auto &value : configsArray) {
            if (!value.isString()) {
                Logger::getInstance().error("Invalid config format: config must be a string");
                return QHttpServerResponse(
                    QJsonObject{
                        {"status", "error"},
                        {"message", "All configs must be strings"}
                    },
                    QHttpServerResponse::StatusCode::BadRequest
                );
            }
            configs.append(value.toString());
        }

        if (configs.isEmpty()) {
            Logger::getInstance().error("Empty configs array in request");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "Configs array cannot be empty"}
                },
                QHttpServerResponse::StatusCode::BadRequest
            );
        }

        // Update configs
        Logger::getInstance().info(QString("Updating all configs with %1 new config(s)").arg(configs.size()));
        if (service->updateAllConfigs(configs)) {
            Logger::getInstance().info("Successfully updated all configs");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "success"},
                    {"message", "Configs updated successfully"}
                }
            );
        } else {
            Logger::getInstance().error("Failed to update configs");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "Failed to update configs"}
                },
                QHttpServerResponse::StatusCode::InternalServerError
            );
        }
    }

    Logger::getInstance().error("Service unavailable while processing PUT /api/v1/configs request");
    return QHttpServerResponse(
        QJsonObject{{"status", "error"}, {"message", "Service unavailable"}},
        QHttpServerResponse::StatusCode::ServiceUnavailable
    );
}

QHttpServerResponse HttpApi::handleDeleteConfig(const QHttpServerRequest &request)
{
    if (auto service = m_service.lock()) {
        // Get UUID from query parameters
        QString uuid = request.query().queryItemValue("uuid");
        
        if (uuid.isEmpty()) {
            Logger::getInstance().error("Missing UUID parameter in request");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "UUID parameter is required"}
                },
                QHttpServerResponse::StatusCode::BadRequest
            );
        }

        Logger::getInstance().info(QString("Attempting to delete config with UUID: %1").arg(uuid));
        // Delete config
        if (service->removeConfig(uuid)) {
            Logger::getInstance().info(QString("Successfully deleted config with UUID: %1").arg(uuid));
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "success"},
                    {"message", "Config deleted successfully"}
                }
            );
        } else {
            Logger::getInstance().error(QString("Failed to delete config with UUID: %1").arg(uuid));
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "Failed to delete config"}
                },
                QHttpServerResponse::StatusCode::InternalServerError
            );
        }
    }

    Logger::getInstance().error("Service unavailable while processing DELETE /api/v1/configs request");
    return QHttpServerResponse(
        QJsonObject{{"status", "error"}, {"message", "Service unavailable"}},
        QHttpServerResponse::StatusCode::ServiceUnavailable
    );
}

QHttpServerResponse HttpApi::handleActivateConfig(const QHttpServerRequest &request)
{
    if (auto service = m_service.lock()) {
        // Get UUID from query parameters
        QString uuid = request.query().queryItemValue("uuid");
        
        if (uuid.isEmpty()) {
            Logger::getInstance().error("Missing UUID parameter in request");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "UUID parameter is required"}
                },
                QHttpServerResponse::StatusCode::BadRequest
            );
        }

        Logger::getInstance().info(QString("Attempting to activate config with UUID: %1").arg(uuid));
        // Activate config
        if (service->activateConfig(uuid)) {
            Logger::getInstance().info(QString("Successfully activated config with UUID: %1").arg(uuid));
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "success"},
                    {"message", "Config activated successfully"}
                }
            );
        } else {
            Logger::getInstance().error(QString("Failed to activate config with UUID: %1").arg(uuid));
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "Failed to activate config"}
                },
                QHttpServerResponse::StatusCode::InternalServerError
            );
        }
    }

    Logger::getInstance().error("Service unavailable while processing PUT /api/v1/configs/activate request");
    return QHttpServerResponse(
        QJsonObject{{"status", "error"}, {"message", "Service unavailable"}},
        QHttpServerResponse::StatusCode::ServiceUnavailable
    );
}

QHttpServerResponse HttpApi::handleGetActiveConfig(const QHttpServerRequest &request)
{
    if (auto service = m_service.lock()) {
        QJsonObject activeConfig = service->getActiveConfig();
        
        if (!activeConfig.isEmpty()) {
            Logger::getInstance().info("Successfully retrieved active config");
            QJsonObject response;
            response["status"] = "success";
            response["config"] = activeConfig;
            return QHttpServerResponse(response);
        } else {
            Logger::getInstance().warning("No active config found");
            return QHttpServerResponse(
                QJsonObject{
                    {"status", "error"},
                    {"message", "No active config found"}
                },
                QHttpServerResponse::StatusCode::NotFound
            );
        }
    }

    Logger::getInstance().error("Service unavailable while processing GET /api/v1/configs/active request");
    return QHttpServerResponse(
        QJsonObject{{"status", "error"}, {"message", "Service unavailable"}},
        QHttpServerResponse::StatusCode::ServiceUnavailable
    );
} 
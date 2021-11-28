#pragma once

#include <QObject>
#include <QString>
#include <QJsonDocument>
#include <QWebSocketServer>

// hyperion / utils
#include <utils/Logger.h>

// settings
#include <utils/settings.h>

/*
OPENSSL command that generated the embedded key and cert file

openssl req -x509 -newkey rsa:4096 -sha256 -days 3650 -nodes \
  -keyout hyperion.key -out hyperion.crt -extensions san -config \
  <(echo "[req]";
    echo distinguished_name=req;
    echo "[san]";
    echo subjectAltName=DNS:hyperion-project.org,IP:127.0.0.1
    ) \
  -subj /CN=hyperion-project.org
*/

class WsServer;

class WebSocketServer : public QObject
{
	Q_OBJECT

public:
	WebSocketServer (const QJsonDocument& config, bool useSsl, QObject * parent = nullptr);

	~WebSocketServer () override;

	void start();
	void stop();

signals:
	///
	/// @emits whenever server is started or stopped (to sync with SSDPHandler)
	/// @param newState   True when started, false when stopped
	///
	void stateChange(bool newState);

	///
	/// @brief Emits whenever the port changes (doesn't compare prev <> now)
	///
	void portChanged(quint16 port);

public slots:
	///
	/// @brief Init server after thread start
	///
	void initServer();

	void onServerStopped      ();
	void onServerStarted      (quint16 port);
	void onServerError        (QString msg);

	///
	/// @brief Handle settings update from Hyperion Settingsmanager emit or this constructor
	/// @param type   settingyType from enum
	/// @param config configuration object
	///
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

	/// check if server has been inited
	bool isInited() const { return _inited; }

	quint16 getPort() const { return _port; }

private:
	QJsonDocument        _config;
	bool				 _useSsl;
	Logger*              _log;
	QString              _baseUrl;
	quint16              _port;
	WsServer*			 _server;
	bool                 _inited = false;

	const QString        WebSocketServer_DEFAULT_CRT_PATH = ":/hyperion.crt";
	const QString        WebSocketServer_DEFAULT_KEY_PATH = ":/hyperion.key";
	quint16              WebSocketServer_DEFAULT_PORT     = 8192;

};

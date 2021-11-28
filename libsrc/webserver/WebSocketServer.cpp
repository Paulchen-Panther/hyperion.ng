#include "webserver/WebSocketServer.h"
#include "WsServer.h"
#include "HyperionConfig.h"

#include <QWebSocketServer>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>

WebSocketServer::WebSocketServer(const QJsonDocument& config, bool useSsl, QObject * parent)
	:  QObject(parent)
	, _config(config)
	, _useSsl(useSsl)
	, _log(Logger::getInstance("WEBSOCKET"))
    , _server(nullptr)
{

}

WebSocketServer::~WebSocketServer()
{
	stop();
}

void WebSocketServer::initServer()
{
	Debug(_log, "Initialize WebSocket-Server");
	_server = new WsServer(this);
	_server->setServerName(QStringLiteral("Hyperion WebSocket Server"));

	if(_useSsl)
		_server->setUseSecure();

	connect (_server, &WsServer::started, this, &WebSocketServer::onServerStarted);
	connect (_server, &WsServer::stopped, this, &WebSocketServer::onServerStopped);
	connect (_server, &WsServer::error,   this, &WebSocketServer::onServerError);

	// init
	handleSettingsUpdate(settings::WEBSOCKET, _config);
}

void WebSocketServer::onServerStarted (quint16 port)
{
	_inited = true;
	Info(_log, "Started on port %d name '%s'", port ,_server->getServerName().toStdString().c_str());
	emit stateChange(true);
}

void WebSocketServer::onServerStopped ()
{
	Info(_log, "Stopped %s", _server->getServerName().toStdString().c_str());
	emit stateChange(false);
}

void WebSocketServer::onServerError (QString msg)
{
	Error(_log, "%s", msg.toStdString().c_str());
}

void WebSocketServer::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if(type == settings::WEBSOCKET)
	{
		Debug(_log, "Apply WebSocketServer settings");
		const QJsonObject& obj = config.object();

		// ssl different port
		quint16 newPort = _useSsl ? obj["sslPort"].toInt(WebSocketServer_DEFAULT_PORT) : obj["port"].toInt(WebSocketServer_DEFAULT_PORT);
		if(_port != newPort)
		{
			_port = newPort;
			stop();
		}

//		// eval if the port is available, will be incremented if not
//		if(!_server->isListening())
//			NetUtils::portAvailable(_port, _log);

		// on ssl we want .key .cert and probably key password
		if(_useSsl)
		{
		    QString keyPath = obj["keyPath"].toString(WebSocketServer_DEFAULT_KEY_PATH);
			QString crtPath = obj["crtPath"].toString(WebSocketServer_DEFAULT_CRT_PATH);

			// check keyPath
			if ( (keyPath != WebSocketServer_DEFAULT_KEY_PATH) && !keyPath.trimmed().isEmpty())
			{
				QFileInfo kinfo(keyPath);
				if (!kinfo.exists())
				{
					Error(_log, "No SSL key found at '%s' falling back to internal", keyPath.toUtf8().constData());
					keyPath = WebSocketServer_DEFAULT_KEY_PATH;
				}
			}
			else
			    keyPath = WebSocketServer_DEFAULT_KEY_PATH;

			// check crtPath
			if ( (crtPath != WebSocketServer_DEFAULT_CRT_PATH) && !crtPath.trimmed().isEmpty())
			{
				QFileInfo cinfo(crtPath);
				if (!cinfo.exists())
				{
					Error(_log, "No SSL certificate found at '%s' falling back to internal", crtPath.toUtf8().constData());
					crtPath = WebSocketServer_DEFAULT_CRT_PATH;
				}
			}
			else
			    crtPath = WebSocketServer_DEFAULT_CRT_PATH;

			// load and verify crt
			QFile cfile(crtPath);
			cfile.open(QIODevice::ReadOnly);
			QList<QSslCertificate> validList;
			QList<QSslCertificate> cList =  QSslCertificate::fromDevice(&cfile, QSsl::Pem);
			cfile.close();

			// Filter for valid certs
			for(const auto & entry : cList)
			{
				if(!entry.isNull() && QDateTime::currentDateTime().daysTo(entry.expiryDate()) > 0)
					validList.append(entry);
				else
					Error(_log, "The provided SSL certificate is invalid/not supported/reached expiry date ('%s')", crtPath.toUtf8().constData());
			}

			if(!validList.isEmpty())
			{
				Debug(_log,"Setup SSL certificate");
				_server->setCertificates(validList);
			} else {
				Error(_log, "No valid SSL certificate has been found ('%s')", crtPath.toUtf8().constData());
			}

			// load and verify key
			QFile kfile(keyPath);
			kfile.open(QIODevice::ReadOnly);
			// The key should be RSA enrcrypted and PEM format, optional the passPhrase
			QSslKey key(&kfile, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey, obj["keyPassPhrase"].toString().toUtf8());
			kfile.close();

			if(key.isNull()){
				Error(_log, "The provided SSL key is invalid or not supported use RSA encrypt and PEM format ('%s')", keyPath.toUtf8().constData());
			} else {
				Debug(_log,"Setup private SSL key");
				_server->setPrivateKey(key);
			}
		}

		start();
		emit portChanged(_port);
	}
}

void WebSocketServer::start()
{
	qDebug() << "Port: " << _port;
	_server->start(_port);
}

void WebSocketServer::stop()
{
	_server->stop();
}

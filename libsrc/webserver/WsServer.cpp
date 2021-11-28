
#include "WsServer.h"
#include <QWebSocket>
//#include <QUrlQuery>

#include <api/JsonAPI.h>
//#include <utils/NetOrigin.h>
#include <utils/Logger.h>

static QString getIdentifier(QWebSocket *peer)
{
	return QStringLiteral("%1:%2").arg(peer->peerAddress().toString(), QString::number(peer->peerPort()));
}

WsServer::WsServer(QObject * parent)
	: QObject(parent)
	, _useSsl(false)
	, _serverName(QStringLiteral("Hyperion WebSocket Server"))
{
	_sockServer = new QWebSocketServer(_serverName, QWebSocketServer::SslMode::NonSecureMode, this);
	connect(_sockServer, &QWebSocketServer::newConnection, this, &WsServer::onClientConnected);
	// connect(_sockServer, &QWebSocketServer::closed, this, &WebSocketClient::onWsDisconnect);
}

void WsServer::start(quint16 port)
{
	Debug(Logger::getInstance("WsServer"),"");
	if(!_sockServer->isListening())
	{
		if(_sockServer->listen(QHostAddress::Any, port))
		{
			qDebug() << "start successful port: " << _sockServer->serverPort ();
			emit started(_sockServer->serverPort());
		}
		else
		{
			qDebug() << "start error: " << _sockServer->errorString ();
			emit error(_sockServer->errorString ());
		}
	}
}

void WsServer::stop()
{
	if (_sockServer->isListening ())
	{
		_sockServer->close ();
		// disconnect clients
		for (QWebSocket* client : _clients)
		{
			_clients.removeAll(client);
			client->deleteLater();
		}

		emit stopped ();
	}
}

void WsServer::onClientConnected()
{
	qDebug() << "onClientConnected";
	while (_sockServer->hasPendingConnections ())
	{
		QWebSocket *socket = _sockServer->nextPendingConnection();
		qDebug() << getIdentifier(socket) << " connected!\n";

		if (_useSsl)
		{
            QSslConfiguration sslConf;
            sslConf.setLocalCertificateChain(_sslCerts);
            sslConf.setPrivateKey(_sslKey);
            sslConf.setPeerVerifyMode(QSslSocket::AutoVerifyPeer);
            socket->setSslConfiguration(sslConf);
            connect(socket, &QWebSocket::sslErrors, socket, &QWebSocket::deleteLater);
		}

		connect(socket, &QWebSocket::textMessageReceived, this, &WsServer::processTextMessage);
		connect(socket, &QWebSocket::binaryMessageReceived, this, &WsServer::processBinaryMessage);
		connect(socket, &QWebSocket::disconnected, this, &WsServer::onClientDisconnected);

		_clients << socket;

		// Json processor
		_jsonAPI = new JsonAPI(getIdentifier(socket), Logger::getInstance("WsServer"), true, this);
		connect(_jsonAPI, &JsonAPI::callbackMessage, this, &WsServer::sendMessage);
		connect(_jsonAPI, &JsonAPI::forceClose,  this, &WsServer::onClientDisconnected);
	}
}

void WsServer::onClientDisconnected()
{
	qDebug() << "onClientDisconnected";
    QWebSocket *client = qobject_cast<QWebSocket*>(sender());
	if (client)
	{
		_clients.removeAll(client);
		client->deleteLater();
	}
}

void WsServer::processTextMessage(QString message)
{
	qDebug() << "processTextMessage";
	_jsonAPI->handleMessage(message);
}

void WsServer::sendMessage(QJsonObject obj)
{
	qDebug() << "sendMessage";
	QJsonDocument writer(obj);
	QByteArray data = writer.toJson(QJsonDocument::Compact) + "\n";

	for (QWebSocket* client : _clients)
	{
		client->sendBinaryMessage(data);
	}
}

void WsServer::processBinaryMessage(QByteArray message)
{
	//uint8_t  priority   = message.at(0);
	//unsigned duration_s = message.at(1);
	unsigned imgSize    = message.size() - 4;
	unsigned width      = ((message.at(2) << 8) & 0xFF00) | (message.at(3) & 0xFF);
	unsigned height     =  imgSize / width;

	if ( imgSize % width > 0 )
	{
		// Error(_log, "message size is not multiple of width");
		return;
	}

	Image<ColorRgb> image;
	image.resize(width, height);

	memcpy(image.memptr(), message.data()+4, imgSize);
	//_hyperion->registerInput();
	//_hyperion->setInputImage(priority, image, duration_s*1000);
}

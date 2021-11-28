#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QHostAddress>
#include <QWebSocketServer>

//class NetOrigin;

class JsonAPI;

class WsServer : public QObject
{
	Q_OBJECT

public:
    explicit WsServer(QObject * parent = Q_NULLPTR);
	const QString & getServerName() const { return _serverName; };
	quint16 getServerPort () const { return _sockServer->serverPort(); };
	QString getErrorString() const { return _sockServer->errorString(); };
	bool isListening() { return _sockServer->isListening(); };

public slots:
	void start(quint16 port = 0);
	void stop();
	void setUseSecure(const bool ssl = true) { _useSsl = ssl; };
	void setServerName(const QString & serverName) { _serverName = serverName; };
	void setPrivateKey(const QSslKey & key) { _sslKey = key; };
	void setCertificates(const QList<QSslCertificate> & certs) { _sslCerts = certs; };

signals:
	void started(quint16 port);
	void stopped();
	void error(const QString & msg);
	void clientConnected(const QString & guid);
	void clientDisconnected(const QString & guid);

	void closed();

private slots:
	void onClientConnected();
	void onClientDisconnected();

	void processTextMessage(QString message);
	void processBinaryMessage(QByteArray message);
	void sendMessage(QJsonObject obj);

private:
	bool                   _useSsl;
	QSslKey                _sslKey;
	QList<QSslCertificate> _sslCerts;
	QString                _serverName;
	QWebSocketServer*      _sockServer;
	QList<QWebSocket*>     _clients;
	JsonAPI*               _jsonAPI;

};

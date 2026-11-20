#include "app/SingleInstance.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <memory>

namespace sora {

// Protocolo: una línea con la dirección (puede ir vacía) y el otro Sora responde "ok". Se espera la
// respuesta antes de cerrar: si se cierra antes de que el otro lea, en Windows se pierde el dato.

SingleInstance::SingleInstance(const QString& key, QObject* parent)
    : QObject(parent)
    , m_key(key)
{
}

bool SingleInstance::sendToRunning(const QString& message)
{
    QLocalSocket socket;
    socket.connectToServer(m_key);
    if (!socket.waitForConnected(300))
        return false;
    QString line = message;
    line.remove(QLatin1Char('\n'));
    socket.write(line.toUtf8() + '\n');
    socket.waitForBytesWritten(1000);
    socket.waitForReadyRead(2000); // la respuesta "ok"; si no llega, el otro Sora existe igualmente
    socket.disconnectFromServer();
    return true;
}

bool SingleInstance::listen()
{
    // shortcut: si dos Sora arrancan a la vez, ninguno encuentra al otro y ambos se abren; es raro.
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_key)) {
        QLocalServer::removeServer(m_key); // en Linux puede quedar el socket de un cierre brusco
        if (!m_server->listen(m_key))
            return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* socket = m_server->nextPendingConnection()) {
            auto data = std::make_shared<QByteArray>();
            connect(socket, &QLocalSocket::readyRead, socket, [this, socket, data] {
                *data += socket->readAll();
                const qsizetype end = data->indexOf('\n');
                if (end < 0)
                    return;
                socket->write("ok\n");
                socket->flush();
                emit received(QString::fromUtf8(data->left(end)));
            });
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
    return true;
}

} // namespace sora

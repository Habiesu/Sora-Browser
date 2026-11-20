#pragma once
#include <QObject>
#include <QString>

class QLocalServer;

namespace sora {

// Un solo Sora por usuario: si ya hay uno abierto, el nuevo le pasa su dirección y sale (dos
// procesos con el mismo perfil de Chromium pueden estropear cookies e historial).
class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(const QString& key, QObject* parent = nullptr);

    // true si había otro Sora y ha recibido `message` (vacío = solo abrir); este debe salir.
    bool sendToRunning(const QString& message);
    // Escucha a los Sora que se abran después; cada uno emite received().
    bool listen();

signals:
    void received(const QString& message);

private:
    QString m_key;
    QLocalServer* m_server = nullptr;
};

} // namespace sora

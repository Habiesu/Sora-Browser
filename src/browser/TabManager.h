#pragma once
#include <QIcon>
#include <QObject>
#include <QUrl>

class QStackedWidget;
class QWebEngineProfile;

namespace sora {

class AdblockService;
class Tab;

// Crea, cierra, mueve y selecciona pestañas. El QStackedWidget es la fuente de verdad del orden.
class TabManager : public QObject {
    Q_OBJECT
public:
    TabManager(QWebEngineProfile* profile, const AdblockService* adblock, QStackedWidget* stack,
               QObject* parent = nullptr);

    // index −1 = al final. La primera pestaña siempre queda activa.
    Tab* newTab(const QUrl& url = {}, int index = -1, bool activate = true);
    void closeTab(int index);
    void moveTab(int from, int to);
    void setCurrentIndex(int index);

    int currentIndex() const;
    int count() const;
    Tab* tab(int index) const;
    Tab* currentTab() const;
    int indexOf(const Tab* tab) const;

    // Suma un bloqueo a la pestaña de ese sitio (la actual si coincide; si no, la primera).
    void countBlocked(const QUrl& firstParty);

signals:
    void tabInserted(int index);
    void tabRemoved(int index);
    void currentChanged(int index);
    void tabTitleChanged(int index, const QString& title);
    void tabIconChanged(int index, const QIcon& icon);
    void lastTabClosed();

private:
    QWebEngineProfile* m_profile;
    const AdblockService* m_adblock;
    QStackedWidget* m_stack;
    bool m_quiet = false; // durante quitar/mover no se reenvían los cambios intermedios del stack
};

} // namespace sora

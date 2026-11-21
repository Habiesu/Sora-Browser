#pragma once
#include <QMenu>

class QLabel;

namespace sora {

// Menú principal (⋯) con la estructura del de Chrome/Brave. Solo emite órdenes; las ejecuta la
// ventana. Los atajos se muestran aquí y los atiende la ventana (QShortcut).
class MainMenu : public QMenu {
    Q_OBJECT
public:
    enum class Command {
        NewTab, NewWindow, NewIncognitoWindow,
        History, Downloads, AddBookmark, ToggleBookmarksBar, BookmarkManager,
        ZoomIn, ZoomOut, ZoomReset, FullScreen,
        Print, Find, SavePage, ClearBrowsingData, DevTools,
        Extensions, Settings, About, Quit
    };
    Q_ENUM(Command)

    explicit MainMenu(QWidget* parent = nullptr);
    void setZoomPercent(int percent);

signals:
    void command(sora::MainMenu::Command which);

private:
    QLabel* m_zoomLevel;
    QAction* add(QMenu* menu, const QString& text, Command which, const QKeySequence& keys = {});
};

} // namespace sora

Q_DECLARE_METATYPE(sora::MainMenu::Command)

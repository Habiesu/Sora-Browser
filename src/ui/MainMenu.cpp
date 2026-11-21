#include "ui/MainMenu.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QWidgetAction>

namespace sora {

QAction* MainMenu::add(QMenu* menu, const QString& text, Command which, const QKeySequence& keys)
{
    QAction* action = menu->addAction(text);
    // El atajo se ve en el menú; quien lo atiende es la ventana (aquí solo con el menú abierto).
    action->setShortcut(keys);
    action->setShortcutContext(Qt::WidgetShortcut);
    connect(action, &QAction::triggered, this, [this, which] { emit command(which); });
    return action;
}

MainMenu::MainMenu(QWidget* parent)
    : QMenu(parent)
    , m_zoomLevel(new QLabel(QStringLiteral("100 %")))
{
    add(this, tr("Nueva pestaña"), Command::NewTab, QKeySequence(Qt::CTRL | Qt::Key_T));
    add(this, tr("Nueva ventana"), Command::NewWindow, QKeySequence(Qt::CTRL | Qt::Key_N));
    add(this, tr("Nueva ventana de incógnito"), Command::NewIncognitoWindow, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    addSeparator();
    add(this, tr("Historial"), Command::History, QKeySequence(Qt::CTRL | Qt::Key_H));
    add(this, tr("Descargas"), Command::Downloads, QKeySequence(Qt::CTRL | Qt::Key_J));
    QMenu* bookmarks = addMenu(tr("Marcadores"));
    add(bookmarks, tr("Añadir esta página a marcadores..."), Command::AddBookmark, QKeySequence(Qt::CTRL | Qt::Key_D));
    add(bookmarks, tr("Mostrar barra de marcadores"), Command::ToggleBookmarksBar, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    add(bookmarks, tr("Administrador de marcadores"), Command::BookmarkManager, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    addSeparator();

    // Fila de zoom como Chrome: "Zoom   −  100 %  +   ⛶".
    auto* row = new QWidget;
    row->setObjectName(QStringLiteral("ZoomRow"));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 2, 8, 2);
    layout->addWidget(new QLabel(tr("Zoom")), 1);
    const auto button = [&](const char* name, const QString& text, const QString& tip, Command which) {
        auto* b = new QToolButton;
        b->setObjectName(QLatin1String(name));
        b->setText(text);
        b->setToolTip(tip);
        b->setAutoRaise(true);
        b->setFixedSize(30, 26);
        connect(b, &QToolButton::clicked, this, [this, which] { emit command(which); });
        layout->addWidget(b);
    };
    button("ZoomOut", QStringLiteral("−"), tr("Reducir (Ctrl+-)"), Command::ZoomOut);
    m_zoomLevel->setObjectName(QStringLiteral("ZoomLevel"));
    m_zoomLevel->setAlignment(Qt::AlignCenter);
    m_zoomLevel->setMinimumWidth(52);
    layout->addWidget(m_zoomLevel);
    button("ZoomIn", QStringLiteral("+"), tr("Ampliar (Ctrl++)"), Command::ZoomIn);
    layout->addSpacing(6);
    button("FullScreen", QStringLiteral("⛶"), tr("Pantalla completa (F11)"), Command::FullScreen);
    auto* zoom = new QWidgetAction(this);
    zoom->setText(tr("Zoom"));
    zoom->setDefaultWidget(row);
    addAction(zoom);
    addSeparator();

    add(this, tr("Imprimir..."), Command::Print, QKeySequence(Qt::CTRL | Qt::Key_P));
    add(this, tr("Buscar..."), Command::Find, QKeySequence(Qt::CTRL | Qt::Key_F));
    QMenu* tools = addMenu(tr("Más herramientas"));
    add(tools, tr("Guardar página como..."), Command::SavePage, QKeySequence(Qt::CTRL | Qt::Key_S));
    add(tools, tr("Borrar datos de navegación..."), Command::ClearBrowsingData, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Delete));
    add(tools, tr("Herramientas para desarrolladores"), Command::DevTools, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
    addSeparator();
    add(this, tr("Extensiones"), Command::Extensions);
    add(this, tr("Configuración"), Command::Settings);
    QMenu* help = addMenu(tr("Ayuda"));
    add(help, tr("Acerca de Sora"), Command::About);
    addSeparator();
    add(this, tr("Salir"), Command::Quit);
}

void MainMenu::setZoomPercent(int percent)
{
    m_zoomLevel->setText(QStringLiteral("%1 %").arg(percent));
}

} // namespace sora

#include "app/MainWindow.h"

#include <QLineEdit>
#include <QRegularExpression>
#include <QToolBar>
#include <QUrlQuery>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineView>

namespace sora {

QUrl urlFromInput(const QString& rawInput)
{
    const QString input = rawInput.trimmed();

    static const QRegularExpression scheme(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+.-]*:"));
    static const QRegularExpression localhost(QStringLiteral("^localhost(:\\d+)?(/.*)?$"),
                                              QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression domain(
        QStringLiteral("^([a-zA-Z0-9-]+\\.)+[a-zA-Z]{2,}(:\\d+)?(/\\S*)?$"));

    if (localhost.match(input).hasMatch())
        return QUrl(QStringLiteral("http://") + input);
    if (domain.match(input).hasMatch())
        return QUrl(QStringLiteral("https://") + input);
    // "localhost:8080" también encaja con el patrón de esquema, por eso se comprueba antes.
    if (!input.contains(QLatin1Char(' ')) && scheme.match(input).hasMatch())
        return QUrl(input);

    QUrl search(QStringLiteral("https://duckduckgo.com/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), input);
    search.setQuery(query);
    return search;
}

MainWindow::MainWindow(QWebEngineProfile* profile, QWidget* parent)
    : QMainWindow(parent)
{
    m_view = new QWebEngineView(this);
    m_view->setPage(new QWebEnginePage(profile, m_view));
    setCentralWidget(m_view);

    m_address = new QLineEdit(this);
    m_address->setPlaceholderText(tr("Busca o escribe una dirección"));
    auto* bar = addToolBar(tr("Navegación"));
    bar->setMovable(false);
    bar->addWidget(m_address);

    connect(m_address, &QLineEdit::returnPressed, this, [this] { navigate(m_address->text()); });
    connect(m_view, &QWebEngineView::urlChanged, this,
            [this](const QUrl& url) { m_address->setText(url.toDisplayString()); });
    connect(m_view, &QWebEngineView::titleChanged, this,
            [this](const QString& title) { setWindowTitle(title.isEmpty() ? QStringLiteral("Sora") : title + QStringLiteral(" — Sora")); });

    setWindowTitle(QStringLiteral("Sora"));
    resize(1280, 800);
}

void MainWindow::navigate(const QString& input)
{
    m_view->setUrl(urlFromInput(input));
}

} // namespace sora

#include <QApplication>
#include <QWebEngineProfile>

#include "app/MainWindow.h"
#include "privacy/RequestInterceptor.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Sora"));
    QApplication::setOrganizationName(QStringLiteral("Sora"));

    auto* profile = new QWebEngineProfile(QStringLiteral("default"), &app);

    auto* interceptor = new sora::RequestInterceptor(&app);
    profile->setUrlRequestInterceptor(interceptor);

    sora::MainWindow window(profile);
    QObject::connect(interceptor, &sora::RequestInterceptor::blocked, &window,
                     [&window] { window.countBlocked(); }, Qt::QueuedConnection);
    window.show();

    const QStringList args = QApplication::arguments();
    window.navigate(args.size() > 1 ? args.last() : QStringLiteral("about:blank"));

    return app.exec();
}

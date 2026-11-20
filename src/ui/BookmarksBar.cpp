#include "ui/BookmarksBar.h"

#include <QHBoxLayout>
#include <QLabel>

namespace sora {

BookmarksBar::BookmarksBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("BookmarksBar"));
    setAttribute(Qt::WA_StyledBackground);
    setFixedHeight(30);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 14, 0);
    layout->addWidget(new QLabel(tr("Aún no hay marcadores"), this));
    layout->addStretch();
}

} // namespace sora

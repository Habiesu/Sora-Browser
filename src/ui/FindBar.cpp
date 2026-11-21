#include "ui/FindBar.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

namespace sora {

FindBar::FindBar(QWidget* parent)
    : QWidget(parent)
    , m_text(new QLineEdit(this))
    , m_count(new QLabel(this))
{
    setObjectName(QStringLiteral("FindBar"));
    setAttribute(Qt::WA_StyledBackground);
    m_text->setObjectName(QStringLiteral("FindText"));
    m_text->setPlaceholderText(tr("Buscar en la página"));
    m_text->setMinimumWidth(220);
    m_count->setObjectName(QStringLiteral("FindCount"));
    m_count->setMinimumWidth(60);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 6, 6);
    layout->setSpacing(4);
    layout->addWidget(m_text, 1);
    layout->addWidget(m_count);
    const auto button = [&](const QString& text, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setText(text);
        b->setToolTip(tip);
        b->setAutoRaise(true);
        b->setFixedSize(28, 28);
        layout->addWidget(b);
        return b;
    };
    connect(button(QStringLiteral("˄"), tr("Anterior (Mayús+Intro)")), &QToolButton::clicked, this,
            [this] { emit findRequested(m_text->text(), true); });
    connect(button(QStringLiteral("˅"), tr("Siguiente (Intro)")), &QToolButton::clicked, this,
            [this] { emit findRequested(m_text->text(), false); });
    connect(button(QStringLiteral("✕"), tr("Cerrar (Esc)")), &QToolButton::clicked, this, &FindBar::close);
    connect(m_text, &QLineEdit::textEdited, this, [this](const QString& text) { emit findRequested(text, false); });
    connect(m_text, &QLineEdit::returnPressed, this, [this] {
        emit findRequested(m_text->text(), QGuiApplication::keyboardModifiers() & Qt::ShiftModifier);
    });
    hide();
}

void FindBar::open()
{
    show();
    raise();
    m_text->setFocus(Qt::ShortcutFocusReason);
    m_text->selectAll();
    if (!m_text->text().isEmpty())
        emit findRequested(m_text->text(), false);
}

void FindBar::setResult(int active, int total)
{
    m_count->setText(m_text->text().isEmpty() ? QString() : tr("%1 de %2").arg(active).arg(total));
}

void FindBar::close()
{
    hide();
    emit closed();
}

void FindBar::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace sora

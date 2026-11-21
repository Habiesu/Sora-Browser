#pragma once
#include <QWidget>

class QLabel;
class QLineEdit;

namespace sora {

// Barra de "Buscar en la página" (Ctrl+F), arriba a la derecha como en Chrome.
class FindBar : public QWidget {
    Q_OBJECT
public:
    explicit FindBar(QWidget* parent = nullptr);
    void open(); // muestra, enfoca y selecciona lo escrito
    void setResult(int active, int total);

signals:
    void findRequested(const QString& text, bool backward);
    void closed();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    QLineEdit* m_text;
    QLabel* m_count;
    void close();
};

} // namespace sora

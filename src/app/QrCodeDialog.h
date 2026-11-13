#pragma once

#include "QrCodeObject.h"

#include <QDialog>
#include <QMap>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QStackedWidget;
class QWidget;

namespace vek {

class ColorButton;

// QR-Code einfügen oder bearbeiten: Art des Inhalts (Webseite, Text, E-Mail, Telefon, SMS,
// WLAN, Visitenkarte) mit eigenen Feldern, Fehlerkorrektur, Form der Module, Ruhezone,
// Farben und beim Einfügen die Grösse. Rechts eine Vorschau mit Version und Modulzahl.
class QrCodeDialog : public QDialog {
    Q_OBJECT

public:
    // inserting: neuer Code (Grösse wählbar), sonst Bearbeiten eines bestehenden
    QrCodeDialog(const QrCodeObject& initial, bool inserting, QWidget* parent = nullptr);

    // Werte des Dialogs ins Objekt (Inhalt, Fehlerkorrektur, Form, Ruhezone, Farben, Grösse)
    void applyTo(QrCodeObject& object) const;

    QComboBox* kindBox() const { return m_kind; }
    // Eingabefeld (QLineEdit, QPlainTextEdit oder QComboBox) der aktuellen Art
    QWidget* field(const QString& id) const;
    QString info() const;
    bool isValid() const;

private:
    QMap<QString, QString> values() const;
    void refresh();
    void showPage(int index);

    bool m_inserting;
    QComboBox* m_kind = nullptr;
    QStackedWidget* m_pages = nullptr;
    QList<QMap<QString, QWidget*>> m_fields; // je Art
    QComboBox* m_ecc = nullptr;
    QComboBox* m_shape = nullptr;
    QSpinBox* m_quiet = nullptr;
    ColorButton* m_color = nullptr;
    QCheckBox* m_hasBackground = nullptr;
    ColorButton* m_background = nullptr;
    QDoubleSpinBox* m_size = nullptr;
    QLabel* m_preview = nullptr;
    QLabel* m_info = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
};

} // namespace vek

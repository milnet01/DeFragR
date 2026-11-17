// Languages: the window in the desktop's language (S13; docs/design.md
// § Wording and languages). DeFragR's translations are built into the
// program under :/i18n. Qt's own, for its standard buttons, come from
// Qt's translations folder when it has one.
#pragma once

#include <QLocale>
#include <QTranslator>

namespace defragr::ui {

class Languages {
public:
    // Installs the first of the locale's languages DeFragR has a
    // translation for, else English, whose file holds only its plural
    // forms. Installs Qt's own words in the same language, and lays the
    // window out right to left for a right-to-left language. Make one
    // after the application and before any window. qtFolder holds Qt's
    // own translations; left out, it is Qt's translations folder.
    explicit Languages(const QLocale &locale = QLocale(), const QString &qtFolder = {});

    // The language installed, such as Hebrew or English.
    QLocale chosen() const { return m_chosen; }

private:
    // DeFragR's own translations. In a right-to-left language, a
    // translation that would read left to right from its first strong
    // letter, such as one starting "DeFragR", gets RIGHT-TO-LEFT MARK in
    // front: a label takes a plain text's direction from that letter, not
    // from the window.
    class OwnTranslator : public QTranslator {
    public:
        QString translate(const char *context, const char *sourceText,
                          const char *disambiguation = nullptr, int n = -1) const override;
        bool rightToLeft = false;
    };

    // Each removes itself from the application when destroyed.
    OwnTranslator m_own;
    QTranslator m_qt;
    QLocale m_chosen{QLocale::English};
};

}  // namespace defragr::ui

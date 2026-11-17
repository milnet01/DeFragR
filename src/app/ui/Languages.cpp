#include "ui/Languages.h"

#include <QGuiApplication>
#include <QLibraryInfo>

namespace defragr::ui {

Languages::Languages(const QLocale &locale, const QString &qtFolder)
{
    const QString folder = QStringLiteral(":/i18n");
    // Only a broken build has no English file. Its plural sentences then
    // read "file(s)".
    if (!m_own.load(locale, QStringLiteral("defragr"), QStringLiteral("_"), folder)
        && !m_own.load(QStringLiteral("defragr_en"), folder))
        return;
    m_chosen = QLocale(m_own.language());
    m_own.rightToLeft = m_chosen.textDirection() == Qt::RightToLeft;
    QCoreApplication::installTranslator(&m_own);
    if (m_chosen.language() == QLocale::English)
        return;

    // Qt's words in DeFragR's language, never the desktop's, so the two
    // never mix. Missing is fine: Qt's words stay English.
    if (m_qt.load(m_chosen, QStringLiteral("qtbase"), QStringLiteral("_"),
                  qtFolder.isNull() ? QLibraryInfo::path(QLibraryInfo::TranslationsPath)
                                    : qtFolder))
        QCoreApplication::installTranslator(&m_qt);
    // Set outright, so it does not rest on Qt's translation being there.
    // Left alone otherwise, so `defragr -reverse` still works.
    if (m_chosen.textDirection() == Qt::RightToLeft)
        QGuiApplication::setLayoutDirection(Qt::RightToLeft);
}

QString Languages::OwnTranslator::translate(const char *context, const char *sourceText,
                                            const char *disambiguation, int n) const
{
    QString text = QTranslator::translate(context, sourceText, disambiguation, n);
    // Empty means no translation, and Qt shows the English instead.
    if (rightToLeft && !text.isEmpty() && !text.isRightToLeft())
        text.prepend(QChar(0x200F));
    return text;
}

}  // namespace defragr::ui

// FixedRoom: how a page gives each thing on it a room of its own, so that
// showing, hiding or rewording one thing moves and resizes nothing else
// (the owner's rule of 2026-11-19; ROADMAP DFR-0110).
#pragma once

#include <QLabel>
#include <QSizePolicy>
#include <QStringList>
#include <QWidget>

namespace defragr::ui {

// The widget keeps its place while hidden, so nothing below it moves.
inline void keepPlace(QWidget *widget)
{
    QSizePolicy policy = widget->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    widget->setSizePolicy(policy);
}

// The label has room for this many lines from the start, text at the top,
// so a line more or fewer moves nothing below it.
inline void keepLines(QLabel *label, int lines)
{
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    // The label's own measure of that many lines, with the mark an
    // elided path carries.
    const QString was = label->text();
    label->setText(QStringList(lines, QStringLiteral("X\u2026")).join(QLatin1Char('\n')));
    label->setMinimumHeight(label->sizeHint().height());
    label->setText(was);
}

}  // namespace defragr::ui

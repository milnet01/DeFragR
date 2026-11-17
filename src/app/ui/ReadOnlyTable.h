// ReadOnlyTable: the set-up the window's tables share. Each lists results
// the user reads and picks rows from, and never edits.
#pragma once

#include <QAbstractItemModel>
#include <QHeaderView>
#include <QTableView>

namespace defragr::ui {

// Whole rows are picked, nothing is edited, rows are not numbered, and
// each column is as wide as its contents, the last taking what is left.
inline QTableView *readOnlyTable(QAbstractItemModel *model)
{
    auto *table = new QTableView;
    table->setModel(model);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    return table;
}

}  // namespace defragr::ui

/*
 * Copyright (C) 2019-2021 Ashar Khan <ashar786khan@gmail.com>
 *
 * This file is part of CP Editor.
 *
 * CP Editor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "LSPCompleter.hpp"
#include <QAbstractItemView>
#include <QFrame>
#include <QStandardItem>
#include <QStandardItemModel>

namespace Extensions
{
LSPCompleter::LSPCompleter(QObject *parent) : QCompleter(parent)
{
    model = new QStandardItemModel(this);
    setModel(model);
    setCompletionMode(QCompleter::PopupCompletion);
    setModelSorting(QCompleter::UnsortedModel);
    setCompletionRole(Qt::UserRole + 1);
    setCaseSensitivity(Qt::CaseSensitive);
    setWrapAround(true);
    setMaxVisibleItems(12);

    auto *completionPopup = popup();
    completionPopup->setFrameShape(QFrame::Box);
    completionPopup->setLineWidth(1);
    completionPopup->setAutoFillBackground(true);
    completionPopup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    completionPopup->setMouseTracking(true);
    completionPopup->setTextElideMode(Qt::ElideRight);
}

void LSPCompleter::clearCompletion()
{
    completionItems.clear();
    model->clear();
}

void LSPCompleter::setCompletions(const QVector<CompletionItem> &items)
{
    completionItems = items;
    model->clear();
    for (int i = 0; i < completionItems.size(); ++i)
    {
        const auto &completion = completionItems.at(i);
        auto *item = new QStandardItem(completion.label);
        item->setData(i, Qt::UserRole);
        item->setData(completion.filterText.isEmpty() ? completion.label : completion.filterText, Qt::UserRole + 1);
        item->setData(completion.detail.isEmpty() ? completion.documentation : completion.detail, Qt::ToolTipRole);
        model->appendRow(item);
    }
}

CompletionItem LSPCompleter::completionForIndex(const QModelIndex &index) const
{
    bool ok = false;
    const int itemIndex = index.data(Qt::UserRole).toInt(&ok);
    if (ok && itemIndex >= 0 && itemIndex < completionItems.size())
        return completionItems.at(itemIndex);
    return {};
}
} // namespace Extensions

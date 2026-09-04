/*
 * Copyright (C) 2019-2021 Ashar Khan <ashar786khan@gmail.com>
 *
 * This file is part of CP Editor.
 *
 * CP Editor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * I will not be responsible if CP Editor behaves in unexpected way and
 * causes your ratings to go down and or lose any important contest.
 *
 * Believe Software is "Software" and it isn't immune to bugs.
 *
 */

#include "LSPCompleter.hpp"
#include <QAbstractItemView>
#include <QFrame>
#include <QStringListModel>

namespace Extensions
{
LSPCompleter::LSPCompleter(QObject *parent) : QCompleter(parent)
{
    model = new QStringListModel(this);
    setModel(model);
    setCompletionMode(QCompleter::PopupCompletion);
    setModelSorting(QCompleter::UnsortedModel);
    setCaseSensitivity(Qt::CaseSensitive);
    setWrapAround(true);
    setMaxVisibleItems(12);

    auto *completionPopup = popup();
    completionPopup->setFrameShape(QFrame::Box);
    completionPopup->setLineWidth(1);
    completionPopup->setAutoFillBackground(true);
    completionPopup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    completionPopup->setTextElideMode(Qt::ElideRight);
}

void LSPCompleter::clearCompletion()
{
    model->setStringList({});
}

void LSPCompleter::setCompletions(const QStringList &items)
{
    model->setStringList(items);
}
} // namespace Extensions

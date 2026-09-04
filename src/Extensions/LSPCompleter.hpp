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
#ifndef LSP_COMPLETER_HPP
#define LSP_COMPLETER_HPP

#include <QCompleter>
#include <QVector>

class QStandardItemModel;

namespace Extensions
{
struct CompletionItem
{
    QString label;
    QString insertText;
    QString detail;
    QString documentation;
    QString filterText;
    QString sortText;
    bool hasTextEdit = false;
    bool isSnippet = false;
    int startLine = -1;
    int startCharacter = -1;
    int endLine = -1;
    int endCharacter = -1;
};

class LSPCompleter : public QCompleter
{
    Q_OBJECT

  public:
    explicit LSPCompleter(QObject *parent = nullptr);

    void clearCompletion();
    void setCompletions(const QVector<CompletionItem> &items);
    CompletionItem completionForIndex(const QModelIndex &index) const;

  private:
    QStandardItemModel *model = nullptr;
    QVector<CompletionItem> completionItems;
};
} // namespace Extensions

#endif // LSP_COMPLETER_HPP

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
#ifndef LSP_COMPLETER_HPP
#define LSP_COMPLETER_HPP

#include <QCompleter>

class QStringListModel;

namespace Extensions
{
class LSPCompleter : public QCompleter
{
    Q_OBJECT

  public:
    explicit LSPCompleter(QObject *parent = nullptr);

    void clearCompletion();
    void setCompletions(const QStringList &items);

  private:
    QStringListModel *model = nullptr;
};
} // namespace Extensions

#endif // LSP_COMPLETER_HPP

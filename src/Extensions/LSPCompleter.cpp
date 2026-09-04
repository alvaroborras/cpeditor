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
#include <QApplication>
#include <QFrame>
#include <QPainter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

namespace Extensions
{
namespace
{
constexpr int CompletionIndexRole = Qt::UserRole;
constexpr int CompletionFilterRole = Qt::UserRole + 1;
constexpr int CompletionDetailRole = Qt::UserRole + 2;
constexpr int CompletionKindRole = Qt::UserRole + 3;
constexpr int MaximumPopupWidth = 760;

QString completionKindName(int kind)
{
    switch (kind)
    {
    case 1:
        return QObject::tr("Text");
    case 2:
        return QObject::tr("Method");
    case 3:
        return QObject::tr("Function");
    case 4:
        return QObject::tr("Constructor");
    case 5:
        return QObject::tr("Field");
    case 6:
        return QObject::tr("Variable");
    case 7:
        return QObject::tr("Class");
    case 8:
        return QObject::tr("Interface");
    case 9:
        return QObject::tr("Module");
    case 10:
        return QObject::tr("Property");
    case 11:
        return QObject::tr("Unit");
    case 12:
        return QObject::tr("Value");
    case 13:
        return QObject::tr("Enum");
    case 14:
        return QObject::tr("Keyword");
    case 15:
        return QObject::tr("Snippet");
    case 16:
        return QObject::tr("Color");
    case 17:
        return QObject::tr("File");
    case 18:
        return QObject::tr("Reference");
    case 19:
        return QObject::tr("Folder");
    case 20:
        return QObject::tr("Enum member");
    case 21:
        return QObject::tr("Constant");
    case 22:
        return QObject::tr("Struct");
    case 23:
        return QObject::tr("Event");
    case 24:
        return QObject::tr("Operator");
    case 25:
        return QObject::tr("Type parameter");
    default:
        return {};
    }
}

class CompletionItemDelegate final : public QStyledItemDelegate
{
  public:
    explicit CompletionItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QString label = index.data(Qt::DisplayRole).toString();
        const QString detail = index.data(CompletionDetailRole).toString();
        const QString kind = index.data(CompletionKindRole).toString();

        QFont labelFont = option.font;
        labelFont.setBold(true);
        QFont detailFont = option.font;
        detailFont.setPointSize(qMax(1, detailFont.pointSize() - 1));
        const QFontMetrics labelMetrics(labelFont);
        const QFontMetrics detailMetrics(detailFont);
        const int kindWidth = kind.isEmpty() ? 0 : QFontMetrics(option.font).horizontalAdvance(kind);
        const int firstLineWidth = labelMetrics.horizontalAdvance(label) + kindWidth + (kind.isEmpty() ? 0 : 16);
        const int secondLineWidth = detail.isEmpty() ? 0 : detailMetrics.horizontalAdvance(detail);
        const int width = qMin(MaximumPopupWidth, qMax(firstLineWidth, secondLineWidth) + 20);
        const int height = labelMetrics.height() + (detail.isEmpty() ? 0 : detailMetrics.height()) + 8;
        return {width, height};
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem backgroundOption(option);
        initStyleOption(&backgroundOption, index);
        backgroundOption.text.clear();
        backgroundOption.icon = {};
        const auto *style = option.widget != nullptr ? option.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &backgroundOption, painter, option.widget);

        const bool selected = option.state & QStyle::State_Selected;
        const QColor primaryColor =
            selected ? option.palette.color(QPalette::HighlightedText) : option.palette.color(QPalette::Text);
        QColor secondaryColor = primaryColor;
        secondaryColor.setAlpha(selected ? 220 : 165);

        const QRect contentRect = option.rect.adjusted(8, 3, -8, -3);
        QFont labelFont = option.font;
        labelFont.setBold(true);
        QFont detailFont = option.font;
        detailFont.setPointSize(qMax(1, detailFont.pointSize() - 1));
        const QFontMetrics labelMetrics(labelFont);
        const QFontMetrics detailMetrics(detailFont);
        const QString label = index.data(Qt::DisplayRole).toString();
        const QString detail = index.data(CompletionDetailRole).toString();
        const QString kind = index.data(CompletionKindRole).toString();
        const int kindWidth = kind.isEmpty() ? 0 : QFontMetrics(option.font).horizontalAdvance(kind);

        painter->setPen(primaryColor);
        painter->setFont(labelFont);
        const int labelWidth = qMax(0, contentRect.width() - kindWidth - (kind.isEmpty() ? 0 : 12));
        painter->drawText(contentRect.left(), contentRect.top() + labelMetrics.ascent(),
                          labelMetrics.elidedText(label, Qt::ElideRight, labelWidth));

        if (!kind.isEmpty())
        {
            painter->setFont(option.font);
            painter->setPen(secondaryColor);
            painter->drawText(contentRect.right() - kindWidth + 1, contentRect.top() + labelMetrics.ascent(), kind);
        }

        if (!detail.isEmpty())
        {
            painter->setFont(detailFont);
            painter->setPen(secondaryColor);
            const int detailTop = contentRect.top() + labelMetrics.height() + detailMetrics.ascent();
            painter->drawText(contentRect.left(), detailTop,
                              detailMetrics.elidedText(detail, Qt::ElideRight, contentRect.width()));
        }
    }
};
} // namespace

LSPCompleter::LSPCompleter(QObject *parent) : QCompleter(parent)
{
    model = new QStandardItemModel(this);
    setModel(model);
    setCompletionMode(QCompleter::PopupCompletion);
    setModelSorting(QCompleter::UnsortedModel);
    setCompletionRole(CompletionFilterRole);
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
    completionPopup->setItemDelegate(new CompletionItemDelegate(completionPopup));
    completionPopup->setMaximumWidth(MaximumPopupWidth);
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
        item->setData(i, CompletionIndexRole);
        item->setData(completion.filterText.isEmpty() ? completion.label : completion.filterText, CompletionFilterRole);
        QString detail = completion.detail;
        if (detail.isEmpty())
            detail = completion.labelDescription;
        else if (!completion.labelDescription.isEmpty())
            detail += "  " + completion.labelDescription;
        item->setData(detail, CompletionDetailRole);
        const QString kind = completionKindName(completion.kind);
        item->setData(kind, CompletionKindRole);

        QStringList tooltip;
        tooltip.append(completion.label);
        if (!kind.isEmpty())
            tooltip.append(kind);
        if (!completion.detail.isEmpty())
            tooltip.append(completion.detail);
        if (!completion.labelDescription.isEmpty())
            tooltip.append(completion.labelDescription);
        if (!completion.documentation.isEmpty())
            tooltip.append(completion.documentation);
        item->setData(tooltip.join("\n\n"), Qt::ToolTipRole);
        model->appendRow(item);
    }
}

CompletionItem LSPCompleter::completionForIndex(const QModelIndex &index) const
{
    bool ok = false;
    const int itemIndex = index.data(CompletionIndexRole).toInt(&ok);
    if (ok && itemIndex >= 0 && itemIndex < completionItems.size())
        return completionItems.at(itemIndex);
    return {};
}
} // namespace Extensions

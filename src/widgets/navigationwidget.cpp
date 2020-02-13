/*
 * Copyright (c) 2014-2020 Patrizio Bekerle -- <patrizio@bekerle.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "navigationwidget.h"

#include <libraries/qmarkdowntextedit/markdownhighlighter.h>

#include <QDebug>
#include <QRegularExpression>
#include <QTextBlock>
#include <QtConcurrent/QtConcurrent>

NavigationWidget::NavigationWidget(QWidget *parent) : QTreeWidget(parent) {
    QObject::connect(
        this, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
        this, SLOT(onCurrentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));

    parseFutureWatcher = new QFutureWatcher<QVector<Node>>(this);
    connect(parseFutureWatcher, SIGNAL(finished()), this,
            SLOT(onParseCompleted()));
}

/**
 * Sets a document to parse
 */
void NavigationWidget::setDocument(QTextDocument *document) {
    _document = document;
}

/**
 * Emits the positionClicked signal to jump to the clicked navigation item's
 * position
 */
void NavigationWidget::onCurrentItemChanged(QTreeWidgetItem *current,
                                            QTreeWidgetItem *previous) {
    Q_UNUSED(previous)

    if (current == nullptr) {
        return;
    }

    emit positionClicked(current->data(0, Qt::UserRole).toInt());
}

/**
 * Parses a text document and builds the navigation tree for it
 */
void NavigationWidget::parse(QTextDocument *document) {
    const QSignalBlocker blocker(this);
    Q_UNUSED(blocker)

    setDocument(document);

    QFuture<QVector<Node>> future =
        QtConcurrent::run(this, &NavigationWidget::parseDocument, document);
    this->parseFutureWatcher->setFuture(future);
}

QVector<Node> NavigationWidget::parseDocument(QTextDocument *document) {
    QVector<Node> nodes;
    for (int i = 0; i < document->blockCount(); i++) {
        QTextBlock block = document->findBlockByNumber(i);
        int elementType = block.userState();
        QString text = block.text();

        // ignore all non headline types
        if ((elementType < MarkdownHighlighter::H1) ||
            (elementType > MarkdownHighlighter::H6)) {
            continue;
        }

        text.remove(QRegularExpression(QStringLiteral("^#+\\s+")));

        if (text.isEmpty()) {
            continue;
        }

        Node node = {text, block.position(), elementType};
        nodes.append(node);
    }
    return nodes;
}

void NavigationWidget::onParseCompleted() {
    QVector<Node> nodes = this->parseFutureWatcher->result();
    if (navigationTreeNodes == nodes) return;

    clear();
    _lastHeadingItemList.clear();

    for (int i = 0; i < nodes.length(); ++i) {
        Node node = nodes.at(i);
        int elementType = node.elementType;
        int pos = node.pos;
        QString text = node.text;

        auto *item = new QTreeWidgetItem();
        item->setText(0, text);
        item->setData(0, Qt::UserRole, pos);
        item->setToolTip(
            0,
            tr("headline %1").arg(elementType - MarkdownHighlighter::H1 + 1));

        // attempt to find a suitable parent item for the element type
        QTreeWidgetItem *lastHigherItem = findSuitableParentItem(elementType);

        if (lastHigherItem == nullptr) {
            // if there wasn't a last higher level item then add the current
            // item to the top level
            addTopLevelItem(item);
        } else {
            // if there was a last higher level item then add the current
            // item as child of that item
            lastHigherItem->addChild(item);
        }

        _lastHeadingItemList[elementType] = item;
    }
    expandAll();
}

/**
 * Attempts to find a suitable parent item for the element type
 */
QTreeWidgetItem *NavigationWidget::findSuitableParentItem(int elementType) {
    elementType--;
    QTreeWidgetItem *lastHigherItem = _lastHeadingItemList[elementType];

    return ((lastHigherItem == nullptr) &&
            (elementType > MarkdownHighlighter::H1))
               ? findSuitableParentItem(elementType)
               : lastHigherItem;
}

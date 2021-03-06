/*
 * tilestampdock.cpp
 * Copyright 2015, Thorbjørn Lindeijer <bjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tilestampsdock.h"

#include "documentmanager.h"
#include "tilestamp.h"
#include "tilestampmanager.h"
#include "tilestampmodel.h"
#include "utils.h"

#include "stampbrush.h"
#include "bucketfilltool.h"

#include <QAction>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QToolBar>
#include <QVBoxLayout>

namespace Tiled {
namespace Internal {

TileStampsDock::TileStampsDock(TileStampManager *stampManager, QWidget *parent)
    : QDockWidget(parent)
    , mTileStampManager(stampManager)
    , mTileStampModel(stampManager->tileStampModel())
    , mNewStamp(new QAction(this))
    , mAddVariation(new QAction(this))
    , mDelete(new QAction(this))
{
    setObjectName(QLatin1String("TileStampsDock"));

    mTileStampView = new TileStampView(this);
    mTileStampView->setModel(mTileStampModel);
    mTileStampView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mTileStampView->header()->setStretchLastSection(false);
    mTileStampView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    mTileStampView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    mTileStampView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mTileStampView, SIGNAL(customContextMenuRequested(QPoint)),
            SLOT(showContextMenu(QPoint)));

    mNewStamp->setIcon(QIcon(QLatin1String(":images/16x16/document-new.png")));
    mAddVariation->setIcon(QIcon(QLatin1String(":/images/16x16/add.png")));
    mDelete->setIcon(QIcon(QLatin1String(":images/16x16/edit-delete.png")));

    Utils::setThemeIcon(mNewStamp, "document-new");
    Utils::setThemeIcon(mAddVariation, "add");
    Utils::setThemeIcon(mDelete, "edit-delete");

    connect(mNewStamp, SIGNAL(triggered()), stampManager, SLOT(newStamp()));
    connect(mAddVariation, SIGNAL(triggered()), SLOT(addVariation()));
    connect(mDelete, SIGNAL(triggered()), SLOT(delete_()));

    mDelete->setEnabled(false);
    mAddVariation->setEnabled(false);

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setMargin(5);

    QToolBar *buttonContainer = new QToolBar;
    buttonContainer->setFloatable(false);
    buttonContainer->setMovable(false);
    buttonContainer->setIconSize(QSize(16, 16));

    buttonContainer->addAction(mNewStamp);
    buttonContainer->addAction(mAddVariation);
    buttonContainer->addAction(mDelete);

    QVBoxLayout *listAndToolBar = new QVBoxLayout;
    listAndToolBar->setSpacing(0);
    listAndToolBar->addWidget(mTileStampView);
    listAndToolBar->addWidget(buttonContainer);

    layout->addLayout(listAndToolBar);

    QItemSelectionModel *selectionModel = mTileStampView->selectionModel();
    connect(selectionModel, SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
            this, SLOT(currentRowChanged(QModelIndex)));

    setWidget(widget);
    retranslateUi();
}

void TileStampsDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void TileStampsDock::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace: {
        delete_();
        return;
    }
    }

    QDockWidget::keyPressEvent(event);
}

void TileStampsDock::currentRowChanged(const QModelIndex &index)
{
    const bool isStamp = mTileStampModel->isStamp(index);

    mDelete->setEnabled(index.isValid());
    mAddVariation->setEnabled(isStamp);

    if (isStamp) {
        emit setStamp(mTileStampModel->stampAt(index));
    } else if (const TileStampVariation *variation = mTileStampModel->variationAt(index)) {
        // single variation clicked, use it specifically
        emit setStamp(TileStamp(new Map(*variation->map)));
    }
}

void TileStampsDock::showContextMenu(QPoint pos)
{
    const QModelIndex index = mTileStampView->indexAt(pos);
    if (!index.isValid())
        return;

    QMenu menu;

    if (mTileStampModel->isStamp(index)) {
        QAction *addStampVariation = new QAction(mAddVariation->icon(),
                                                 mAddVariation->text(), &menu);
        QAction *deleteStamp = new QAction(mDelete->icon(),
                                           tr("Delete Stamp"), &menu);

        connect(deleteStamp, SIGNAL(triggered(bool)), SLOT(delete_()));
        connect(addStampVariation, SIGNAL(triggered(bool)), SLOT(addVariation()));

        menu.addAction(addStampVariation);
        menu.addSeparator();
        menu.addAction(deleteStamp);
    } else {
        QAction *removeVariation = new QAction(QIcon(QLatin1String(":/images/16x16/remove.png")),
                                               tr("Remove Variation"),
                                               &menu);

        Utils::setThemeIcon(removeVariation, "remove");

        connect(removeVariation, SIGNAL(triggered(bool)), SLOT(delete_()));

        menu.addAction(removeVariation);
    }

    menu.exec(mTileStampView->viewport()->mapToGlobal(pos));
}

void TileStampsDock::delete_()
{
    const QModelIndex index = mTileStampView->currentIndex();
    if (!index.isValid())
        return;

    mTileStampModel->removeRow(index.row(), index.parent());
}

void TileStampsDock::addVariation()
{
    const QModelIndex index = mTileStampView->currentIndex();
    if (!index.isValid())
        return;
    if (!mTileStampModel->isStamp(index))
        return;

    const TileStamp &stamp = mTileStampModel->stampAt(index);
    mTileStampManager->addVariation(stamp);
}

void TileStampsDock::retranslateUi()
{
    setWindowTitle(tr("Tile Stamps"));

    mNewStamp->setText(tr("Add New Stamp"));
    mAddVariation->setText(tr("Add Variation"));
    mDelete->setText(tr("Delete Selected"));
}


TileStampView::TileStampView(QWidget *parent)
    : QTreeView(parent)
{
}

QSize TileStampView::sizeHint() const
{
    return QSize(130, 100);
}

} // namespace Internal
} // namespace Tiled

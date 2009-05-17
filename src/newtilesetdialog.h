/*
 * Tiled Map Editor (Qt)
 * Copyright 2009 Tiled (Qt) developers (see AUTHORS file)
 *
 * This file is part of Tiled (Qt).
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
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef NEWTILESETDIALOG_H
#define NEWTILESETDIALOG_H

#include <QtGui/QDialog>

namespace Ui {
class NewTilesetDialog;
}

namespace Tiled {
namespace Internal {

class NewTilesetDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(NewTilesetDialog)

public:
    explicit NewTilesetDialog(QWidget *parent = 0);
    virtual ~NewTilesetDialog();

    // TODO: Add methods to get/set stuff

protected:
    virtual void changeEvent(QEvent *e);

private:
    Ui::NewTilesetDialog *mUi;
};

} // namespace Internal
} // namespace Tiled

#endif // NEWTILESETDIALOG_H
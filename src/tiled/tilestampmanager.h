/*
 * tilestampmanager.h
 * Copyright 2010-2011, Stefan Beller <stefanbeller@googlemail.com>
 * Copyright 2014-2015, Thorbjørn Lindeijer <bjorn@lindeijer.nl>
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

#ifndef TILESTAMPMANAGER_H
#define TILESTAMPMANAGER_H

#include <QObject>
#include <QVector>

namespace Tiled {

class Map;
class TileLayer;

namespace Internal {

class MapDocument;
class TileStamp;
class TileStampModel;
class ToolManager;

/**
 * Implements a manager which handles lots of copy&paste slots.
 * Ctrl + <1..9> will store tile layers, and just <1..9> will recall these
 * tile layers.
 */
class TileStampManager : public QObject
{
    Q_OBJECT

public:
    TileStampManager(const ToolManager &toolManager, QObject *parent = 0);
    ~TileStampManager();

    static QList<Qt::Key> quickStampKeys();

    TileStampModel *tileStampModel() const;

public slots:
    void newStamp();
    void addVariation(const TileStamp &targetStamp);

    void selectQuickStamp(int index);
    void createQuickStamp(int index);
    void extendQuickStamp(int index);

signals:
    void setStamp(const TileStamp &stamp);

private:
    Q_DISABLE_COPY(TileStampManager)

    void eraseQuickStamp(int index);
    void setQuickStamp(int index, TileStamp stamp);

    QVector<TileStamp> mQuickStamps;
    TileStampModel *mTileStampModel;

    const ToolManager &mToolManager;
};


/**
 * Returns the keys used for quickly accessible tile stamps.
 * Note: To store a tile layer <Ctrl> is added. The given keys will work
 * for recalling the stored values.
 */
inline QList<Qt::Key> TileStampManager::quickStampKeys()
{
    QList<Qt::Key> keys;
    keys << Qt::Key_1
         << Qt::Key_2
         << Qt::Key_3
         << Qt::Key_4
         << Qt::Key_5
         << Qt::Key_6
         << Qt::Key_7
         << Qt::Key_8
         << Qt::Key_9;
    return keys;
}

inline TileStampModel *TileStampManager::tileStampModel() const
{
    return mTileStampModel;
}

} // namespace Tiled::Internal
} // namespace Tiled

#endif // TILESTAMPMANAGER_H

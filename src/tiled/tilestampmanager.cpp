/*
 * tilestampmanager.cpp
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

#include "tilestampmanager.h"

#include "abstracttool.h"
#include "bucketfilltool.h"
#include "documentmanager.h"
#include "mapdocument.h"
#include "map.h"
#include "preferences.h"
#include "stampbrush.h"
#include "tilelayer.h"
#include "tileselectiontool.h"
#include "tileset.h"
#include "tilesetmanager.h"
#include "tilestamp.h"
#include "tilestampmodel.h"
#include "toolmanager.h"

#include <QDebug>
#include <QDirIterator>
#include <QJsonDocument>
#include <QSaveFile>

using namespace Tiled;
using namespace Tiled::Internal;

static QString stampFilePath(const QString &name)
{
    const Preferences *prefs = Preferences::instance();
    const QDir stampsDir(prefs->stampsDirectory());
    return stampsDir.filePath(name + (QLatin1String(".stamp")));
}

TileStampManager::TileStampManager(const ToolManager &toolManager,
                                   QObject *parent)
    : QObject(parent)
    , mQuickStamps(quickStampKeys().length())
    , mTileStampModel(new TileStampModel(this))
    , mLoadingStamps(false)
    , mToolManager(toolManager)
{
    Preferences *prefs = Preferences::instance();

    connect(prefs, &Preferences::stampsDirectoryChanged,
            this, &TileStampManager::stampsDirectoryChanged);

    connect(mTileStampModel, &TileStampModel::stampAdded,
            this, &TileStampManager::stampAdded);
    connect(mTileStampModel, &TileStampModel::stampChanged,
            this, &TileStampManager::stampChanged);
    connect(mTileStampModel, &TileStampModel::stampRemoved,
            this, &TileStampManager::deleteStamp);

    loadStamps();
}

TileStampManager::~TileStampManager()
{
    // needs to be over here where the TileStamp type is complete
}

static TileStamp stampFromContext(AbstractTool *selectedTool)
{
    TileStamp stamp;

    if (StampBrush *stampBrush = dynamic_cast<StampBrush*>(selectedTool)) {
        // take the stamp from the stamp brush
        stamp = stampBrush->stamp();
    } else if (BucketFillTool *fillTool = dynamic_cast<BucketFillTool*>(selectedTool)) {
        // take the stamp from the fill tool
        stamp = fillTool->stamp();
    } else if (MapDocument *mapDocument = DocumentManager::instance()->currentDocument()) {
        // try making a stamp from the current tile selection
        const TileLayer *tileLayer =
                dynamic_cast<TileLayer*>(mapDocument->currentLayer());
        if (!tileLayer)
            return stamp;

        QRegion selection = mapDocument->selectedArea();
        if (selection.isEmpty())
            return stamp;

        selection.translate(-tileLayer->position());
        QScopedPointer<TileLayer> copy(tileLayer->copy(selection));

        if (copy->size().isEmpty())
            return stamp;

        const Map *map = mapDocument->map();
        Map *copyMap = new Map(map->orientation(),
                               copy->width(), copy->height(),
                               map->tileWidth(), map->tileHeight());

        // Add tileset references to map
        foreach (const SharedTileset &tileset, copy->usedTilesets())
            copyMap->addTileset(tileset);

        copyMap->setRenderOrder(map->renderOrder());
        copyMap->addLayer(copy.take());

        stamp.addVariation(copyMap);
    }

    return stamp;
}

void TileStampManager::createStamp()
{
    TileStamp stamp = stampFromContext(mToolManager.selectedTool());
    if (stamp.isEmpty())
        return;

    mTileStampModel->addStamp(stamp);
}

void TileStampManager::addVariation(const TileStamp &targetStamp)
{
    TileStamp stamp = stampFromContext(mToolManager.selectedTool());
    if (stamp.isEmpty())
        return;

    if (stamp == targetStamp) // avoid easy mistake of adding duplicates
        return;

    foreach (const TileStampVariation &variation, stamp.variations())
        mTileStampModel->addVariation(targetStamp, variation);
}

void TileStampManager::selectQuickStamp(int index)
{
    const TileStamp &stamp = mQuickStamps.at(index);
    if (!stamp.isEmpty())
        emit setStamp(stamp);
}

void TileStampManager::createQuickStamp(int index)
{
    TileStamp stamp = stampFromContext(mToolManager.selectedTool());
    if (stamp.isEmpty())
        return;

    setQuickStamp(index, stamp);
}

void TileStampManager::extendQuickStamp(int index)
{
    TileStamp quickStamp = mQuickStamps[index];

    if (quickStamp.isEmpty())
        createQuickStamp(index);
    else
        addVariation(quickStamp);
}

void TileStampManager::stampsDirectoryChanged()
{
    // erase current stamps
    mQuickStamps.fill(TileStamp());
    mStampsByName.clear();
    mTileStampModel->clear();

    loadStamps();
}

void TileStampManager::eraseQuickStamp(int index)
{
    const TileStamp stamp = mQuickStamps.at(index);
    if (!stamp.isEmpty()) {
        mQuickStamps[index] = TileStamp();

        if (!mQuickStamps.contains(stamp))
            mTileStampModel->removeStamp(stamp);
    }
}

void TileStampManager::setQuickStamp(int index, TileStamp stamp)
{
    stamp.setQuickStampIndex(index);

    // make sure existing quickstamp is removed from stamp model
    eraseQuickStamp(index);

    mTileStampModel->addStamp(stamp);

    mQuickStamps[index] = stamp;
}

void TileStampManager::loadStamps()
{
    mLoadingStamps = true;

    const Preferences *prefs = Preferences::instance();
    const QString stampsDirectory = prefs->stampsDirectory();
    const QDir stampsDir(stampsDirectory);

    QDirIterator iterator(stampsDirectory,
                          QStringList() << QLatin1String("*.stamp"),
                          QDir::Files | QDir::Readable);
    while (iterator.hasNext()) {
        const QString &stampFileName = iterator.next();

        QFile stampFile(stampFileName);
        if (!stampFile.open(QIODevice::ReadOnly))
            continue;

        QByteArray data = stampFile.readAll();

        QJsonDocument document = QJsonDocument::fromBinaryData(data);
        if (document.isNull()) {
            // document not valid binary data, maybe it's an JSON text file
            QJsonParseError error;
            document = QJsonDocument::fromJson(data, &error);
            if (error.error != QJsonParseError::NoError) {
                qDebug() << "Failed to parse stamp file:" << qPrintable(error.errorString());
                continue;
            }
        }

        TileStamp stamp = TileStamp::fromJson(document.object(), stampsDir);
        if (stamp.isEmpty())
            continue;

        mTileStampModel->addStamp(stamp);

        int index = stamp.quickStampIndex();
        if (index >= 0 && index < mQuickStamps.size())
            mQuickStamps[index] = stamp;
    }

    mLoadingStamps = false;
}

void TileStampManager::stampAdded(const TileStamp &stamp)
{
    if (stamp.name().isEmpty() || mStampsByName.contains(stamp.name())) {
        // pick the first available stamp name
        const QString stampNameTemplate(QLatin1String("Stamp %1"));
        QString name;
        int index = 0;
        do {
            ++index;
            name = stampNameTemplate.arg(index);
        } while (mStampsByName.contains(name));

        const_cast<TileStamp&>(stamp).setName(name);
    }

    mStampsByName.insert(stamp.name(), stamp);

    if (!mLoadingStamps)
        saveStamp(stamp);
}

void TileStampManager::stampChanged(const TileStamp &stamp)
{
    // check for rename
    QString existingName = mStampsByName.key(stamp);
    if (existingName != stamp.name()) {
        mStampsByName.remove(existingName);
        mStampsByName.insert(stamp.name(), stamp);
        QFile::rename(stampFilePath(existingName), stampFilePath(stamp.name()));
    }

    saveStamp(stamp);
}

void TileStampManager::saveStamp(const TileStamp &stamp)
{
    // make sure we have a stamps directory
    const Preferences *prefs = Preferences::instance();
    const QString stampsDirectory(prefs->stampsDirectory());
    QDir stampsDir(stampsDirectory);

    if (!stampsDir.exists() && !stampsDir.mkpath(QLatin1String("."))) {
        qDebug() << "Failed to create stamps directory" << stampsDirectory;
        return;
    }

    QString filePath = stampFilePath(stamp.name());

    QJsonObject stampJson = stamp.toJson(QFileInfo(filePath).dir());
    QSaveFile file(filePath);
    file.open(QIODevice::WriteOnly);
    file.write(QJsonDocument(stampJson).toJson(QJsonDocument::Compact));

    if (!file.commit())
        qDebug() << "Failed to write stamp" << filePath;
}

void TileStampManager::deleteStamp(const TileStamp &stamp)
{
    mStampsByName.remove(stamp.name());
    QFile::remove(stampFilePath(stamp.name()));
}

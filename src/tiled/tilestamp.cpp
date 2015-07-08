/*
 * tilestamp.cpp
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

#include "tilestamp.h"

#include "maptovariantconverter.h"
#include "tilelayer.h"
#include "tilesetmanager.h"
#include "varianttomapconverter.h"

#include <QDebug>
#include <QJsonArray>

namespace Tiled {
namespace Internal {

class TileStampData : public QSharedData
{
public:
    TileStampData();
    TileStampData(const TileStampData &other);
    ~TileStampData();

    QString name;
    QVector<TileStampVariation> variations;
    int quickStampIndex;
};

TileStampData::TileStampData()
    : quickStampIndex(-1)
{}

TileStampData::TileStampData(const TileStampData &other)
    : QSharedData(other)
    , name(other.name)
    , variations(other.variations)
    , quickStampIndex(-1)
{
    TilesetManager *tilesetManager = TilesetManager::instance();

    // deep-copy the map data
    for (TileStampVariation &variation : variations) {
        variation.map = new Map(*variation.map);
        tilesetManager->addReferences(variation.map->tilesets());
    }
}

TileStampData::~TileStampData()
{
    TilesetManager *tilesetManager = TilesetManager::instance();

    // decrease reference to tilesets and delete maps
    for (const TileStampVariation &variation : variations) {
        tilesetManager->removeReferences(variation.map->tilesets());
        delete variation.map;
    }
}


TileStamp::TileStamp()
    : d(new TileStampData)
{
}

TileStamp::TileStamp(Map *map)
    : d(new TileStampData)
{
    addVariation(map);
}

TileStamp::TileStamp(const TileStamp &other)
    : d(other.d)
{
}

TileStamp TileStamp::operator=(const TileStamp &other)
{
    d = other.d;
    return *this;
}

bool TileStamp::operator==(const TileStamp &other) const
{
    return d == other.d;
}

TileStamp::~TileStamp()
{
    // destructor needs to be here, where TileStampData is defined
}

QString TileStamp::name() const
{
    return d->name;
}

void TileStamp::setName(const QString &name)
{
    d->name = name;
}

qreal TileStamp::probability(int index) const
{
    return d->variations.at(index).probability;
}

void TileStamp::setProbability(int index, qreal probability)
{
    d->variations[index].probability = probability;
}

const QVector<TileStampVariation> &TileStamp::variations() const
{
    return d->variations;
}

/**
 * Adds a variation \a map to this tile stamp with a given \a probability.
 *
 * The tile stamp takes ownership over the map.
 */
void TileStamp::addVariation(Map *map, qreal probability)
{
    Q_ASSERT(map);

    // increase tileset reference counts to keep them alive
    TilesetManager::instance()->addReferences(map->tilesets());

    d->variations.append(TileStampVariation(map, probability));
}

/**
 * Takes the variation map at \a index. Ownership of the map is passed to the
 * caller, who also has to make sure to handle tileset reference counting.
 */
Map *TileStamp::takeVariation(int index)
{
#if QT_VERSION >= 0x050200
    return d->variations.takeAt(index).map;
#else
    Map *variation = d->variations.at(index).map;
    d->variations.remove(index);
    return variation;
#endif
}

void TileStamp::deleteVariation(int index)
{
    Map *map = takeVariation(index);
    TilesetManager::instance()->removeReferences(map->tilesets());
    delete map;
}

/**
 * A stamp is considered empty when it has no variations.
 */
bool TileStamp::isEmpty() const
{
    return d->variations.isEmpty();
}

int TileStamp::quickStampIndex() const
{
    return d->quickStampIndex;
}

void TileStamp::setQuickStampIndex(int quickStampIndex)
{
    d->quickStampIndex = quickStampIndex;
}

Map *TileStamp::randomVariation() const
{
    if (d->variations.isEmpty())
        return 0;

    QMap<qreal, const TileStampVariation *> probabilityThresholds;
    qreal sum = 0.0;
    for (const TileStampVariation &variation : d->variations) {
        sum += variation.probability;
        probabilityThresholds.insert(sum, &variation);
    }

    qreal random = ((qreal)rand() / RAND_MAX) * sum;
    auto it = probabilityThresholds.lowerBound(random);

    const TileStampVariation *variation;
    if (it != probabilityThresholds.end())
        variation = it.value();
    else
        variation = &d->variations.last(); // floating point may have failed us

    return variation->map;
}

/**
 * Returns a new stamp where all variations have been flipped in the given
 * \a direction.
 */
TileStamp TileStamp::flipped(FlipDirection direction) const
{
    TileStamp flipped(*this);
    flipped.d.detach();

    for (const TileStampVariation &variation : flipped.variations()) {
        TileLayer *layer = static_cast<TileLayer*>(variation.map->layerAt(0));
        layer->flip(direction);
    }

    return flipped;
}

/**
 * Returns a new stamp where all variations have been rotated in the given
 * \a direction.
 */
TileStamp TileStamp::rotated(RotateDirection direction) const
{
    TileStamp rotated(*this);
    rotated.d.detach();

    for (const TileStampVariation &variation : rotated.variations()) {
        TileLayer *layer = static_cast<TileLayer*>(variation.map->layerAt(0));
        layer->rotate(direction);

        variation.map->setWidth(layer->width());
        variation.map->setHeight(layer->height());
    }

    return rotated;
}

QJsonObject TileStamp::toJson(const QDir &dir) const
{
    QJsonObject json;
    json.insert(QLatin1String("name"), d->name);

    if (d->quickStampIndex != -1)
        json.insert(QLatin1String("quickStampIndex"), d->quickStampIndex);

    QJsonArray variations;
    for (const TileStampVariation &variation : d->variations) {
        MapToVariantConverter converter;
        QVariant mapVariant = converter.toVariant(variation.map, dir);
        QJsonValue mapJson = QJsonValue::fromVariant(mapVariant);

        QJsonObject variationJson;
        variationJson.insert(QLatin1String("probability"), variation.probability);
        variationJson.insert(QLatin1String("map"), mapJson);
        variations.append(variationJson);
    }
    json.insert(QLatin1String("variations"), variations);

    return json;
}

TileStamp TileStamp::fromJson(const QJsonObject &json, const QDir &mapDir)
{
    TileStamp stamp;

    stamp.setName(json.value(QLatin1String("name")).toString());
    stamp.setQuickStampIndex(json.value(QLatin1String("quickStampIndex")).toInt(-1));

    QJsonArray variations = json.value(QLatin1String("variations")).toArray();
    for (const QJsonValue &value : variations) {
        QJsonObject variationJson = value.toObject();

        QVariant mapVariant = variationJson.value(QLatin1String("map")).toVariant();
        VariantToMapConverter converter;
        Map *map = converter.toMap(mapVariant, mapDir);
        if (!map) {
            qDebug() << "Failed to load map for stamp:" << converter.errorString();
            continue;
        }

        qreal probability = variationJson.value(QLatin1String("probability")).toDouble(1);

        stamp.addVariation(map, probability);
    }

    return stamp;
}

} // namespace Internal
} // namespace Tiled

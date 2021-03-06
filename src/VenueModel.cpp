#include "VenueModel.h"

#include "OpeningHoursAlgorithms.h"

#include <QtQml/qqml.h>
#include <QtQml/QQmlEngine>
#include <QtQml/QJSValueIterator>

#include <QDateTime>

#include <QStandardItem>

VenueModel::VenueModel(QObject *parent) :
    QStandardItemModel(parent) ,
    m_openStateUpdateTimer(this)
{
    updateOpenState();

    connect(&m_openStateUpdateTimer, &QTimer::timeout, this, &VenueModel::updateOpenState);
    m_openStateUpdateTimer.start(MICROSECONDS_PER_SECOND * SECONDS_PER_MINUTE/2);
}

VenueModel::VenueSubTypeFlag subTypeStringToFlag(const QString name)
{
    static const auto subTypeStringLookup
    = QHash<QString, VenueModel::VenueSubTypeFlag>
    {
        { "Restaurant" , VenueModel::RestaurantFlag },
        { "Imbiss"     , VenueModel::FastFoodFlag   },
        { "Cafe"       , VenueModel::CafeFlag       },
        { "Eiscafe"    , VenueModel::IceCreamFlag   }
    };

    auto const it = subTypeStringLookup.find(name);
    if (it != subTypeStringLookup.end())
    {
        return *it;
    }
    else
    {
        return VenueModel::NoneFlag;
    }
}

VenueModel::VenueSubTypeFlags extractVenueSubType(const QJSValue& from)
{
    VenueModel::VenueSubTypeFlags ret;
    if (!from.hasProperty("tags"))
    {
        return ret;
    }

    auto const tagsProperty = from.property("tags");
    if (!tagsProperty.isArray())
    {
        return ret;
    }

    for (auto const& tagVariant : tagsProperty.toVariant().toList())
    {
        if (tagVariant.canConvert<QString>())
        {
            auto const flag = subTypeStringToFlag(tagVariant.toString());
            ret |= flag;
        }
    }

    return ret;
}

QVariant extractSimplifiedSearchName(const QJSValue& stringJS)
{
    if (!stringJS.isString())
    {
        // We don't perform any proper type checking
        // for any of the values anyhow. It's JSON, after all...
        return stringJS.toVariant();
    }

    return simplifySearchString(stringJS.toString());
}

QJSValue stripWhiteSpaces(const QJSValue& stringJS)
{
    if (!stringJS.isString())
    {
        // We don't perform any proper type checking
        // for any of the values anyhow. It's JSON, after all...
        return stringJS;
    }

    return stringJS.toString().simplified();
}

QStandardItem* VenueModel::jsonItem2QStandardItem(const QJSValue& from)
{
    auto item = new QStandardItem;
    auto roleNames = this->roleNames();
    for (auto roleKey : roleNames.keys())
    {
        auto roleName = QString(roleNames[roleKey]);
        if (from.hasProperty(roleName))
        {
            auto value = from.property(roleName);
            if (   roleKey == VenueModelRoles::Name
                || roleKey == VenueModelRoles::Street)
            {
                value = stripWhiteSpaces(value);
            }

            if (roleKey == VenueModelRoles::Name)
            {
                auto const simplifiedSearchName = extractSimplifiedSearchName(value);
                item->setData(simplifiedSearchName, VenueModelRoles::SimplifiedSearchName);
            }

            if (roleKey == VenueModelRoles::Street)
            {
                auto const simplifiedSearchName = extractSimplifiedSearchName(value);
                item->setData(simplifiedSearchName, VenueModelRoles::SimplifiedSearchStreet);
            }

            if (roleKey == VenueModelRoles::Description)
            {
                auto const simplifiedSearchName = extractSimplifiedSearchName(value);
                item->setData(simplifiedSearchName, VenueModelRoles::SimplifiedSearchDescription);
            }

            if (roleKey == VenueModelRoles::DescriptionEn)
            {
                auto const simplifiedSearchName = extractSimplifiedSearchName(value);
                item->setData(simplifiedSearchName, VenueModelRoles::SimplifiedSearchDescriptionEn);
            }
            item->setData(value.toVariant(), roleKey);
        }
    }

    auto const venueSubType = extractVenueSubType(from);
    item->setData(QVariant::fromValue(static_cast<int>(venueSubType)), VenueModelRoles::VenueSubTypeRole);

    extractAndProcessOpenHoursData(*item, from);

    return item;
}


void VenueModel::importFromJson(const QJSValue &item, VenueType venueType)
{
    if (item.isArray()) {
        auto root = this->invisibleRootItem();
        QJSValueIterator array(item);

        while (array.next()) {
            if (!array.hasNext())
                break; // last value in array is an int with the length

            auto venueItem = jsonItem2QStandardItem(array.value());
            venueItem->setData(venueType, VenueModelRoles::VenueTypeRole);
            root->appendRow(venueItem);
        }
    }

    m_loadedVenueType |= VenueTypeFlag(enumValueToFlag(venueType));
    emit loadedVenueTypeChanged();
}

void VenueModel::setFavorite(const QString &id, bool favorite)
{
    auto const index = indexFromID(id);
    if (index.isValid())
    {
        setData(index, favorite, VenueModelRoles::Favorite);
    }
}

QModelIndex VenueModel::indexFromID(const QString& id) const
{
    auto const index = match(this->index(0,0), VenueModelRoles::ID, id, 1, Qt::MatchExactly);
    if (index.size() == 1)
    {
        return index[0];
    }
    else return QModelIndex();
}

QHash<int, QByteArray> VenueModel::roleNames() const
{
    static const auto roles =
    QHash<int, QByteArray>
    {
        { VenueModelRoles::ID,            "id"          },
        { VenueModelRoles::Name,          "name"        },
        { VenueModelRoles::VenueTypeRole, "venueType"   },
        { VenueModelRoles::Favorite,      "favorite"    },
        { VenueModelRoles::Street,        "street"      },
        { VenueModelRoles::Description,   "comment"     },
        { VenueModelRoles::DescriptionEn, "commentEnglish" },
        { VenueModelRoles::Website,       "website"     },
        { VenueModelRoles::Telephone,     "telephone"   },
        { VenueModelRoles::Pictures,      "pictures"    },

        // Coordinates
        { VenueModelRoles::LatCoord,      "latCoord"    },
        { VenueModelRoles::LongCoord,     "longCoord"   },

        // Properties
        { VenueModelRoles::Wlan,          "wlan" },
        { VenueModelRoles::VegCategory,   "vegan" },
        { VenueModelRoles::HandicappedAccessible,
                                          "handicappedAccessible" },
        { VenueModelRoles::HandicappedAccessibleWc,
                                          "handicappedAccessibleWc" },
        { VenueModelRoles::Catering,      "catering"     },
        { VenueModelRoles::Organic,       "organic"      },
        { VenueModelRoles::GlutenFree,    "glutenFree"   },
        { VenueModelRoles::Delivery,      "delivery"     },
        { VenueModelRoles::SeatsOutdoor,  "seatsOutdoor" },
        { VenueModelRoles::SeatsIndoor,   "seatsIndoor"  },
        { VenueModelRoles::Dog,           "dog"          },
        { VenueModelRoles::ChildChair,    "childChair"   },

        // OpeningHours
        { VenueModelRoles::CondensedOpeningHours,         "condensedOpeningHours"  },
        { VenueModelRoles::Open,          "open"   },
        { VenueModelRoles::OtMon,         "otMon"  },
        { VenueModelRoles::OtTue,         "otTue"  },
        { VenueModelRoles::OtWed,         "otWed"  },
        { VenueModelRoles::OtThu,         "otThu"  },
        { VenueModelRoles::OtFri,         "otFri"  },
        { VenueModelRoles::OtSat,         "otSat"  },
        { VenueModelRoles::OtSun,         "otSun"  }

    };

    return roles;
}

VenueModel::VenueTypeFlags VenueModel::loadedVenueType() const
{
    return m_loadedVenueType;
}

void VenueModel::updateOpenState()
{
    std::tie(m_dayOfWeek, m_currentMinute) = dayOfWeekAndCurrentMinute();
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), { VenueModelRoles::Open });
}

QVariant VenueModel::data(const QModelIndex &index, int role) const
{
    if (role == VenueModelRoles::Open)
    {
        const auto openingMinutesVar = QStandardItemModel::data(index, VenueModel::OpeningMinutes);
        if (!openingMinutesVar.isValid())
        {
            return QVariant::fromValue(false);
        }

        auto const openingMinutes = openingMinutesVar.toList();
        return isInRange(openingMinutes[m_dayOfWeek].toMap(), m_currentMinute);
    }

    return QStandardItemModel::data(index, role);
}


#include "protocolutils.h"
#include "crypto.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(ProtocolUtilsLog)
Q_LOGGING_CATEGORY(ProtocolUtilsLog, "ProtocolUtils")

QByteArray ProtocolUtils::createBindingRequest(const DeviceDescriptor &device)
{
    QJsonObject json
    {
        { "mac", device.id },
        { "t", "bind" },
        { "uid", 0 }
    };

    return QJsonDocument{ json }.toJson(QJsonDocument::Compact);
}

QByteArray ProtocolUtils::createDeviceRequest(const QByteArray &encryptedPack, int i)
{
    QJsonObject json
    {
        { "cid", "app" },
        { "i", i },
        { "t", "pack" },
        { "uid", 0 },
        { "pack", QString::fromUtf8(encryptedPack) }
    };

    return QJsonDocument{ json }.toJson(QJsonDocument::Compact);
}

QByteArray ProtocolUtils::createDeviceStatusRequestPack(const QString& id)
{
    // TODO move these keys to constants

    QJsonObject json
    {
        { "cols", QJsonArray {
                "Pow",
                "Mod",
                "SetTem",
                "WdSpd",
                "Air",
                "Blo",
                "Health",
                "SwhSlp",
                "Lig",
                "SwingLfRig",
                "SwUpDn",
                "Quiet",
                "Tur",
                "StHt",
                "TemUn",
                "HeatCoolType",
                "TemRec",
                "SvSt",
                "NoiseSet"
            }
        },
        { "mac", id },
        { "t", "status" }
    };

    return QJsonDocument{ json }.toJson(QJsonDocument::Compact);
}

bool ProtocolUtils::readPackFromResponse(const QByteArray& response,
                                         const QString& decryptionKey,
                                         QJsonObject& pack)
{
    qCDebug(ProtocolUtilsLog) << "reading pack from response:" << response;

    QJsonParseError parseError;
    auto&& responseJsonDocument = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        qCWarning(ProtocolUtilsLog) << "response is not a valid JSON object. Parse error:" << parseError.errorString();
        return false;
    }

    auto&& responseJson = responseJsonDocument.object();

    auto&& encryptedPack = responseJson["pack"].toString();
    if (encryptedPack.isEmpty())
    {
        qCWarning(ProtocolUtilsLog) << "response doesn't have a 'pack' field which is mandatory";
        return false;
    }
    qCDebug(ProtocolUtilsLog) << "Attempt decrypt with key" << decryptionKey;
    auto&& decryptedPack = Crypto::decryptPack(encryptedPack.toUtf8(), decryptionKey);
    qCDebug(ProtocolUtilsLog) << "decrypted pack:" << decryptedPack;

    auto&& packJsonDocument = QJsonDocument::fromJson(decryptedPack, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        qCWarning(ProtocolUtilsLog) << "decrypted pack is not a valid JSON object. Parse error:" << parseError.errorString();
        return false;
    }

    pack = packJsonDocument.object();

    return true;
}

ProtocolUtils::DeviceParameterMap ProtocolUtils::readStatusMapFromPack(const QJsonObject& pack)
{
    if (pack["t"] != "dat")
    {
        qCWarning(ProtocolUtilsLog) << "failed to read status map from pack, pack type mismatch:" << pack["t"];
        return{};
    }

    auto&& keys = pack["cols"];
    if (!keys.isArray())
    {
        qCWarning(ProtocolUtilsLog) << "failed to read status map from pack, 'cols' is not an array";
        return{};
    }

    auto&& keyArray = keys.toArray();
    if (keyArray.isEmpty())
    {
        qCWarning(ProtocolUtilsLog) << "failed to read status map from pack, 'cols' is empty";
        return{};
    }

    auto&& values = pack["dat"];
    if (!values.isArray())
    {
        qCWarning(ProtocolUtilsLog) << "failed to read status map from pack, 'dat' is not an array";
        return{};
    }

    auto&& valueArray = values.toArray();
    if (valueArray.isEmpty())
    {
        qCWarning(ProtocolUtilsLog) << "failed to read status map from pack, 'dat' is empty";
        return{};
    }

    if (keyArray.size() != valueArray.size())
    {
        qCWarning(ProtocolUtilsLog) << "failed to read status map from pack, 'dat' size mismatch";
        return{};
    }

    DeviceParameterMap map;
    for (int i = 0; i < keyArray.size(); i++)
        map[keyArray[i].toString()] = valueArray[i].toInt();

    return map;
}

QByteArray ProtocolUtils::createDeviceCommandPack(const ProtocolUtils::DeviceParameterMap& parameters)
{
    if (parameters.isEmpty())
        return{};

    QVariantList variantValues;
    auto&& intValues = parameters.values();

    std::transform(std::begin(intValues),
                   std::end(intValues),
                   std::back_inserter(variantValues),
                   [](int value) { return QVariant{ value }; });

    QJsonObject json
    {
        { "opt", QJsonArray::fromStringList(parameters.keys()) },
        { "p", QJsonArray::fromVariantList(variantValues) },
        { "t", "cmd" }
    };

    return QJsonDocument{ json }.toJson(QJsonDocument::Compact);
}

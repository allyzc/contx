#include "contx.h"
#include <QRegularExpression>

namespace Contx {

QVector<uint8_t> strToBytes(const QString& input)
{
    QVector<uint8_t> buffer;

    QString s = input;

    // 1. 去掉前后空白
    s = s.trimmed();

    // 2. 去掉 0x / 0X
    if (s.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        s = s.mid(2);
    }

    // 3. 移除所有非十六进制字符
    s.remove(QRegularExpression(QStringLiteral("[^0-9A-Fa-f]")));

    // 4. 奇数字符长度，前补 0
    if (s.length() & 1) {
        s.prepend(QLatin1Char('0'));
    }

    // 5. 每两个字符解析成一个字节
    buffer.reserve(s.length() / 2);

    for (int i = 0; i < s.length(); i += 2) {
        bool ok = false;
        uint8_t value = static_cast<uint8_t>(
            s.mid(i, 2).toUInt(&ok, 16)
            );
        if (ok) {
            buffer.append(value);
        }
    }

    return buffer;
}

QString bytesToStr(const uint8_t* data,
                   int len,
                   bool upper,
                   QChar sep)
{
    if (!data || len <= 0) {
        return QString();
    }

    QString result;
    result.reserve(len * 3);

    const char* fmt = upper ? "%02X" : "%02x";

    for (int i = 0; i < len; ++i) {
        result += QString::asprintf(fmt, data[i]);
        if (!sep.isNull() && i != len - 1) {
            result += sep;
        }
    }

    return result;
}

uint64_t strToHexAddress(QString s)
{
    // 1. 去掉前后空白
    s = s.trimmed();

    // 2. 去掉 0x / 0X
    if (s.startsWith("0x", Qt::CaseInsensitive)) {
        s = s.mid(2);
    }

    // 3. 移除所有非十六进制字符（空格、逗号等）
    s.remove(QRegularExpression("[^0-9A-Fa-f]"));

    // 4. 转换
    bool ok = false;
    uint64_t addr = s.toULongLong(&ok, 16);

    return ok ? addr : 0;
}

} // namespace Contx

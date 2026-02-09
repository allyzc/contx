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
    s = s.trimmed();
    if (s.isEmpty())
        return 0;

    // 1. 先检查：是否包含非法字母（G-Z / g-z）
    //    只要出现，直接判非法
    static const QRegularExpression illegalLetter(
        R"([G-Zg-z])"
        );
    if (illegalLetter.match(s).hasMatch())
        return 0;

    // 2. 去掉 0x / 0X 前缀（允许前面有空格）
    if (s.startsWith("0x", Qt::CaseInsensitive))
        s = s.mid(2);

    // 3. 移除允许修正的分隔符
    //    这里只保留 0-9 A-F a-f
    s.remove(QRegularExpression("[^0-9A-Fa-f]"));

    if (s.isEmpty())
        return 0;

    // 4. 转换
    bool ok = false;
    uint64_t addr = s.toULongLong(&ok, 16);
    return ok ? addr : 0;
}


QString formatDouble(double v, int prec)
{
    if (qFuzzyCompare(v, qRound(v)))
        return QString::number((int64_t)qRound(v));

    return QString::number(v, 'f', prec).remove(QRegExp("0+$")).remove(QRegExp("\\.$"));
}

//只把十六进制常量转成大写
QString formatHexUpper(const QString& text)
{
    QString result = text;

    // 捕获：0x + 十六进制数字
    static QRegularExpression re("(0x)([0-9a-f]+)");

    QRegularExpressionMatchIterator it = re.globalMatch(result);
    while (it.hasNext())
    {
        QRegularExpressionMatch m = it.next();

        QString prefix = m.captured(1); // "0x"
        QString digits = m.captured(2); // "7f"

        QString replaced = prefix + digits.toUpper();

        result.replace(m.capturedStart(),
                       m.capturedLength(),
                       replaced);
    }

    return result;
}

} // namespace Contx

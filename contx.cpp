#include "contx.h"
#include <QRegularExpression>
#include <limits>
#include <utility>

namespace {

constexpr int kMaxAddressExpressionLength = 256;
constexpr int kMaxAddressExpressionDepth = 16;

using ParseStatus = Contx::HexAddressParseStatus;

bool isExpressionCharacter(QChar c)
{
    return c == QLatin1Char('+') || c == QLatin1Char('-') ||
           c == QLatin1Char('*') || c == QLatin1Char('/') ||
           c == QLatin1Char('(') || c == QLatin1Char(')');
}

bool containsAddressExpression(const QString& text)
{
    for (const QChar c : text) {
        if (isExpressionCharacter(c))
            return true;
    }
    return false;
}

bool isHexDigit(QChar c, uint64_t* digit = nullptr)
{
    uint64_t value = 0;
    if (c >= QLatin1Char('0') && c <= QLatin1Char('9')) {
        value = static_cast<uint64_t>(c.unicode() - QLatin1Char('0').unicode());
    } else if (c >= QLatin1Char('A') && c <= QLatin1Char('F')) {
        value = static_cast<uint64_t>(c.unicode() - QLatin1Char('A').unicode() + 10);
    } else if (c >= QLatin1Char('a') && c <= QLatin1Char('f')) {
        value = static_cast<uint64_t>(c.unicode() - QLatin1Char('a').unicode() + 10);
    } else {
        return false;
    }

    if (digit)
        *digit = value;
    return true;
}

QString normalizeAddressExpression(const QString& text)
{
    QString normalized;
    normalized.reserve(text.size());
    for (const QChar c : text) {
        // 表达式模式仍允许常见的排版空白、逗号和下划线分组。
        if (c.isSpace() || c == QLatin1Char(',') || c == QLatin1Char('_'))
            continue;
        normalized.append(c);
    }
    return normalized;
}

class HexAddressExpressionParser
{
public:
    explicit HexAddressExpressionParser(QString expression)
        : m_expression(std::move(expression))
    {
    }

    uint64_t parse(ParseStatus* status)
    {
        uint64_t value = 0;
        if (!parseExpression(value)) {
            if (status)
                *status = m_status;
            return 0;
        }

        if (m_position != m_expression.size()) {
            setTrailingInputError();
            if (status)
                *status = m_status;
            return 0;
        }

        if (status)
            *status = ParseStatus::Success;
        return value;
    }

private:
    bool parseExpression(uint64_t& value)
    {
        if (!parseTerm(value))
            return false;

        while (m_position < m_expression.size()) {
            const QChar op = m_expression.at(m_position);
            if (op != QLatin1Char('+') && op != QLatin1Char('-'))
                break;

            ++m_position;
            uint64_t right = 0;
            if (!parseTerm(right))
                return false;

            if (op == QLatin1Char('+')) {
                if (value > std::numeric_limits<uint64_t>::max() - right)
                    return fail(ParseStatus::Overflow);
                value += right;
            } else {
                if (value < right)
                    return fail(ParseStatus::Underflow);
                value -= right;
            }
        }
        return true;
    }

    bool parseTerm(uint64_t& value)
    {
        if (!parsePrimary(value))
            return false;

        while (m_position < m_expression.size()) {
            const QChar op = m_expression.at(m_position);
            if (op != QLatin1Char('*') && op != QLatin1Char('/'))
                break;

            ++m_position;
            uint64_t right = 0;
            if (!parsePrimary(right))
                return false;

            if (op == QLatin1Char('*')) {
                if (right != 0 &&
                    value > std::numeric_limits<uint64_t>::max() / right) {
                    return fail(ParseStatus::Overflow);
                }
                value *= right;
            } else {
                if (right == 0)
                    return fail(ParseStatus::DivideByZero);
                value /= right;
            }
        }
        return true;
    }

    bool parsePrimary(uint64_t& value)
    {
        if (m_position >= m_expression.size())
            return fail(ParseStatus::InvalidSyntax);

        if (m_expression.at(m_position) == QLatin1Char('(')) {
            if (++m_depth > kMaxAddressExpressionDepth)
                return fail(ParseStatus::ExpressionTooComplex);

            ++m_position;
            if (!parseExpression(value))
                return false;
            if (m_position >= m_expression.size() ||
                m_expression.at(m_position) != QLatin1Char(')')) {
                return fail(ParseStatus::InvalidSyntax);
            }
            ++m_position;
            --m_depth;
            return true;
        }

        return parseHexNumber(value);
    }

    bool parseHexNumber(uint64_t& value)
    {
        if (m_position + 1 < m_expression.size() &&
            m_expression.at(m_position) == QLatin1Char('0') &&
            (m_expression.at(m_position + 1) == QLatin1Char('x') ||
             m_expression.at(m_position + 1) == QLatin1Char('X'))) {
            m_position += 2;
        }

        const int digitStart = m_position;
        value = 0;
        while (m_position < m_expression.size()) {
            uint64_t digit = 0;
            if (!isHexDigit(m_expression.at(m_position), &digit))
                break;

            if (value > (std::numeric_limits<uint64_t>::max() - digit) / 16)
                return fail(ParseStatus::Overflow);
            value = value * 16 + digit;
            ++m_position;
        }

        if (m_position == digitStart) {
            if (m_position < m_expression.size() &&
                !isExpressionCharacter(m_expression.at(m_position))) {
                return fail(ParseStatus::InvalidCharacter);
            }
            return fail(ParseStatus::InvalidSyntax);
        }
        return true;
    }

    void setTrailingInputError()
    {
        const QChar c = m_expression.at(m_position);
        m_status = isExpressionCharacter(c)
                ? ParseStatus::InvalidSyntax
                : ParseStatus::InvalidCharacter;
    }

    bool fail(ParseStatus status)
    {
        m_status = status;
        return false;
    }

    QString m_expression;
    int m_position = 0;
    int m_depth = 0;
    ParseStatus m_status = ParseStatus::InvalidSyntax;
};

uint64_t parseCompatibleHexAddress(QString text, ParseStatus* status)
{
    // 兼容原有的普通地址输入：仅非法字母会直接拒绝，其余分隔符会被移除。
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
        text = text.mid(2);

    static const QRegularExpression illegalLetter(QStringLiteral("[G-Zg-z]"));
    if (illegalLetter.match(text).hasMatch()) {
        if (status)
            *status = ParseStatus::InvalidCharacter;
        return 0;
    }

    text.remove(QRegularExpression(QStringLiteral("[^0-9A-Fa-f]")));
    if (text.isEmpty()) {
        if (status)
            *status = ParseStatus::InvalidSyntax;
        return 0;
    }

    bool converted = false;
    const uint64_t address = text.toULongLong(&converted, 16);
    if (status)
        *status = converted ? ParseStatus::Success : ParseStatus::Overflow;
    return converted ? address : 0;
}

} // namespace

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

uint64_t strToHexAddress(QString s, HexAddressParseStatus* status)
{
    s = s.trimmed();
    if (s.isEmpty()) {
        if (status)
            *status = HexAddressParseStatus::EmptyInput;
        return 0;
    }

    if (!containsAddressExpression(s))
        return parseCompatibleHexAddress(s, status);

    // 表达式模式不能再静默删除未知字符，否则错误输入可能变成另一有效地址。
    if (s.size() > kMaxAddressExpressionLength) {
        if (status)
            *status = HexAddressParseStatus::ExpressionTooComplex;
        return 0;
    }

    QString expression = normalizeAddressExpression(s);
    if (expression.isEmpty()) {
        if (status)
            *status = HexAddressParseStatus::InvalidSyntax;
        return 0;
    }

    return HexAddressExpressionParser(std::move(expression)).parse(status);
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

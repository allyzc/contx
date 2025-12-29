#ifndef CONTX_H
#define CONTX_H

#include <QString>
#include <QVector>
#include <cstdint>

namespace Contx {

/**
 * @brief 将十六进制字符串解析为字节数组
 *
 * 支持：
 *  - 自动去除前后空白
 *  - 自动去除 0x / 0X 前缀
 *  - 自动过滤非十六进制字符（空格、逗号、换行等）
 *  - 奇数字符长度时，自动在前面补 0
 *
 * 示例：
 *  "1234"        -> {0x12, 0x34}
 *  "0x12 34 AB"  -> {0x12, 0x34, 0xAB}
 */
QVector<uint8_t> strToBytes(const QString& s);

/**
 * @brief 将字节数组转换为十六进制字符串
 *
 * @param data   字节数据指针
 * @param len    字节长度
 * @param upper  是否使用大写字母（默认 true）
 * @param sep    字节分隔符（默认空格）
 *
 * 示例：
 *  {0x12,0x34} -> "12 34"
 */
QString bytesToStr(const uint8_t* data,
                   int len,
                   bool upper = true,
                   QChar sep = QChar('\0'));

uint64_t strToHexAddress(QString s);

} // namespace Contx

#endif // CONTX_H

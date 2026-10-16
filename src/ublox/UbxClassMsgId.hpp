/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_CLASS_MSG_ID_HPP_
#define UBX_CLASS_MSG_ID_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <utility>

#include "EUbxMsg.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::UbxClassMsgId
{

using ClassMsgId = std::pair<uint8_t, uint8_t>;

inline constexpr std::array<ClassMsgId, numberOfUbxMsgs> table = [] {
    using enum EUbxMsg;
    std::array<ClassMsgId, numberOfUbxMsgs> t{};
    t[to_underlying(UBX_ACK_ACK)] = { 0x05, 0x01 };
    t[to_underlying(UBX_ACK_NAK)] = { 0x05, 0x00 };
    t[to_underlying(UBX_CFG_CFG)] = { 0x06, 0x09 };
    t[to_underlying(UBX_CFG_GEOFENCE)] = { 0x06, 0x69 };
    t[to_underlying(UBX_CFG_MSG)] = { 0x06, 0x01 };
    t[to_underlying(UBX_CFG_NAV5)] = { 0x06, 0x24 };
    t[to_underlying(UBX_CFG_PRT)] = { 0x06, 0x00 };
    t[to_underlying(UBX_CFG_RATE)] = { 0x06, 0x08 };
    t[to_underlying(UBX_CFG_TP5)] = { 0x06, 0x31 };
    t[to_underlying(UBX_CFG_VALSET)] = { 0x06, 0x8A };
    t[to_underlying(UBX_CFG_VALGET)] = { 0x06, 0x8B };
    t[to_underlying(UBX_MON_RF)] = { 0x0A, 0x38 };
    t[to_underlying(UBX_MON_SPAN)] = { 0x0A, 0x31 };
    t[to_underlying(UBX_MON_VER)] = { 0x0A, 0x04 };
    t[to_underlying(UBX_MON_SYS)] = { 0x0A, 0x39 };
    t[to_underlying(UBX_NAV_DOP)] = { 0x01, 0x04 };
    t[to_underlying(UBX_NAV_GEOFENCE)] = { 0x01, 0x39 };
    t[to_underlying(UBX_NAV_PVT)] = { 0x01, 0x07 };
    t[to_underlying(UBX_NAV_SAT)] = { 0x01, 0x35 };
    t[to_underlying(UBX_TIM_TM2)] = { 0x0D, 0x03 };
    return t;
}();

constexpr ClassMsgId translate(const EUbxMsg& eUbxMsg)
{
    return table[to_underlying(eUbxMsg)];
}

constexpr EUbxMsg translate(const ClassMsgId& rawUbxClassMsgId)
{
    const auto it = std::find(table.begin(), table.end(), rawUbxClassMsgId);
    if (it == table.end())
        return EUbxMsg::END_UBX;
    return static_cast<EUbxMsg>(std::distance(table.begin(), it));
}

}  // JimmyPaputto::UbxClassMsgId

#endif  // UBX_CLASS_MSG_ID_HPP_

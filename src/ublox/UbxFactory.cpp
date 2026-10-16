/*
 * Jimmy Paputto 2022
 */

#include "UbxFactory.hpp"


namespace JimmyPaputto
{

UbxFactory::UbxFactory()
{
    using enum EUbxMsg;
    messages_[to_underlying(UBX_ACK_ACK)] = &ackAck_;
    messages_[to_underlying(UBX_ACK_NAK)] = &ackNak_;
    messages_[to_underlying(UBX_CFG_CFG)] = &cfgCfg_;
    messages_[to_underlying(UBX_CFG_GEOFENCE)] = &cfgGeofence_;
    messages_[to_underlying(UBX_CFG_MSG)] = &cfgMsg_;
    messages_[to_underlying(UBX_CFG_NAV5)] = &cfgNav5_;
    messages_[to_underlying(UBX_CFG_PRT)] = &cfgPrt_;
    messages_[to_underlying(UBX_CFG_RATE)] = &cfgRate_;
    messages_[to_underlying(UBX_CFG_TP5)] = &cfgTp5_;
    messages_[to_underlying(UBX_CFG_VALSET)] = &cfgValset_;
    messages_[to_underlying(UBX_CFG_VALGET)] = &cfgValget_;
    messages_[to_underlying(UBX_MON_RF)] = &monRf_;
    messages_[to_underlying(UBX_MON_SPAN)] = &monSpan_;
    messages_[to_underlying(UBX_MON_VER)] = &monVer_;
    messages_[to_underlying(UBX_MON_SYS)] = &monSys_;
    messages_[to_underlying(UBX_NAV_DOP)] = &navDop_;
    messages_[to_underlying(UBX_NAV_GEOFENCE)] = &navGeofence_;
    messages_[to_underlying(UBX_NAV_PVT)] = &navPvt_;
    messages_[to_underlying(UBX_NAV_SAT)] = &navSat_;
    messages_[to_underlying(UBX_TIM_TM2)] = &timTm2_;
}

ubxmsg::IUbxMsg& UbxFactory::create(const EUbxMsg& eUbxMsg,
    std::span<const uint8_t> frame)
{
    auto* ubxMsg = messages_[to_underlying(eUbxMsg)];
    ubxMsg->deserialize(frame);
    return *ubxMsg;
}

}  // JimmyPaputto

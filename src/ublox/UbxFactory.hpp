/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_FACTORY_HPP_
#define UBX_FACTORY_HPP_

#include <array>
#include <span>

#include "EUbxMsg.hpp"
#include "ubxmsg/IUbxMsg.hpp"
#include "ubxmsg/UBX_ACK_ACK.hpp"
#include "ubxmsg/UBX_ACK_NAK.hpp"
#include "ubxmsg/UBX_CFG_CFG.hpp"
#include "ubxmsg/UBX_CFG_GEOFENCE.hpp"
#include "ubxmsg/UBX_CFG_MSG.hpp"
#include "ubxmsg/UBX_CFG_NAV5.hpp"
#include "ubxmsg/UBX_CFG_PRT.hpp"
#include "ubxmsg/UBX_CFG_RATE.hpp"
#include "ubxmsg/UBX_CFG_TP5.hpp"
#include "ubxmsg/UBX_CFG_VALSET.hpp"
#include "ubxmsg/UBX_CFG_VALGET.hpp"
#include "ubxmsg/UBX_MON_RF.hpp"
#include "ubxmsg/UBX_MON_SPAN.hpp"
#include "ubxmsg/UBX_MON_SYS.hpp"
#include "ubxmsg/UBX_MON_VER.hpp"
#include "ubxmsg/UBX_NAV_DOP.hpp"
#include "ubxmsg/UBX_NAV_GEOFENCE.hpp"
#include "ubxmsg/UBX_NAV_PVT.hpp"
#include "ubxmsg/UBX_NAV_SAT.hpp"
#include "ubxmsg/UBX_TIM_TM2.hpp"


namespace JimmyPaputto
{

class UbxFactory final
{
public:
    explicit UbxFactory();
    UbxFactory(const UbxFactory&) = delete;
    UbxFactory& operator=(const UbxFactory&) = delete;

    ubxmsg::IUbxMsg& create(const EUbxMsg& eUbxMsg,
        std::span<const uint8_t> frame);

private:
    ubxmsg::UBX_ACK_ACK ackAck_;
    ubxmsg::UBX_ACK_NAK ackNak_;
    ubxmsg::UBX_CFG_CFG cfgCfg_;
    ubxmsg::UBX_CFG_GEOFENCE cfgGeofence_;
    ubxmsg::UBX_CFG_MSG cfgMsg_;
    ubxmsg::UBX_CFG_NAV5 cfgNav5_;
    ubxmsg::UBX_CFG_PRT cfgPrt_;
    ubxmsg::UBX_CFG_RATE cfgRate_;
    ubxmsg::UBX_CFG_TP5 cfgTp5_;
    ubxmsg::UBX_CFG_VALSET cfgValset_;
    ubxmsg::UBX_CFG_VALGET cfgValget_;
    ubxmsg::UBX_MON_RF monRf_;
    ubxmsg::UBX_MON_SPAN monSpan_;
    ubxmsg::UBX_MON_VER monVer_;
    ubxmsg::UBX_MON_SYS monSys_;
    ubxmsg::UBX_NAV_DOP navDop_;
    ubxmsg::UBX_NAV_GEOFENCE navGeofence_;
    ubxmsg::UBX_NAV_PVT navPvt_;
    ubxmsg::UBX_NAV_SAT navSat_;
    ubxmsg::UBX_TIM_TM2 timTm2_;

    std::array<ubxmsg::IUbxMsg*, numberOfUbxMsgs> messages_;
};

}  // JimmyPaputto

#endif  // UBX_FACTORY_HPP_

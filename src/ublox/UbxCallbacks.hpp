/*
 * Jimmy Paputto 2022
 */

#ifndef JP_UBX_CALLBACKS_HPP_
#define JP_UBX_CALLBACKS_HPP_

#include <array>
#include <functional>
#include <memory>
#include <utility>

#include "EUbxMsg.hpp"
#include "IUbloxConfigRegistry.hpp"
#include "ubxmsg/IUbxMsg.hpp"
#include "common/Notifier.hpp"


namespace JimmyPaputto
{

class UbxCallbacks
{
public:
    explicit UbxCallbacks(IUbloxConfigRegistry& configRegistry,
        Notifier& timeMarkNotifier);

    void run(ubxmsg::IUbxMsg& ubxMsg, const EUbxMsg& eUbxMsg);

    bool consumeNavigationEpoch()
    {
        return std::exchange(navigationEpochSeen_, false);
    }

private:
    void ackNakCb(ubxmsg::IUbxMsg& ubxMsg, EUbxMsg eUbxMsg,
        IUbloxConfigRegistry& configRegistry);

    std::array<std::function<void(ubxmsg::IUbxMsg&)>, numberOfUbxMsgs> callbacks_;
    Notifier& timeMarkNotifier_;
    bool navigationEpochSeen_ = false;
};

}  // JimmyPaputto

#endif  // JP_UBX_CALLBACKS_HPP_

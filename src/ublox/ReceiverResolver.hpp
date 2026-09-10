/*
 * Jimmy Paputto 2026
 */

#ifndef JIMMY_PAPUTTO_RECEIVER_RESOLVER_HPP_
#define JIMMY_PAPUTTO_RECEIVER_RESOLVER_HPP_

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "ublox/UsbDeviceScanner.hpp"


namespace JimmyPaputto
{

namespace HatProduct
{
// Exactly as written into the HAT EEPROM and published by the firmware in
// /proc/device-tree/hat/product.
inline constexpr std::string_view gnssL1 = "L1 GNSS HAT";
inline constexpr std::string_view gnssL1L5Time = "L1/L5 GNSS TIME HAT";
inline constexpr std::string_view gnssL1L5Rtk = "L1/L5 GNSS RTK HAT";
}  // HatProduct

enum class EReceiverTransport : uint8_t
{
    Hat,
    Usb
};

enum class EReceiverResolution : uint8_t
{
    Ok,
    NoReceiver,             // nothing on the header and nothing on USB
    UnknownHat,             // the EEPROM names a product this library cannot drive
    UsbRequestedButAbsent,  // USB was asked for and there is none
    AmbiguousUsb            // several USB receivers and no explicit path
};

struct ReceiverRequest
{
    bool useUsb = false;
    std::string usbDevicePath{};
};

struct ReceiverSelection
{
    EReceiverResolution resolution = EReceiverResolution::NoReceiver;
    EReceiverTransport transport = EReceiverTransport::Hat;
    std::string hatProduct{};
    std::string usbDevicePath{};

    // A HAT was opened while a receiver is also sitting on USB. The L1 and RTK
    // HATs expose the module's own USB port, so the two may be one receiver.
    bool usbAlsoAttached = false;

    // USB was opened although the header carries a HAT this library does not
    // recognise - worth saying out loud, since a broken EEPROM looks the same.
    bool unknownHatIgnored = false;
};

bool isSupportedHatProduct(std::string_view product);

// Picks the transport without touching hardware: the caller passes what it
// found (device-tree product string, scanned USB receivers) and gets back a
// verdict. Nothing here prints or terminates, so the whole decision table is
// testable.
ReceiverSelection resolveReceiver(const ReceiverRequest& request,
    const std::string& hatProduct,
    const std::vector<UsbGnssDevice>& usbDevices);

// Shared by the "you also have USB" warning and the fatal ambiguity message.
void printUsbCandidates(std::FILE* stream,
    const std::vector<UsbGnssDevice>& devices);

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_RECEIVER_RESOLVER_HPP_

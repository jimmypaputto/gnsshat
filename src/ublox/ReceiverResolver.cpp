/*
 * Jimmy Paputto 2026
 */

#include "ublox/ReceiverResolver.hpp"


namespace JimmyPaputto
{

bool isSupportedHatProduct(std::string_view product)
{
    return product == HatProduct::gnssL1
        || product == HatProduct::gnssL1L5Time
        || product == HatProduct::gnssL1L5Rtk;
}

ReceiverSelection resolveReceiver(const ReceiverRequest& request,
    const std::string& hatProduct,
    const std::vector<UsbGnssDevice>& usbDevices)
{
    ReceiverSelection selection;
    selection.hatProduct = hatProduct;

    // An explicit path is a direct order: it names a receiver the scan may not
    // even see, so nothing gets to override it.
    if (!request.usbDevicePath.empty())
    {
        selection.resolution = EReceiverResolution::Ok;
        selection.transport = EReceiverTransport::Usb;
        selection.usbDevicePath = request.usbDevicePath;
        return selection;
    }

    const bool hatUsable = isSupportedHatProduct(hatProduct);

    if (!request.useUsb && hatUsable)
    {
        selection.resolution = EReceiverResolution::Ok;
        selection.transport = EReceiverTransport::Hat;
        selection.usbAlsoAttached = !usbDevices.empty();
        return selection;
    }

    if (usbDevices.empty())
    {
        if (request.useUsb)
            selection.resolution = EReceiverResolution::UsbRequestedButAbsent;
        else if (hatProduct.empty())
            selection.resolution = EReceiverResolution::NoReceiver;
        else
            selection.resolution = EReceiverResolution::UnknownHat;
        return selection;
    }

    if (usbDevices.size() > 1)
    {
        selection.resolution = EReceiverResolution::AmbiguousUsb;
        return selection;
    }

    selection.resolution = EReceiverResolution::Ok;
    selection.transport = EReceiverTransport::Usb;
    selection.usbDevicePath = usbDevices.front().devicePath;
    selection.unknownHatIgnored =
        !request.useUsb && !hatProduct.empty() && !hatUsable;
    return selection;
}

void printUsbCandidates(std::FILE* stream,
    const std::vector<UsbGnssDevice>& devices)
{
    for (const auto& device : devices)
    {
        fprintf(
            stream,
            "[GNSS]   %s (%s)%s%s\r\n",
            device.devicePath.c_str(),
            device.product.c_str(),
            device.byPathAlias.empty() ? "" : " stable alias: ",
            device.byPathAlias.c_str()
        );
    }
}

}  // JimmyPaputto

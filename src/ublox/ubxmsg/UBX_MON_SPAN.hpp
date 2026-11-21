/*
 * Jimmy Paputto 2026
 */

#ifndef UBX_MON_SPAN_HPP_
#define UBX_MON_SPAN_HPP_

#include <cmath>

#include "IUbxMsg.hpp"
#include "ublox/RFBlockSpectrumData.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_MON_SPAN : public IUbxMsg
{
public:
    static constexpr std::size_t spectrumSize = 256;

    explicit UBX_MON_SPAN()
    :   rfBlocksSpectrumData_(RfBlockSpectrumData::maxNumberOfRfBlocks)
    {
        for (auto& block : rfBlocksSpectrumData_)
            block.data.reserve(spectrumSize);
    }

    std::vector<uint8_t> serialize() const override
    {
        return { };
    }

    bool deserialize(std::span<const uint8_t> serialized) override
    {
        const std::size_t length = payloadLength(serialized);
        if (length < 4)
            return false;

        const uint8_t numberOfRfBlocks = serialized[7];
        if (length < 4 + numberOfRfBlocks * 272u)
            return false;

        rfBlocksSpectrumData_.resize(numberOfRfBlocks);

        for (uint8_t i = 0; i < numberOfRfBlocks; i++)
        {
            const std::size_t offset = 10 + i * 272;
            auto& rfBlockSpectrumData = rfBlocksSpectrumData_[i];

            rfBlockSpectrumData.id = i;
            rfBlockSpectrumData.data.assign(
                serialized.begin() + offset,
                serialized.begin() + offset + spectrumSize
            );

            rfBlockSpectrumData.span = readLE<uint32_t>(serialized, offset + 256);
            rfBlockSpectrumData.resolution = readLE<uint32_t>(serialized, offset + 260);
            rfBlockSpectrumData.centerFreq = readLE<uint32_t>(serialized, offset + 264);
            rfBlockSpectrumData.gain = serialized[offset + 268];
        }
        return true;
    }

    static std::vector<uint8_t> poll()
    {
        return buildFrame({ 0xB5, 0x62, 0x0A, 0x31, 0x00, 0x00 });
    }

    const std::vector<RfBlockSpectrumData>& rfBlocksSpectrumData() const
    {
        return rfBlocksSpectrumData_;
    }

private:
    std::vector<RfBlockSpectrumData> rfBlocksSpectrumData_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_MON_SPAN_HPP_

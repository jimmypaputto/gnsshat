/*
 * Jimmy Paputto 2022
 */

#ifndef I_UBX_MSG_HPP_
#define I_UBX_MSG_HPP_

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>


namespace JimmyPaputto::ubxmsg
{

class IUbxMsg
{
public:
    virtual std::vector<uint8_t> serialize() const = 0;

    // Reads a whole frame (sync, class, id, length, payload, checksum).
    // Returns false and keeps the previous state when the payload is too
    // short for the message.
    virtual bool deserialize(std::span<const uint8_t> frame) = 0;

protected:
    static constexpr std::size_t payloadOffset = 6;

    static std::vector<uint8_t> buildFrame(std::vector<uint8_t> frame);

    // Payload length from the length field, or 0 when the frame is not
    // exactly header + payload + checksum.
    static std::size_t payloadLength(std::span<const uint8_t> frame);
};

}  // JimmyPaputto::ubxmsg

#endif  // I_UBX_MSG_HPP_

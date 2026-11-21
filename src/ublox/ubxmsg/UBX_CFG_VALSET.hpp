/*
 * Jimmy Paputto 2025
 */

#ifndef UBX_CFG_VALSET_HPP_
#define UBX_CFG_VALSET_HPP_

#include <vector>
#include <cstdint>

#include "IUbxMsg.hpp"
#include "ublox/EUbxMemoryLayer.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

/**
 * Simple configuration key-value pair for basic types
 */
struct ConfigKeyValue
{
    uint32_t key;
    std::vector<uint8_t> value;  // Raw bytes of the value
};

/**
 * UBX-CFG-VALSET message - Set configuration values
 * Used to set configuration values in the receiver's configuration database
 */
class UBX_CFG_VALSET : public IUbxMsg
{
public:
    explicit UBX_CFG_VALSET() = default;

    /**
     * Constructor for setting configuration values
     * @param version Protocol version (0x00 for current)
     * @param layer Configuration layer
     * @param cfgData Vector of configuration key-value pairs
     */
    explicit UBX_CFG_VALSET(uint8_t version, EUbxMemoryLayer layer, 
                            const std::vector<ConfigKeyValue>& cfgData)
    :   version_(version),
        layer_(layer),
        cfgData_(cfgData)
    {}

    /**
     * Create a VALSET message for setting a single uint8_t value
     */
    static std::vector<uint8_t> setU1(uint32_t key, uint8_t value, EUbxMemoryLayer layer = EUbxMemoryLayer::RAM)
    {
        ConfigKeyValue cfg = {key, {value}};
        return UBX_CFG_VALSET(0x00, layer, {cfg}).serialize();
    }

    /**
     * Create a VALSET message for setting a single uint16_t value
     */
    static std::vector<uint8_t> setU2(uint32_t key, uint16_t value, EUbxMemoryLayer layer = EUbxMemoryLayer::RAM)
    {
        ConfigKeyValue cfg = {key, serializeInt2LittleEndian(value)};
        return UBX_CFG_VALSET(0x00, layer, {cfg}).serialize();
    }

    /**
     * Create a VALSET message for setting a single uint32_t value
     */
    static std::vector<uint8_t> setU4(uint32_t key, uint32_t value, EUbxMemoryLayer layer = EUbxMemoryLayer::RAM)
    {
        ConfigKeyValue cfg = {key, serializeInt2LittleEndian(value)};
        return UBX_CFG_VALSET(0x00, layer, {cfg}).serialize();
    }

    std::vector<uint8_t> serialize() const override
    {
        // Build payload first
        std::vector<uint8_t> payload;
        payload.push_back(version_);
        payload.push_back(static_cast<uint8_t>(layer_));
        payload.push_back(0x00); // Reserved byte 1
        payload.push_back(0x00); // Reserved byte 2
        
        // Add configuration key-value pairs
        for (const auto& cfg : cfgData_) {
            // Add key using serializeInt2LittleEndian (little-endian)
            const auto keyBytes = serializeInt2LittleEndian(cfg.key);
            payload.insert(payload.end(), keyBytes.begin(), keyBytes.end());
            
            // Add value bytes
            payload.insert(payload.end(), cfg.value.begin(), cfg.value.end());
        }
        
        // Build frame: UBX header + length + payload
        std::vector<uint8_t> frame = { 0xB5, 0x62, 0x06, 0x8A };
        const auto lengthBytes = serializeInt2LittleEndian(static_cast<uint16_t>(payload.size()));
        frame.insert(frame.end(), lengthBytes.begin(), lengthBytes.end());
        frame.insert(frame.end(), payload.begin(), payload.end());

        return buildFrame(frame);
    }

    // Input-only: the receiver never sends this message.
    bool deserialize(std::span<const uint8_t>) override
    {
        return false;
    }

    // Getters
    inline uint8_t version() const { return version_; }
    inline EUbxMemoryLayer layer() const { return layer_; }
    inline const std::vector<ConfigKeyValue>& cfgData() const { return cfgData_; }

private:
    uint8_t version_{0x00};                         // Message version
    EUbxMemoryLayer layer_{EUbxMemoryLayer::RAM};   // Configuration layer
    std::vector<ConfigKeyValue> cfgData_;           // Configuration data
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_CFG_VALSET_HPP_

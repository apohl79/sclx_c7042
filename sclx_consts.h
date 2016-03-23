#ifndef SCLX_CONSTS_H_
#define SCLX_CONSTS_H_

struct sclx {
    // Operation
    static constexpr std::uint8_t OP_DRIVE = 0xff;
    static constexpr std::uint8_t OP_AUX = 0xbf;
    // Bit definitions
    static constexpr std::uint8_t BRAKE = 1 << 7;
    static constexpr std::uint8_t LANE_CHANGE = 1 << 6;
    static constexpr std::uint8_t POWER = 63;    
    static constexpr std::uint8_t LED_GREEN = 1 << 7;
    static constexpr std::uint8_t LED_RED = 1 << 6;
    static constexpr std::uint8_t LED_6 = 1 << 5;
    static constexpr std::uint8_t LED_5 = 1 << 4;
    static constexpr std::uint8_t LED_4 = 1 << 3;
    static constexpr std::uint8_t LED_3 = 1 << 2;
    static constexpr std::uint8_t LED_2 = 1 << 1;
    static constexpr std::uint8_t LED_1 = 1;
    static constexpr std::uint8_t HANDSET_6 = 1 << 6;
    static constexpr std::uint8_t HANDSET_5 = 1 << 5;
    static constexpr std::uint8_t HANDSET_4 = 1 << 4;
    static constexpr std::uint8_t HANDSET_3 = 1 << 3;
    static constexpr std::uint8_t HANDSET_2 = 1 << 2;
    static constexpr std::uint8_t HANDSET_1 = 1 << 1;
    static constexpr std::uint8_t TRACK_POWER_STATUS = 1;
    static constexpr std::uint8_t CARID_INVALID = 7;
    static constexpr std::uint8_t BTN_START = 1;
    static constexpr std::uint8_t BTN_RIGHT = 1 << 1;
    static constexpr std::uint8_t BTN_UP = 1 << 2;
    static constexpr std::uint8_t BTN_ENTER = 1 << 3;
    static constexpr std::uint8_t BTN_LEFT = 1 << 4;
    static constexpr std::uint8_t BTN_DOWN = 1 << 5;
    static constexpr std::uint8_t PB_ANALOG_DIGITAL = 1 << 7;
};

#endif  // SCLX_CONSTS_H_

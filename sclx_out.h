#ifndef SCLX_OUT_H_
#define SCLX_OUT_H_

#include <tasks/serial/term.h>

#include "sclx_consts.h"
#include "crc.h"

class sclx_out {
  public:
    struct __attribute__((__packed__)) packet_t {
        std::uint8_t op_mode;
        std::uint8_t drive[6];
        std::uint8_t led_status;
        std::uint8_t crc;
    };

    sclx_out() {
        m_data_p = reinterpret_cast<char*>(&m_packet.op_mode);
        m_size = sizeof(m_packet);
        std::memset(m_data_p, 0xff, m_size);
        m_packet.led_status = sclx::LED_GREEN | sclx::LED_RED;
        m_packet.crc = 0;
    }

    void write(tasks::serial::term& term) {
        m_packet.crc = crc8(&m_packet.op_mode, m_size - 1);
        std::streamsize bytes = term.write(m_data_p, m_size);
        if (bytes >= 0) {
            m_written += bytes;
        } else {
            throw tasks::tasks_exception(tasks::tasks_error::UNSET,
                                         "write failed: " + std::string(std::strerror(errno)), errno);
        }
    }

    bool done() { return m_written == m_size; }
    void reset() { m_written = 0; }

    inline packet_t& packet() { return m_packet; }

  private:
    packet_t m_packet;
    char* m_data_p;
    std::streamsize m_size;
    std::streamsize m_written = 0;
};

#endif  // SCLX_OUT_H_

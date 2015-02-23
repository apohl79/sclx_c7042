#ifndef SCLX_IN_H_
#define SCLX_IN_H_

#include <tasks/serial/term.h>
#include <cstring>

#include "crc.h"

class sclx_in {
  public:
    struct __attribute__((__packed__)) packet_t {
        std::uint8_t status;
        std::uint8_t handset[6];
        std::uint8_t aux_current;
        std::uint8_t carid_sf;
        std::uint32_t game_time_sf;
        std::uint8_t button_status;
        std::uint8_t crc;
    };

    sclx_in() {
        m_data_p = reinterpret_cast<char*>(&m_packet.status);
        m_size = sizeof(m_packet);
        std::memset(m_data_p, 0, m_size);
    }

    inline void read(tasks::serial::term& term) {
        std::streamsize bytes = term.read(m_data_p + m_read, m_size - m_read);
        if (bytes >= 0) {
            m_read += bytes;
        } else {
            throw tasks::tasks_exception(tasks::tasks_error::UNSET, "read failed: " + std::string(std::strerror(errno)),
                                         errno);
        }
    }

    inline bool done() { return m_read == m_size; }
    inline bool valid() const { return m_packet.crc == crc8(&m_packet.status, m_size - 1); }
    inline void reset() { m_read = 0; }

    inline packet_t& packet() { return m_packet; }

    inline std::streamsize size() const { return m_size; }

  private:
    packet_t m_packet;
    char* m_data_p;
    std::streamsize m_size = sizeof(m_packet);
    std::streamsize m_read = 0;
};

#endif  // SCLX_IN_H_

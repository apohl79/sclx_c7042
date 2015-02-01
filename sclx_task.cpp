#include <algorithm>
#include <cstring>
#include <sstream>
#include <thread>
#include <chrono>

#define _WITH_PUT_TIME
#define _WITH_SHORT_LOG
#include <tasks/logging.h>

#include <tasks/exec.h>

#include "sclx_task.h"
#include "sclx_consts.h"

#define in_cur m_in[m_in_cur]
#define in_last m_in[m_in_last]

sclx_task::sclx_task(std::string port)
    : serial_io_task(EV_WRITE),
      m_port(port),
      m_last_update(std::chrono::steady_clock::now()),
      m_game_reset(false),
      m_game_start(false) {
    term().open(m_port, B19200, tasks::serial::termmode_t::_8N1);

    // default power rate is 100%
    for (std::uint8_t i = 0; i < 6; i++) {
        m_cars[i].id = i;
        m_cars[i].power_rate = 100;
    }
    reset_game_data();
    m_game.reset = 1;
    m_game.state = game_state_t::TRAINING;
}

bool sclx_task::handle_event(tasks::worker* worker, int events) {
    bool success = true;
    try {
        if (EV_READ & events) {
            if (!m_powerbase_connected) {
                terr("powerbase connected" << std::endl);
                m_powerbase_connected = true;
                reset_game_data();
                m_game.reset = 1;
                m_game.state = game_state_t::TRAINING;
            }
            in_cur.read(term());
            if (in_cur.done()) {
                // Switch the incoming packets
                if (in_cur.valid()) {
                    // New incoming packet
                    handle_data();
                    switch_in_packets();
                }
                in_cur.reset();
                // Toggle to write mode
                set_events(EV_WRITE);
                update_watcher(worker);
            }
        } else if (EV_WRITE & events) {
            if (m_game.reset == 1) {
                m_out.packet().led_status |= sclx::LED_GREEN | sclx::LED_RED;
                m_game.reset++;
            } else if (m_game.reset == 2) {
                m_out.packet().led_status |= sclx::LED_GREEN;
                m_out.packet().led_status &= ~sclx::LED_RED;
                m_game.reset = 0;
                m_post_next_game_update = 0;
                if (m_game.state == game_state_t::STARTING) {
                    set_game_state(game_state_t::RACE);
                }
            } else {
                // if a game is running, turn on the red led
                if (m_game.state == game_state_t::RACE) {
                    m_out.packet().led_status |= sclx::LED_RED;
                    m_out.packet().led_status &= ~sclx::LED_GREEN;
                }
            }
            m_out.write(term());
            if (m_out.done()) {
                // Done writing the packet
                m_out.reset();
                // Toggle to read mode
                set_events(EV_READ);
                update_watcher(worker);
            }
        }
    } catch (tasks::tasks_exception& e) {
        terr("sclx_task: exception: " << e.what() << std::endl);
        success = false;
    }
    return success;
}

void sclx_task::handle_data() {
    m_last_update = std::chrono::steady_clock::now();

    if (!in_last.done()) {
        return;
    }

    if (m_game.state == game_state_t::BINDING) {
        auto dif = std::chrono::duration_cast<std::chrono::seconds>(m_last_update - m_bind_start).count();
        if (dif < 3) {
            m_out.packet().led_status = sclx::LED_RED | (1 << m_bind_id);
        } else {
            set_game_state(game_state_t::TRAINING);
            m_game.reset = 1;
        }
    } else {
        // always actiavte the LED for connected handsets
        update_leds();
        // check for handset updates
        update_handsets();
        // game data
        update_game();
        // powerbase button events
        update_buttons();
    }

    if (in_last.packet().aux_current != in_cur.packet().aux_current) {
        terr("aux_current changed" << std::endl);
    }
}

void sclx_task::reset_car_data() {
    for (int i = 0; i < 6; i++) {
        m_cars[i].finished = false;
        m_cars[i].game_time = 0;
        m_cars[i].best_lap_time = 0;
        m_cars[i].laps = 0;
    }
}

void sclx_task::reset_game_data() {
    reset_car_data();
    m_game.game_time = 0;
    m_game.finished_cars = 0;
    if (m_game.positions.empty()) {
        for (std::uint8_t i = 0; i < 6; i++) {
            car_ref_t carref = {i, m_cars};
            m_game.positions.push_back(carref);
        }
    } else {
        std::sort(m_game.positions.begin(), m_game.positions.end());
    }
}

void sclx_task::activate_cars(std::vector<std::uint8_t>& carids) {
    for (auto id : carids) {
        m_cars[id].active = true;
    }
}

void sclx_task::deactivate_cars() {
    for (std::uint8_t i = 0; i < 6; i++) {
        m_cars[i].active = false;
    }
}

void sclx_task::set_game_state(game_state_t state) {
    if (m_game.state != state) {
        // new state
        m_game.state = state;
        // any actions
        switch (state) {
            case game_state_t::STOPPED:
                for (int i = 0; i < 6; i++) {
                    set_power(i, 0);
                    set_lane_change(i, false);
                }
                break;
            case game_state_t::RACE:
                break;
            case game_state_t::COUNTDOWN:
            case game_state_t::STARTING:
            case game_state_t::TRAINING:
            case game_state_t::BINDING:
                reset_game_data();
                break;
        }
        // inform handler
        tasks::exec([this, state] { m_on_state_func(state); });
    }
}

void sclx_task::cycle_reset(tasks::worker* worker) {
    if (m_powerbase_connected) {
        terr("powerbase disconnected" << std::endl);
        m_powerbase_connected = false;
    }
    term().close();
    term().open(m_port, B19200, tasks::serial::termmode_t::_8N1);
    in_cur.reset();
    m_out.reset();
    set_events(EV_WRITE);
    update_watcher(worker);
    m_last_update = std::chrono::steady_clock::now();
}

void sclx_task::set_drive_data(std::uint8_t carid, bool enable, std::uint8_t bit) {
    if (carid < 6) {
        std::uint8_t drive = ~m_out.packet().drive[carid];
        if (enable) {
            drive |= bit;
        } else {
            drive &= ~bit;
        }
        m_out.packet().drive[carid] = ~drive;
    } else {
        throw tasks::tasks_exception(std::string("set_drive_data: invalid carid ") +
                                     std::to_string(static_cast<int>(carid)));
    }
}

void sclx_task::set_brake(std::uint8_t carid, bool enable) {
    set_drive_data(carid, enable, sclx::BRAKE);
}

void sclx_task::set_lane_change(std::uint8_t carid, bool enable) {
    set_drive_data(carid, enable, sclx::LANE_CHANGE);
}

void sclx_task::set_power(std::uint8_t carid, std::uint8_t power) {
    if (carid < 6) {
        if (power <= sclx::POWER) {
            if (m_cars[carid].power_rate < 100) {
                // reduce power
                power = power * m_cars[carid].power_rate / 100;
            }
            std::uint8_t drive = ~m_out.packet().drive[carid];
            drive &= ~sclx::POWER;
            drive += power;
            m_out.packet().drive[carid] = ~drive;
        } else {
            throw tasks::tasks_exception(std::string("set_power: invalid power value ") +
                                         std::to_string(static_cast<int>(power)));
        }
    } else {
        throw tasks::tasks_exception(std::string("set_power: invalid carid ") +
                                     std::to_string(static_cast<int>(carid)));
    }
}

void sclx_task::set_power_rate(std::uint8_t carid, std::uint8_t percentage) {
    if (carid < 6 && percentage > 0 && percentage <= 100) {
        m_cars[carid].power_rate = percentage;
    } else {
        throw tasks::tasks_exception("set_power_rate: invalid input data: carid=" + std::to_string((int)carid) +
                                     " percentage=" + std::to_string((int) percentage));
    }
}

void sclx_task::set_leds(std::uint8_t leds) {
    m_out.packet().led_status = leds;
}

void sclx_task::update_leds() {
    std::uint8_t status = in_cur.packet().status;
    std::uint8_t leds = 0;
    bool connected = false;
    if (sclx::HANDSET_1 & status) {
        leds |= sclx::LED_1;
        connected = true;
    }
    update_ctrl_connected(0, connected);
    connected = false;
    if (sclx::HANDSET_2 & status) {
        leds |= sclx::LED_2;
        connected = true;
    }
    update_ctrl_connected(1, connected);
    connected = false;
    if (sclx::HANDSET_3 & status) {
        leds |= sclx::LED_3;
        connected = true;
    }
    update_ctrl_connected(2, connected);
    connected = false;
    if (sclx::HANDSET_4 & status) {
        leds |= sclx::LED_4;
        connected = true;
    }
    update_ctrl_connected(3, connected);
    connected = false;
    if (sclx::HANDSET_5 & status) {
        leds |= sclx::LED_5;
        connected = true;
    }
    update_ctrl_connected(4, connected);
    connected = false;
    if (sclx::HANDSET_6 & status) {
        leds |= sclx::LED_6;
        connected = true;
    }
    update_ctrl_connected(5, connected);
    m_out.packet().led_status = leds;
}

void sclx_task::update_ctrl_connected(std::uint8_t id, bool connected) {
    if (m_ctrl_connected[id] != connected) {
        m_ctrl_connected[id] = connected;
        m_on_controller_func(id, connected);
    }
}

void sclx_task::update_handsets() {
    if (m_game.state != game_state_t::STOPPED) {
        for (int i = 0; i < 6; i++) {
            // apply the power/brake/lane change settings from the handsets
            if (m_game.state == game_state_t::TRAINING || (m_cars[i].active && !m_cars[i].finished)) {
                std::uint8_t cur = ~in_cur.packet().handset[i];
                std::uint8_t last = ~in_last.packet().handset[i];
                if (cur != last) {
                    if ((sclx::BRAKE & cur) != (sclx::BRAKE & last)) {
                        tdbg("handset" << i << ": brake " << (sclx::BRAKE & cur ? "on" : "off") << std::endl);
                        set_brake(i, sclx::BRAKE & cur);
                    }
                    if ((sclx::LANE_CHANGE & cur) != (sclx::LANE_CHANGE & last)) {
                        tdbg("handset" << i << ": lane change " << (sclx::LANE_CHANGE & cur ? "on" : "off")
                                       << std::endl);
                        set_lane_change(i, sclx::LANE_CHANGE & cur);
                    }
                    if ((sclx::POWER & cur) != (sclx::POWER & last)) {
                        tdbg("handset" << i << ": power " << static_cast<int>(sclx::POWER & cur) << std::endl);
                        set_power(i, sclx::POWER & cur);
                    }
                }
            } else if (m_cars[i].active && m_cars[i].finished) {
                std::uint8_t power = ~m_out.packet().drive[i] & sclx::POWER;
                if (power > 0) {
                    // after the finish line we want the car to go for some time
                    std::uint64_t dif = m_game.game_time - m_cars[i].game_time;
                    if (dif < 1500000) {
                        set_power(i, 40);
                    } else {
                        set_power(i, 0);
                    }
                }
            }
        }
    }
}

void sclx_task::update_game() {
    std::uint8_t carid = (sclx::CARID_INVALID & in_cur.packet().carid_sf);  // just look at the last 3 bits
    std::uint64_t time = 6.4 * in_cur.packet().game_time_sf;
    if (in_last.packet().game_time_sf != in_cur.packet().game_time_sf) {
        // game timer
        m_game.game_time = time;
        if (carid > 0 && carid < 7) {
            carid--;  // we work with 0 based indexes
            if (m_game.state == game_state_t::RACE || m_game.state == game_state_t::TRAINING) {
                // a car crossed the start/finish line
                car_data_t& car = m_cars[carid];
                std::stringstream sout;
                // lap time
                std::uint64_t lap_time = time - car.game_time;
                car.game_time = time;
                if (car.laps > 0) {
                    bool record = false;
                    if (car.best_lap_time == 0 || lap_time < car.best_lap_time) {
                        // new record
                        m_cars[carid].best_lap_time = lap_time;
                        record = true;
                    }
                    std::uint8_t laps = car.laps;
                    tasks::exec(
                        [this, carid, laps, lap_time, record] { m_on_lap_func(carid, laps, lap_time, record); });
                } else {
                    car.start_time = time;
                }
                if (m_game.state == game_state_t::RACE && car.laps == m_game.laps) {
                    // finished
                    car.finished = true;
                    m_game.finished_cars++;
                    if (m_game.finished_cars == m_game.active_cars) {
                        // all cars passed the finish line, game finished
                        tasks::exec([this, time] {
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                            post_game_update(true, time);
                        });
                    }
                } else {
                    // next laps
                    car.laps++;
                }
            } else if (m_game.state == game_state_t::STARTING || m_game.state == game_state_t::COUNTDOWN) {
                // false start
                tasks::exec([this, carid] { m_on_false_start_func(carid); });
                set_game_state(game_state_t::STOPPED);
            }
        }
        if (time > m_post_next_game_update) {
            // send a game update every second
            tasks::exec([this, time] { post_game_update(false, time); });
            m_post_next_game_update = time + 1000000;
        }
    }
}

void sclx_task::update_buttons() {
    if (in_last.packet().button_status != in_cur.packet().button_status) {
        std::uint8_t btn = ~in_cur.packet().button_status;
        tdbg("update_buttons: btn=0x" << std::hex << static_cast<int>(btn) << std::dec << std::endl);
        if (sclx::BTN_START & btn) {
            tasks::exec([this] { m_on_button_func(sclx::BTN_START); });
        } else if (sclx::BTN_RIGHT & btn) {
            tasks::exec([this] { m_on_button_func(sclx::BTN_RIGHT); });
        } else if (sclx::BTN_UP & btn) {
            tasks::exec([this] { m_on_button_func(sclx::BTN_UP); });
        } else if (sclx::BTN_ENTER & btn) {
            tasks::exec([this] { m_on_button_func(sclx::BTN_ENTER); });
        } else if (sclx::BTN_LEFT & btn) {
            tasks::exec([this] { m_on_button_func(sclx::BTN_LEFT); });
        } else if (sclx::BTN_DOWN & btn) {
            tasks::exec([this] { m_on_button_func(sclx::BTN_DOWN); });
        }
    }
}

void sclx_task::post_game_update(bool finished, std::uint64_t game_time) {
    std::sort(m_game.positions.begin(), m_game.positions.end());

    std::vector<std::uint8_t> positions;

    for (auto& carref : m_game.positions) {
        bool active = m_game.state == game_state_t::TRAINING;
        if (!active) {
            auto& car = m_cars[carref.id];
            active = car.active;
        }
        if (active) {
            positions.push_back(carref.id);
        }
    }

    if (finished) {
        // stop the game
        set_game_state(game_state_t::STOPPED);
        // call the event handler
        m_on_game_finished_func(game_time, positions);
    } else {
        // call the event handler
        m_on_game_update_func(game_time, positions);
    }
}

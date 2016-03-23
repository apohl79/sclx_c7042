#ifndef SCLX_TASK_H_
#define SCLX_TASK_H_

#include <tasks/serial_io_task.h>
#include <tasks/worker.h>

#include <chrono>
#include <atomic>
#include <functional>
#include <vector>

#include "sclx_in.h"
#include "sclx_out.h"

class sclx_task : public tasks::serial_io_task {
  public:
    enum class game_state_t : std::uint8_t { STOPPED, COUNTDOWN, RACE, STARTING, TRAINING, BINDING };

    // car gaming data
    struct car_data_t {
        std::uint8_t id;        
        bool active;
        bool finished;
        std::uint8_t power_rate;        
        std::uint64_t start_time;
        std::uint64_t game_time;
        std::uint32_t best_lap_time;
        std::uint8_t laps;
    };

    sclx_task(std::string port);
    bool handle_event(tasks::worker* worker, int events);

    void set_leds(std::uint8_t leds);
    void set_brake(std::uint8_t carid, bool enable);
    void set_lane_change(std::uint8_t carid, bool enable);
    void set_power(std::uint8_t carid, std::uint8_t power);
    void set_power_rate(std::uint8_t carid, std::uint8_t percentage);

    // if we don't get data for some time, the task gest reset
    void cycle_reset(tasks::worker* worker);

    void game_stop() {
        set_game_state(game_state_t::STOPPED);
        m_game.reset = 1;
    }
    
    void game_init(uint8_t laps, std::vector<std::uint8_t> carids) {
        if (carids.size() < 1) {
            throw tasks::tasks_exception(tasks::tasks_error::UNSET,
                                         "game_start: you need at least 1 car to start a game");
        }
        m_game.laps = laps;
        m_game.active_cars = carids.size();
        deactivate_cars();
        activate_cars(carids);
        set_game_state(game_state_t::COUNTDOWN);
        m_game.reset = 1;
    }

    void game_start() {
        if (m_game.state == game_state_t::COUNTDOWN) {
            set_game_state(game_state_t::STARTING);
            m_game.reset = 1;
        }
    }

    void bind_car(std::uint8_t id) {
        set_game_state(game_state_t::STOPPED);
        set_game_state(game_state_t::BINDING);
        m_game.reset = 1;
        m_bind_id = id;
        m_bind_start = std::chrono::steady_clock::now();
    }

    void training() {
        set_game_state(game_state_t::TRAINING);
        m_game.reset = 1;
    }

    game_state_t game_state() {
        return m_game.state;
    }
    
    bool car_mode_digital() const {
        return m_digital_car_mode;
    }

    void set_digital_car_mode(bool digital = true) {
        if (digital != m_digital_car_mode) {
            m_digital_car_mode = digital;
            m_update_car_mode = true;
        }
    }

    inline std::chrono::steady_clock::time_point last_update() const {
        return m_last_update;
    }

    // event handlers
    typedef std::function<void(std::uint8_t btn)> button_func_t;
    void on_button_press(button_func_t f) {
        m_on_button_func = f;
    }

    typedef std::function<void(game_state_t state)> state_func_t;
    void on_game_state_change(state_func_t f) {
        m_on_state_func = f;
    }

    typedef std::function<void(std::uint8_t carid, std::uint8_t lap, std::uint64_t lap_time, bool record)> lap_func_t;
    void on_lap_count(lap_func_t f) {
        m_on_lap_func = f;
    }

    typedef std::function<void(std::uint8_t carid)> false_start_func_t;
    void on_false_start(false_start_func_t f) {
        m_on_false_start_func = f;
    }

    typedef std::function<void(std::uint64_t game_time, std::vector<std::uint8_t>& positions)> game_update_func_t;
    void on_game_finished(game_update_func_t f) {
        m_on_game_finished_func = f;
    }
    void on_game_update(game_update_func_t f) {
        m_on_game_update_func = f;
    }

    typedef std::function<void(std::uint8_t id, bool connected)> controller_func_t;    
    void on_controller_change(controller_func_t f) {
        m_on_controller_func = f;
    }

  private:
    std::string m_port;
    bool m_powerbase_connected = false;

    // used to order cars by position
    struct car_ref_t {
        std::uint8_t id;
        car_data_t* cars;

        bool operator<(const car_ref_t& a) const {
            car_data_t& car1 = cars[id];
            car_data_t& car2 = cars[a.id];
            if (car1.laps == car2.laps) {
                if (car1.game_time == car2.game_time) {
                    return id < a.id;
                } else {
                    return car1.game_time < car2.game_time;
                }
            } else {
                return car1.laps > car2.laps;
            }
        }
    };

    // game data
    struct game_data_t {
        std::atomic<game_state_t> state;
        std::atomic<std::uint8_t> reset;
        std::uint8_t laps;
        std::uint64_t game_time;        
        std::uint8_t active_cars;
        std::uint8_t finished_cars;
        std::vector<car_ref_t> positions;
    };

    // two incoming packets to detect deltas
    sclx_in m_in[2];
    sclx_out m_out;
    int m_in_cur = 0;
    int m_in_last = 1;
    std::chrono::steady_clock::time_point m_last_update;
    std::uint64_t m_post_next_game_update = 0;

    std::atomic<bool> m_game_reset;
    std::atomic<bool> m_game_start;

    game_data_t m_game;
    car_data_t m_cars[6];
    bool m_ctrl_connected[6] = {false, false, false, false, false, false};

    std::uint8_t m_bind_id = 6;
    std::chrono::steady_clock::time_point m_bind_start;

    bool m_digital_car_mode = true;
    std::atomic<bool> m_update_car_mode;

    // event handlers
    button_func_t m_on_button_func = [](std::uint8_t) {};
    state_func_t m_on_state_func = [](game_state_t) {};
    lap_func_t m_on_lap_func = [](std::uint8_t, std::uint8_t, std::uint64_t, bool) {};
    false_start_func_t m_on_false_start_func = [](std::uint8_t) {};
    game_update_func_t m_on_game_finished_func = [](std::uint64_t, std::vector<std::uint8_t>&) {};
    game_update_func_t m_on_game_update_func = [](std::uint64_t, std::vector<std::uint8_t>&) {};
    controller_func_t m_on_controller_func = [] (std::uint8_t, bool) {};

    void init_term();

    inline void switch_in_packets() {
        if (m_in_cur) {
            m_in_cur = 0;
            m_in_last = 1;
        } else {
            m_in_cur = 1;
            m_in_last = 0;
        }
    }

    void reset_car_data();
    void reset_game_data();
    void activate_cars(std::vector<std::uint8_t>& carids);
    void deactivate_cars();

    void set_game_state(game_state_t state);

    void handle_data();
    void set_drive_data(std::uint8_t carid, bool enable, std::uint8_t bit);
    
    void update_leds();
    void update_handsets();
    void update_game();
    void update_buttons();
    void update_ctrl_connected(std::uint8_t id, bool connected);

    void post_game_update(bool finished, std::uint64_t game_time);
};

#endif  // SCLX_TASK_H_

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <thread>
#include <vector>
#include <unordered_map>

//#define _WITH_PUT_TIME
#define _WITH_SHORT_LOG
#include <tasks/logging.h>

#include <tasks/serial/term.h>
#include <tasks/exec.h>

#include <json/json.h>

#include "sclx_task.h"
#include "sclx_cycle_task.h"

#include "websocket/server_ws.hpp"

sclx_task* sclx;
int laps = 3;
std::atomic<bool> starting;
SimpleWeb::SocketServer<SimpleWeb::WS> sclx_ws(8383, 2);
std::mutex mtx_ws;
std::string settings_path("settings.json");

using connection_ptr_t = std::shared_ptr<SimpleWeb::SocketServerBase<SimpleWeb::WS>::Connection>;
using message_ptr_t = std::shared_ptr<SimpleWeb::SocketServerBase<SimpleWeb::WS>::Message>;

struct driver_t {
    int id;
    std::string name;
    std::uint8_t power;
    std::string image;
    driver_t() : id(0), power(100) {
    }
};

struct controller_t {
    std::uint8_t driver;
    bool connected;
    controller_t() : driver(0), connected(false) {
    }
};

std::map<int, driver_t> driver_map;
controller_t controllers[6];

void load_settings() {
    std::ifstream file;
    file.open(settings_path);
    if (file.good()) {
        Json::Reader reader;
        Json::Value root;
        if (reader.parse(file, root, false)) {
            Json::Value tmp = root["drivers"];
            for (Json::ArrayIndex i = 0; i < tmp.size(); i++) {
                driver_t driver;
                driver.id = tmp[i]["id"].asInt();
                driver.name = tmp[i]["name"].asString();
                driver.power = tmp[i]["power"].asInt();
                driver.image = tmp[i]["image"].asString();
                driver_map[driver.id] = driver;
            }
            tmp = root["controllers"];
            for (Json::ArrayIndex i = 0; i < tmp.size(); i++) {
                int id = tmp[i]["id"].asInt();
                int driverid = tmp[i]["driver"].asInt();
                controllers[id].driver = driverid;
                sclx->set_power_rate(id, driver_map[driverid].power);
            }
            if (root.isMember("laps")) {
                laps = root["laps"].asInt();
            }
            terr("loaded settings from " << settings_path << std::endl);
        }
    }
}

void save_settings() {
    Json::Value root;
    root["type"] = "settings";
    for (auto& driver : driver_map) {
        Json::Value drv;
        drv["id"] = driver.second.id;
        drv["name"] = driver.second.name;
        drv["power"] = driver.second.power;
        drv["image"] = driver.second.image;
        root["drivers"].append(drv);
    }
    for (int i = 0; i < 6; i++) {
        Json::Value ctrl;
        ctrl["id"] = i;
        ctrl["driver"] = controllers[i].driver;
        root["controllers"].append(ctrl);
    }
    root["laps"] = laps;
    std::ofstream file;
    file.open(settings_path);
    if (file.good()) {
        Json::StyledStreamWriter writer;
        writer.write(file, root);
        terr("wrote settings to " << settings_path << std::endl);
    }
}

void write_json_to_ws(Json::Value& root, connection_ptr_t conn = nullptr) {
    Json::FastWriter writer;
    std::stringstream out;
    out << writer.write(root);
    if (nullptr != conn) {
        sclx_ws.send(conn, out);
    } else {
        std::lock_guard<std::mutex> lock(mtx_ws);
        for (auto c : sclx_ws.get_connections()) {
            sclx_ws.send(c, out);
        }
    }
}

void button_press(std::uint8_t btn) {
    switch (btn) {
        case sclx::BTN_START:
            if (!starting) {
                starting = true;
                if (sclx->game_state() == sclx_task::game_state_t::TRAINING) {
                    // select all connected controllers for a new race
                    std::vector<std::uint8_t> carids;
                    for (std::uint8_t i = 0; i < 6; i++) {
                        if (controllers[i].connected) {
                            carids.push_back(i);
                        }
                    }
                    // init the race, false starts are now possible
                    terr("new game - " << laps << " laps, " << carids.size() << " cars" << std::endl);
                    sclx->game_init(laps, carids);

                    // send countdown messages to the web app to show the race lights
                    Json::Value root;
                    root["type"] = "countdown";
                    root["number"] = 4;
                    write_json_to_ws(root);
                    sleep(11);
                    bool error = false;
                    for (int i = 3; i > 1; i--) {
                        root["number"] = i;
                        write_json_to_ws(root);
                        sleep(2);
                        if (sclx->game_state() != sclx_task::game_state_t::COUNTDOWN) {
                            // false start
                            error = true;
                            break;
                        }
                    }

                    // start the race
                    if (!error) {
                        root["number"] = 1;
                        write_json_to_ws(root);
                        sclx->game_start();
                        sleep(2);
                    }

                    // hide the race lights in the web app
                    root["number"] = 0;
                    write_json_to_ws(root);
                } else {
                    sclx->training();
                }
                starting = false;
            }
            break;
        case sclx::BTN_UP: {
            laps++;
            // update the UI
            Json::Value root;
            root["type"] = "laps_update";
            root["laps"] = laps;
            write_json_to_ws(root);
            // save it
            save_settings();
            break;
        }
        case sclx::BTN_DOWN: {
            if (laps > 1) {
                laps--;
                // update the UI
                Json::Value root;
                root["type"] = "laps_update";
                root["laps"] = laps;
                write_json_to_ws(root);
                // save it
                save_settings();
            }
            break;
        }
    }
}

void lap_count(std::uint8_t carid, std::uint8_t lap, std::uint64_t lap_time, bool record) {
    Json::Value root;
    root["type"] = "lap_count";
    root["id"] = carid;
    root["lap"] = lap;
    root["lap_time"] = lap_time;
    root["record"] = record;
    write_json_to_ws(root);
}

void false_start(std::uint64_t carid) {
    Json::Value root;
    root["type"] = "false_start";
    root["id"] = carid;
    write_json_to_ws(root);
}

void game_finished(std::uint64_t game_time, std::vector<std::uint8_t>& positions) {
    terr("game finished, laps: " << (int)laps << "  game time: " << ((double)game_time) / 1000000 << std::endl);
    int pos = 1;
    for (auto car : positions) {
        terr("(" << pos++ << ") car " << (int)car << std::endl);
    }
    Json::Value root;
    root["type"] = "game_finished";
    root["time"] = game_time;
    Json::Value pos_arr(Json::arrayValue);
    for (auto p : positions) {
        pos_arr.append(p);
    }
    root["positions"] = pos_arr;
    write_json_to_ws(root);
}

void game_update(std::uint64_t game_time, std::vector<std::uint8_t>& positions) {
    Json::Value root;
    root["type"] = "game_update";
    root["time"] = game_time;
    Json::Value pos_arr(Json::arrayValue);
    for (auto p : positions) {
        pos_arr.append(p);
    }
    root["positions"] = pos_arr;
    write_json_to_ws(root);
}

std::string game_state_to_string(sclx_task::game_state_t state) {
    switch (state) {
        case sclx_task::game_state_t::STOPPED:
            return "STOPPED";
            break;
        case sclx_task::game_state_t::COUNTDOWN:
            return "COUNTDOWN";
            break;
        case sclx_task::game_state_t::RACE:
            return "RACE";
            break;
        case sclx_task::game_state_t::STARTING:
            return "STARTING";
            break;
        case sclx_task::game_state_t::TRAINING:
            return "TRAINING";
            break;
        case sclx_task::game_state_t::BINDING:
            return "BINDING";
            break;
    }
    return "";
}

void game_state_change(sclx_task::game_state_t state) {
    Json::Value root;
    root["type"] = "game_state";
    root["state"] = game_state_to_string(state);
    write_json_to_ws(root);
}

void controller_change(std::uint8_t id, bool connected) {
    controllers[id].connected = connected;
    Json::Value root;
    root["type"] = "controller_changed";
    root["id"] = id;
    root["connected"] = connected;
    write_json_to_ws(root);
}

void handle_message(connection_ptr_t conn, message_ptr_t msg) {
    std::stringstream data;
    msg->data >> data.rdbuf();

    Json::Reader reader;
    Json::Value root;
    if (reader.parse(data, root, false)) {
        //Json::StyledStreamWriter writer;
        //writer.write(std::cout, root);
        if (root.isMember("type")) {
            if (root["type"].asString() == "settings") {
                // settings update
                driver_map.clear();
                Json::Value tmp = root["drivers"];
                for (Json::ArrayIndex i = 0; i < tmp.size(); i++) {
                    driver_t driver;
                    driver.id = tmp[i]["id"].asInt();
                    driver.name = tmp[i]["name"].asString();
                    driver.power = tmp[i]["power"].asInt();
                    driver.image = tmp[i]["image"].asString();
                    driver_map[driver.id] = driver;
                }
                tmp = root["controllers"];
                for (Json::ArrayIndex i = 0; i < tmp.size(); i++) {
                    int id = tmp[i]["id"].asInt();
                    Json::Value v = tmp[i]["driver"];
                    if (v.isString()) {
                        controllers[id].driver = std::stoul(v.asString());
                    } else {
                        controllers[id].driver = v.asInt();
                    }
                    sclx->set_power_rate(id, driver_map[controllers[id].driver].power);
                }
                save_settings();
            } else if (root["type"].asString() == "bind_car") {
                std::uint8_t id = root["id"].asInt();
                sclx->bind_car(id);
            } else if (root["type"].asString() == "play_sound") {
                std::string cmd = "/opt/sclx_c7042/rpi_sound.sh /opt/sclx_c7042/webui/";
                cmd += root["file"].asString();
                tasks::exec([cmd] { std::system(cmd.c_str()); });
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <serial device>" << std::endl;
        return 1;
    }

    // create an endpoint
    auto& ws = sclx_ws.endpoint["^/sclx/?$"];

    try {
        tasks::dispatcher::init_workers(1);
        auto disp = tasks::dispatcher::instance();
        disp->start();

        starting = false;
        
        sclx = new sclx_task(argv[1]);
        sclx->on_finish([disp] { disp->terminate(); });
        sclx->on_button_press(button_press);
        sclx->on_lap_count(lap_count);
        sclx->on_false_start(false_start);
        sclx->on_game_finished(game_finished);
        sclx->on_game_update(game_update);
        sclx->on_game_state_change(game_state_change);
        sclx->on_controller_change(controller_change);
        disp->add_task(sclx);
        sclx_cycle_task* cycle = new sclx_cycle_task(sclx, 1.);
        disp->add_task(cycle);

        load_settings();

        // send the game state on a new connection
        ws.onopen = [](connection_ptr_t conn) {
            terr("new client, sending settings and game state" << std::endl);
            Json::Value root;
            root["type"] = "settings";
            for (auto& driver : driver_map) {
                Json::Value drv;
                drv["id"] = driver.second.id;
                drv["name"] = driver.second.name;
                drv["power"] = driver.second.power;
                drv["image"] = driver.second.image;
                root["drivers"].append(drv);
            }
            for (int i = 0; i < 6; i++) {
                Json::Value ctrl;
                ctrl["id"] = i;
                ctrl["driver"] = controllers[i].driver;
                ctrl["connected"] = controllers[i].connected;
                root["controllers"].append(ctrl);
            }
            write_json_to_ws(root, conn);
            root["type"] = "game_state";
            root["state"] = game_state_to_string(sclx->game_state());
            write_json_to_ws(root, conn);
            root["type"] = "laps_update";
            root["laps"] = laps;
            write_json_to_ws(root, conn);
        };
        ws.onmessage = handle_message;
        tasks::exec([] { sclx_ws.start(); });

        disp->join();
    } catch (tasks::tasks_exception& e) {
        terr("error: " << e.what() << std::endl);
    }

    return 0;
}

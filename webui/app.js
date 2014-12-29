var app = angular.module('sclx', ['ngModal', 'anguFixedHeaderTable', 'ngAudio']);
var game = {
    state: 'Verbindungsfehler',
    time: 0,
    laps: 0,
    countdown_number: 0,
    countdown_images: [
        'images/countdown_1.png',
        'images/countdown_2.png',
        'images/countdown_3.png',
        'images/countdown_4.png'
    ],
    show_game_finished: false,
    show_false_start: false,
    false_start_id: 0,
    show_settings: false,
    positions: [],
    cars: [
        { id: 0, laps: 0, last_time: 0, best_time: 0 },
        { id: 1, laps: 0, last_time: 0, best_time: 0 },
        { id: 2, laps: 0, last_time: 0, best_time: 0 },
        { id: 3, laps: 0, last_time: 0, best_time: 0 },
        { id: 4, laps: 0, last_time: 0, best_time: 0 },
        { id: 5, laps: 0, last_time: 0, best_time: 0 },
    ],
    controllers: [
        { id: 0, driver: 0, connected: false },
        { id: 1, driver: 0, connected: false },
        { id: 2, driver: 0, connected: false },
        { id: 3, driver: 0, connected: false },
        { id: 4, driver: 0, connected: false },
        { id: 5, driver: 0, connected: false },
    ],
    drivers: [
        { id: 0, name: "Unbekannt", power: 100, image: 'images/driver.png' },
    ],
    bind_car_id: 6
}; 

app.factory('backend', ['$rootScope', '$timeout', function($rootScope, $timeout) {
    var backend = {};
    var ws;

    function on_game_state(obj) {
        var state = "";
        switch (obj.state) {
        case "STOPPED":
            state = "Angehalten";
            break;
        case "RACE":
            state = "Rennen";
            break;
        case "TRAINING":
            state = "Training";
            break;
        case "COUNTDOWN":
            state = "Achtung";
            break;
        case "BINDING":
            state = "Programmierung";
            break;
        }
        if (game.state == "Programmierung") {
            $rootScope.$apply(game.bind_car_id = 6);
        }
        $rootScope.$apply(game.state = state);
    }
    
    function on_game_update(obj) {
        if (game.state == "Achtung") {
            $rootScope.$apply(game.time = 0);
        } else {
            $rootScope.$apply(game.time = obj.time);
        }
        $rootScope.$apply(game.positions = obj.positions);
    }
    
    function on_game_finished(obj) {
        $rootScope.game_finish_sound.play();
        $rootScope.$apply(game.time = obj.time);
        $rootScope.$apply(game.positions = obj.positions);
        $rootScope.$apply(game.show_game_finished = true);
        $timeout(function() {$rootScope.$apply(game.show_game_finished = false);}, 18000);
    }
    
    function on_lap_count(obj) {
        $rootScope.$apply(game.cars[obj.id].laps = obj.lap);
        $rootScope.$apply(game.cars[obj.id].last_time = obj.lap_time);
        if (obj.record) {
            $rootScope.$apply(game.cars[obj.id].best_time = obj.lap_time);
        }
    }
    
    function on_false_start(obj) {
        $rootScope.$apply(game.show_game_finished = false);
        $rootScope.$apply(game.false_start_id = obj.id);
        $rootScope.$apply(game.show_false_start = true);
        $timeout(function() {$rootScope.$apply(game.show_false_start = false);}, 5000);        
    }

    function on_laps_update(obj) {
        $rootScope.$apply(game.laps = obj.laps);
    }

    function on_countdown(obj) {
        $rootScope.$apply(game.countdown_number = obj.number);
        if (obj.number == 4) {
            for(var i = 0; i < 6; i++) {
                $rootScope.$apply(game.cars[i].laps = 0);
                $rootScope.$apply(game.cars[i].last_time = 0);
                $rootScope.$apply(game.cars[i].best_time = 0);
            }
            $rootScope.game_start_sound.play();
            $rootScope.game_start_sound2.play();
        }
    }

    function on_settings(obj) {
        if (obj.drivers && obj.drivers.length > 0) {
            $rootScope.$apply(game.drivers = obj.drivers);
        }
        $rootScope.$apply(game.controllers = obj.controllers);        
    }

    function on_controller_changed(obj) {
        $rootScope.$apply(game.controllers[obj.id].connected = obj.connected);                
    }

    backend.connect = function() {
        $rootScope.connect();
    };

    backend.send = function(data) {
        ws.send(JSON.stringify(data));
    };

    $rootScope.connect = function() {
        var url = "ws://localhost:8383/sclx";
        ws = new WebSocket(url);
        ws.onclose = function(){
            $rootScope.$apply(game.state = "Verbindungsfehler");
            $timeout(function() {$rootScope.connect();}, 1000);
        };
        ws.onmessage = function(msg){
            var obj = JSON.parse(msg.data);
            switch (obj.type) {
            case "game_state":
                on_game_state(obj);
                break;
            case "game_update":
                on_game_update(obj);
                break;
            case "game_finished":
                on_game_finished(obj);
                break;
            case "lap_count":
                on_lap_count(obj);
                break;
            case "false_start":
                on_false_start(obj);
                break;
            case "laps_update":
                on_laps_update(obj);
                break;
            case "countdown":
                on_countdown(obj);
                break;
            case "settings":
                on_settings(obj);
                break;
            case "controller_changed":
                on_controller_changed(obj);
                break;
            }
        };
    }
    
    return backend;
}]);

app.controller('sclx_ctrl', ['$rootScope', 'backend', 'ngAudio', function($rootScope, backend, ngAudio) {
    backend.connect();
    this.game = game;

    $rootScope.game_start_sound = ngAudio.load("sounds/start.wav");
    $rootScope.game_start_sound2 = ngAudio.load("sounds/start2.wav");
    $rootScope.game_finish_sound = ngAudio.load("sounds/finish.wav");

    this.save_settings = function() {
        backend.send({
            type: "settings",
            drivers: this.game.drivers,
            controllers: this.game.controllers
        });
        this.game.show_settings = false;
    };
    this.add_driver = function() {
        var next_id = 0;
        for (i in this.game.drivers) {
            while (next_id <= this.game.drivers[i].id) {
                next_id++;
            }
        }
        this.game.drivers.push({ id: next_id, name: "", power: 100, image: 'images/driver.png' });
    };
    this.delete_driver = function(id) {
        for (i in this.game.drivers) {
            if (this.game.drivers[i].id == id) {
                this.game.drivers.splice(i, 1);
                return;
            }
        }
    };
    this.bind_car = function(id) {
        this.game.bind_car_id = id;
        backend.send({
            type: "bind_car",
            id: id
        });
    };
}]);

app.config(function(ngModalDefaultsProvider) {
    return ngModalDefaultsProvider.set({
        closeButtonHtml: ""
    });
});

app.filter('sclx_time_car', function() {
    return function(input) {
        var out = "";
        if (input > 0) {
            var secs = input/1000000;
            out += secs;
        }
        return out;
    };
});

app.filter('sclx_time_clock', function() {
    return function(input) {
        var secs_total = Math.floor(input/1000000);
        var secs = secs_total % 60;
        var mins = Math.floor(secs_total / 60) % 60;
        var hrs = Math.floor(secs_total / 3600);
        var out = "";
        if (hrs < 10) {
            out += "0";
        }
        out += hrs + ":";
        if (mins < 10) {
            out += "0";
        }
        out += mins + ":";
        if (secs < 10) {
            out += "0";
        }
        out += secs;
        return out;
    };
});


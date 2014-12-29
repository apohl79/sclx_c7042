SCLX C7042 Racing
=================

An open source race control for the C7042 power base by Scalextric.

The backend is written in C++ using an event driven model. You should be able to compile it on a recent linux/unix. The current websocket implementation does probably not work on a raspberry pi, but it will get replaced. The UI is written as an AngularJS app that runs on every modern browser/device. The frontend is talking to the backend via websockets to allow for real time updates.

The project is work in progress. A lot of things are missing, but you can

- setup drivers,
- limit the max power/speed per driver,
- assign them to controllers
- and finally drive races against each other!

Have fun! :-)

Setup
-----

The Setup is quite easy for a developer.

```
# Clone
git clone https://github.com/apohl79/sclx_c7042.git
cd sclx_c7042

# Web UI
cd webui/lib
./fetch-deps.sh
cd ../..

# Compile
mkdir build
cd build
cmake
make

```

Now connect the powerbase to your computer and run

```
./sclx <uart-port>

# Example:
./sclx /dev/tty.usbserial-AH01NHJ9
```

You can point your browser to the index.html file of the webui folder now.

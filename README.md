![screenshot](assets/screenshot.gif)

# Matrix

The code rain effect from you know what.

> All I see is blonde, brunette, redhead.

## Build and Run

First, make sure you have ncurses installed.
On ubuntu: `sudo apt install libncurses-dev`

Then, with `cmake`
```
mkdir build
cd build
cmake ..
make
./apps/matrix
```
Alternatively, just
```
g++ apps/matrix.cpp -std=c++17 -lncurses -o matrix
./matrix
```
To quit, type `q`

Welcome to the desert of the real.

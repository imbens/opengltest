# opengltest
You might have to install some packages on Raspbian Buster Lite:
```
sudo apt install libegl1-mesa-dev libgles2-mesa-dev libx11-dev
```
To build on Raspbian:
```
gcc opengltest.c -lGLESv2 -lEGL -lX11 -o opengltest
```
To run on Raspbian:
```
xinit &
export DISPLAY=:0
./opengltest
```

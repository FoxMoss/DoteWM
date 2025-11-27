<img src="logo.png" height="300"></img>

**A framework for writing window managers in familiar HTML, JS, and CSS.**

A [<img src="hc.png" style="height: 2em; margin-left: 1em; margin-right: 1em;"></img>](https://hackclub.com/) project.

## Demos

**Windows 98 styled, written in dreamland**

![](blog/fastfetch.png)

[github.com/FoxMoss/dote-dreamland-win95-example](https://github.com/FoxMoss/dote-dreamland-win95-example)

This is the one I actually developed on and will have the least amount of bugs. `dreamland.js` is a
great framework, if you're new take a look [at the docs](https://dreamland.js.org/).


**Windows XP styled, written in React**

![](blog/reactdemo.png)

[github.com/FoxMoss/dote-react-xp-example](https://github.com/FoxMoss/dote-react-xp-example)

A port of the Win 98 one to React and XP.css, a bit more rough.


**DVD Logo Window Manager, written in vanilla js**

![](blog/dvdwm.gif)

[github.com/FoxMoss/dote-vanilla-dvd-example](https://github.com/FoxMoss/dote-vanilla-dvd-example)

The most rough of the bunch, completely unusable. Many hours will be wasted waiting for your
terminal to hit the corner.

## Install

### On Arch
We are on the AUR under dote-wm. So install with your favorite AUR helper.

```
yay -S dote-wm
```

### On Debian

Check [releases](https://github.com/FoxMoss/DoteWM/releases/)! I'll try to provide up to date `.deb`
files.

### On Other Distros

You'll have to build from source, it wont take too long.

The dependencies on arch are as follows and will need to be renamed to the ecosystem of your choice.
```
gcc-libs
libx11
libxext
libxcomposite
libxfixes
libxi
mesa
glew
nanomsg
protobuf
abseil-cpp
gtk3
nspr
nss
alsa-lib
nlohmann-json
cmake
git
```

Compile commands are standard CMake affair:
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j3
```

## Usage

Any files placed inside `~/.config/dote/` will be served with index.html being loaded first. Hot
reloading is best effort, sometimes a change will need a full restart.

### Testing
I would recommend developing in Xephyr as changes can occasionally crash your window manager.

``` bash
Xephyr :1 -resizeable -no-host-grab

# in another terminal
DISPLAY=:1 dotewm
```

### Daily Driving
Set your ~/.xinitrc to the following:
```xinitrc
exec dotewm
```

Or consult your login manager on usage.

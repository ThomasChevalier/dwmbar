# dwmbar

dwmbar is a simple status bar for the [dwm](https://dwm.suckless.org/) tiling window manager.

![](images/example.png)

## Installation

Configure by editing the source code in `dwmbar.c` then install:
```bash
sudo make install
```

The program sets the name of the root windows to a text formatted for the [status2d](https://dwm.suckless.org/patches/status2d/) patch of dwm.

## Description

I was not satisfied with existing status bar for dwm so I wrote my own. In particular I didn't want to call shell scripts to fetch basic informations and preferred using a more efficient polling technique than just calling a function every second. In the end, this little project strives to be the most efficient possible.

To get into more detail, standard status bars use a single thread and sleep for a pre-determined number of seconds before updating all the informations. I used this technique with great success in a previous application but it suffers from a major drawback: the smaller the update interval the snappier the bar, but also the more it draws resources. This becomes a problem for me when I was changing the sound of my computer. I wanted quick feedback but updating the status every tenth of a second seemed ridiculous. I looked for other alternatives, such as sending a signal to the status bar every time the "volume script" changes its value but in the ends I chose to monitor files for changes using [inotify](https://en.wikipedia.org/wiki/Inotify).


## Features

The status bar can display several values:

* current keyboard layout
* cpu temperature
* fan speed
* ram used
* percentage of remaining battery
* power consumption
* brightness level
* sound level
* a clock


## Technical details

Each value is contained in a `Block`. A `Block` has an icon, a color and a text content. For each block a `listener` and a `callback` are defined. The `listener` calls the `callback` whenever the content of the block should be updated.

The application is multi-threaded, and each block runs in its own thread. Thus they can update themselve independently of each other. When a block's listener fires the associated callback, the content of the block changes and a global `pthread_cond_t` condition variable is unblocked. The main function can then loop over all the blocks, see which one has changed, and then set the output text accordingly.

Three types of listeners are implemented:

* `time_listener`: the simplest one, simply sleep for a given interval then launch the callback.
* `aligned_time_listener`: a variant of the first listener, update a block every n seconds but align the interval on an unix timestamp. For example, the clock should be updated every 60 seconds, but I want it to change instantaneously when the minute changes. For that, we align the update interval on the timestamp `1592384460`, which is exactly 09:01:00 GMT.
* `file_listener`: update the block every time the content of a file changes. It uses the `inotify` linux kernel library to monitor the specified files.

The aligned time listener is only used for the clock.

The file listener is used for all the values changed via a custom script, which writes the new value in a file every time it is called, namely the volume, the brightness and the current keyboard layout.

Unfortunately for the rest of the values the time listener is used. It is simply not possible to react to events such as a change in the cpu temperature or a drop of the battery level. Still, I use a different update interval, based on how often I want some informations to be updated.

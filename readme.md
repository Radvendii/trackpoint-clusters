# Trackpoint Clusters

## Disclaimer

I wrote this program to do something very specific, for my very specific circumstances. I have only tested this program on my computer. I have hard coded values to the ones I want. I have tried, where possible, to make the program general-purpose, mostly so I can use it if I ever get a different computer.

I have also tried to write clear, well-commented code, so that you can modify it to your own purposes, and if it doesn't work you can understand and fix it for your setup. You will **likely** have to modify it, unless you happen to want the exact same thing I want.

All that said, I want to be clear that this program is not guaranteed to work for you, and *may in fact fuck up your system*. Any damage it does will almost certainly be reverted with a reboot, or even just restarting X11, but I don't guarantee that. If you cannot read through and understand what this program does, or don't have a friend who can do that for you, it may not be for you.

## Motivation / Story time

I recently got a nice split keyboard with thumb clusters (the [ergodox-ez](https://ergodox-ez.com/)), and then went I went to type on my thinkpad keyboard, I realized I was missing the thumb clusters. Using ctrl and shift with my left pinky finger so often is just kind of a pain. I was also missing the layers, which allowed me to have so many symbol keys within easy reach. Then I looked down at the keyboard, and it clicked. Literally, I clicked the mouse button.

## Description

This program turns the lenovo trackpoint mouse buttons into thumb clusters you can use while you type. It binds right click to shift, and left click to control. This puts them in easy range of each thumb, rather than needing to awkwardly bend your pinky over to reach them. It also rebinds left + middle click and right + middle click for an extra two keys. These have been bound to the little-known `ISO_LEVEL3_SHIFT` and `ISO_LEVEL5_SHIFT` keys. I was going to hack together my own implementation of keyboard layers, but then I realized linux literally has out-of-the box support for it, so I just used that.

However, when you want to use the trackpoint as a mouse (when, not if), that works seamlessly, too. This script will deactivate itself while the mouse is moving, so if you have your finger on the trackpoint, and start clicking away, they will act as normal mouse buttons rather than modifier keys.

### Limitations

Due to the way `XSetPointerMapping()` works, if you are holding a mouse buttons when you stop (or start) moving the mouse, the script will not be able to disable (or enable) the mouse buttons, and will freeze up. If this happens, all you have to do is take your hands off the mouse buttons and wait a minute for the program to do it's thing, and then proceed. I haven't noticed this happen too often to me, but if it breaks your workflow, and you find a way to fix it, please send a pull request!

This program gracefully handles `SIGINT`, `SIGTERM`, and `SIGHUP`. If you kill it with `C-c`, `kill` or by closing the controlling terminal, it will reset the mouse buttons and modifier keys.

However, this program does *not* gracefully handle `SIGQUIT` or `SIGKILL` (and other more esoteric ways of killing the process). If you stop the program with `C-\` or `kill -9` (or a machete), your mouse buttons and modifier keys will not be reset. You can run `xmodmap -e 'pointer = 1 2 3'` to enable mouse clicks, and `xdotool -keyup Control_L Shift_R ISO_Level3_Shift ISO_Level5_Shift` to release all modifier keys. With shift and control, you can also just press and release the appropriate keys on the keyboard.


## How it works

In linux, keyboard and mouse input is sent to pseudo-files in the `/dev/input` folder. If you read from those files, you can grab the input events as they happen, not mediated through how the computer thinks they should be interpreted.

This program grabs the mouse-clicks from the appropriate file (corresponding to the trackpoint), and simulates modifier key presses in their place. It also disables the mouse buttons from sending normal click events to your computer while it's running, so that they only act as modifier keys.

The program also monitors the device for mouse-*move* events. If it picks up a mouse-move event from the trackpoint, it will stop all of the previously described functioning, and allow the mouse to start working again.

## How to use it

Dependencies (you must have these installed for this to work): `xdo` (library, not `xdotool` the program), and `perl`.

To compile, if you are using the `nix` package manager, you can simply run `nix-build` from the source directory. If not, you should run `gcc trackpoint-clusters.c -lX11 -lxdo -o trackpoint-clusters` from the source directory, but also, you should use `nix`.

Before you run it, you will need your user to have permissions to read `/dev/input` files somehow. I recommend adding your user to the `input` group, but hey, you could also run the script as root if you're feeling risky.

To run it, you can just run the binary `trackpoint-clusters`. I would set up some way to run it at system startup, but that's up to you.

## FAQ

I haven't actually received any questions yet, I'm just guessing

1. Will this work on my computer?

   I don't know, try it.

1. Does this affect the touchpad?

   Good question! No. This script reads from the trackpoint device file only, so it will not pick up mouse-click or mouse-move events from the touchpad at all. You're free to use that as normal. (Not sure why you would, though. You have a trackpoint right there.)

1. This broke my computer!!

   That's not a question, but my advice is the following:
   
   - Stop using the keyboard for a minute. Lift up your hands; give the system time to reset. Okay, now try again.
   - Quit the script with C-c or killing the process.
   - Press and then releasing all relevant modifier keys (the actual keys on the keyboard): left control and right shift.
   - Run `xdotool -keyup Control_L Shift_R ISO_Level3_Shift ISO_Level5_Shift`
   - Run `xmodmap -e 'pointer = 1 2 3'`
   - Restart the X11 server.
   - Restart the computer.
   
1. Wow this is so genius!

   Also not a question, but aw, thanks =)

## License

Nothing in this section overrides the license found in LICENSE file. That is the license under which this code is released. If you want any other terms, you have to contact me directly.

Here's the bottom line: I want to enable people to do good things in the world. I know the AGPL license can get on people's nerves because it's too restrictive or whatever. If you want to use this code as part of a larger project and for some reason you can't get it to work with the license as is, I'm a reasonable person. Hit me up and we can talk.

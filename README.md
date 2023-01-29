# Bad MIDI keyboard Soundbox

## Config file
The config file is stored in ./config.toml. The `settings` part contains the MIDI device ID of your keyboard (just launch the program to get it). If adaptive volume is enabled, the volume of the sound played will depend on the key pressure. Volume sets the default master volume.

In the `sounds` part, you can map your key to the file you want to play, using `key = "file.mp3"`. You can get the key ID by launching the program and pressing on any key. Sounds are stored in the `./resources` folder.

```toml
[settings]
midi-device-id = 3
adaptive-volume = true
volume = 35 # in percent

[sounds]
48 = "file.mp3"
```
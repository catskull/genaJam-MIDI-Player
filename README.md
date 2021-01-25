# genaJam MIDI Player
 Alternate genaJam firmware for playing back standard midi files

This is a simple adaptation from [this example from the MD_MIDIFile library](https://github.com/MajicDesigns/MD_MIDIFile/blob/main/examples/MD_MIDIFile_Play_LCD/MD_MIDIFile_Play_LCD.ino).

The only changes are moving from an analog buton array to dedicated digital inputs and changing pin assignments when necessary.

Required libraries to build:
 - [MD_MIDIFile](https://github.com/MajicDesigns/MD_MIDIFile)
 - [MD_UISwitch](https://github.com/MajicDesigns/MD_UISwitch)
 - [SdFat 1.0](https://github.com/greiman/SdFat/releases/tag/1.1.4)

Download all and extract all libraries to your Arduino/Libraries folder (under Documents/Arduino/Libraries on macOS).

genaJam relies on [MightyCore](https://github.com/MCUdude/MightyCore), so follow their instructions to install as well. Select the ATmega1284 from the boards menu and leave all other options the default.

Files are restricted to 8 character names and must end in .MID. Format your SD cart to FAT32.
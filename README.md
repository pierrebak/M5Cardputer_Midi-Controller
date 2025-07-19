M5Cardputer Advanced MIDI Controller 🎹

An enhanced MIDI controller for the M5Cardputer that turns the compact device into a powerful and portable tool for music creation.

This project moves beyond simple key-to-note mapping, providing a complete MIDI workstation experience with an intuitive, page-based user interface, multiple performance modes, MIDI recording and playback, a fully adjustable metronome, and settings persistence.

✨ Key Features

    Page-Based UI: A clean, easy-to-read display with three distinct control pages (Perform, Transport, and Settings) that you can cycle through with the Tab key.

    5 Versatile Performance Modes: Instantly switch between Piano, Drums, Chords, Bass, and Lead modes, each with a unique keyboard layout.

    MIDI Recording & Playback: Record your performances, save them automatically to an SD card, and play them back anytime.

    Built-in Metronome: A configurable metronome with its own dedicated volume control to keep you in time.

    Sustain & Pitch Bend: Latch notes with Sustain Mode or add expression with pitch bends in Lead Mode.

    Persistent Settings: Your preferred volume, BPM, and other settings are saved to memory and loaded on startup.

    File Management: Save multiple recordings, list them, and even load specific files as "presets."

⚙️ Getting Started

Hardware Requirements

    An M5Cardputer

    A MIDI Synthesizer Unit (SAM2695)

    A microSD Card (FAT32 formatted) for saving recordings.

🔌 Hardware Setup

    Plug the M5Stack MIDI Module into the red Grove port located on the top of the M5Cardputer. The code is already configured to use this port for MIDI communication.

    Use a standard 5-pin MIDI cable to connect the module's MIDI OUT port to your synthesizer, audio interface, or computer.

    Insert your formatted microSD card into the Cardputer's SD card slot.

Software & Installation

    Install Arduino IDE: Make sure you have the latest version from the Arduino website.

    Install M5Stack Board Definitions: In the Arduino IDE, go to File > Preferences and add the following URL to the "Additional Board Manager URLs" field:

    https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json

    Install Libraries & Board:

        Open the Boards Manager (Tools > Board > Boards Manager...).

        Search for "M5Stack" and install it.

        Open the Library Manager (Sketch > Include Library > Manage Libraries...).

        Search for and install the M5Cardputer library.

    Load the Code:

        Clone or download this repository.

        Open the .ino file in the Arduino IDE.

        Connect your M5Cardputer to your computer.

        Select the correct board (Tools > Board > M5Stack Arduino > M5STACK Cardputer) and port.

        Click Upload.

🎛️ How to Use

The controller is designed for ease of use. The Tab key is your primary way to navigate between the three main interface pages.

Page 1: Perform Page (Default)

This is your main playing interface for making music.
Key(s)	Function
Z & Q rows	Play musical notes (layout changes with mode).
Shift + Note	Play an accented note (higher velocity).
/	Increase Octave.
,	Decrease Octave.
= / +	Increase default Velocity.
-	Decrease default Velocity.
m	Cycle through performance modes.
Spacebar	Toggle Sustain Mode On/Off.
.	Panic! Sends an "All Notes Off" MIDI message.
Tab	Go to the Transport Page.

Page 2: Transport & Recording Page

This page manages all recording and playback functions. Recordings are automatically saved with a timestamp when you stop.
Key(s)	Function
r	Start/Stop Recording. A ● icon appears.
p	Start/Stop Playback. A ▶ icon appears.
l	List Recordings stored on the SD card.
d	Delete all recordings. (Use with caution!)
1 - 5	Load a preset recording (song1.mid, etc.).
Tab	Go to the Settings Page.

Page 3: Metronome & Settings Page

This page controls the metronome and global device settings.
Key(s)	Function
m	Toggle Metronome On/Off.
= / +	Increase BPM by 5.
-	Decrease BPM by 5.
.	Increase Metronome Volume.
,	Decrease Metronome Volume.
h	Increase Global Master Volume.
g	Decrease Global Master Volume.
s	Save current settings to memory.
l	Load saved settings from memory.
r	Reset all settings to their defaults.
Tab	Go back to the Perform Page.

⚠️ Troubleshooting

    "SD Card init failed!": Ensure your microSD card is formatted to FAT32 and is inserted correctly into the Cardputer's slot before powering it on.

    Stuck Notes (MIDI Panic): If a note gets "stuck" and plays continuously, go to the Perform Page and press the . (period) key. This sends a universal "All Notes Off" command to silence everything.

    No Sound:

        Make sure the MIDI Module is securely connected to the Grove port.

        Check that your MIDI cable is plugged into the MIDI OUT port on the module and connected to a working MIDI IN port on your synth or computer.

        Verify your receiving device is set to the correct MIDI channel. This controller sends on Channel 1 (0) for instruments and Channel 10 (9) for drums.

        Navigate to the Settings Page and make sure the "Master Vol" is not set to zero.

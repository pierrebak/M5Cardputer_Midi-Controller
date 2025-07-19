#include "M5Cardputer.h"
#include <HardwareSerial.h>
#include <Preferences.h>
#include <SD.h>
#include <FS.h>

#define MIDI_NOTE_ON          0x90
#define MIDI_NOTE_OFF         0x80
#define MIDI_CONTROL_CHANGE   0xB0
#define MIDI_PROGRAM_CHANGE   0xC0
#define MIDI_PITCH_BEND       0xE0
#define MIDI_CHANNEL          0
#define MIDI_DRUM_CHANNEL     9

// Recording settings
#define MAX_MIDI_EVENTS       1000
#define RECORDING_FOLDER      "/midi_recordings"

// Note timing settings
#define MAX_SUSTAINED_NOTES   16
#define SUSTAIN_TIME_MS       2000
#define MAX_TEMP_NOTES        16
#define NOTE_DURATION_MS      150

// Metronome settings
#define METRONOME_NOTE        76 // High Wood Block

HardwareSerial MidiSerial(1);
Preferences prefs;

// --- Data Structures ---
struct MidiEvent {
  unsigned long timestamp;
  byte status;
  byte data1;
  byte data2;
  bool active;
};

struct SustainedNote {
  int note;
  int channel;
  unsigned long startTime;
  bool active;
};

struct TemporaryNote {
  int note;
  int channel;
  unsigned long startTime;
  bool active;
};

// --- Global State Variables ---
int uiPage = 0; // 0: Perform, 1: Transport, 2: Settings
bool isRecording = false;
bool isPlaying = false;
MidiEvent recordedEvents[MAX_MIDI_EVENTS];
int eventCount = 0;
unsigned long recordStartTime = 0;
unsigned long playStartTime = 0;
int currentPlayEvent = 0;
String currentRecordingName = "";

int mode = 0; // 0: Piano, 1: Drums, 2: Chords, 3: Bass, 4: Lead
int currentProgram = 0;
int velocity = 100;
int globalVolume = 127;
int octave = 4;
bool sustainMode = false;
SustainedNote sustainedNotes[MAX_SUSTAINED_NOTES];
TemporaryNote temporaryNotes[MAX_TEMP_NOTES];

bool metronomeEnabled = false;
int bpm = 120;
int metronomeVolume = 100;
unsigned long beatInterval = 500;
unsigned long lastBeatTime = 0;

// --- Forward Declarations for UI and Input handlers ---
void displayInterface();
void handleKey(const Keyboard_Class::KeysState& keyState);
void handlePerformPageKeys(char key, bool shift);
void handleTransportPageKeys(char key);
void handleSettingsPageKeys(char key);
void sendNoteOn(byte channel, byte pitch, byte vel);
void sendNoteOff(byte channel, byte pitch, byte vel);
void sendControlChange(byte channel, byte ctrl, byte val);
void sendProgramChange(byte channel, byte program);
void sendPitchBend(int value);
void recordMidiEvent(byte status, byte data1, byte data2);
void allNotesOff();
void setModeDefaults();
void loadSettings();
void saveSettings();
void resetSettings();
void updateBeatInterval();
void toggleRecording();
void togglePlayback();
void listRecordings();
void deleteAllRecordings();
void loadRecording(String filename);
const char* modeName();

// --- Core Functions ---
void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  MidiSerial.begin(31250, SERIAL_8N1, 1, 2);

  if (!SD.begin()) {
    M5Cardputer.Display.println("SD Card init failed!");
    delay(2000);
  } else {
    if (!SD.exists(RECORDING_FOLDER)) {
      SD.mkdir(RECORDING_FOLDER);
    }
  }

  prefs.begin("midi_controller", false);
  loadSettings();
  updateBeatInterval();

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextDatum(top_left);
  M5Cardputer.Display.setFont(&fonts::Font2);

  for (int i = 0; i < MAX_SUSTAINED_NOTES; i++) sustainedNotes[i].active = false;
  for (int i = 0; i < MAX_MIDI_EVENTS; i++) recordedEvents[i].active = false;
  for (int i = 0; i < MAX_TEMP_NOTES; i++) temporaryNotes[i].active = false;

  sendProgramChange(MIDI_CHANNEL, currentProgram);
  sendControlChange(MIDI_CHANNEL, 7, globalVolume);
  
  displayInterface();
}

void loop() {
  M5Cardputer.update();

  handleSustainedNotes();
  handleTemporaryNotes();
  handlePlayback();
  handleMetronome();

  if (M5Cardputer.Keyboard.isChange()) {
    if (M5Cardputer.Keyboard.isPressed()) {
      // Get the entire state of the keyboard
      auto keyState = M5Cardputer.Keyboard.keysState();
      
      // Call the updated handleKey function
      handleKey(keyState);
      
      displayInterface(); // Update the display after a key press
    }
  }
  delay(5);
}

// --- Input Handling ---
void handleKey(const Keyboard_Class::KeysState& keyState) {
    
  // 1. Check for the Tab key first. This is the highest priority.
  if (keyState.tab) {
    uiPage = (uiPage + 1) % 3;
    return; // Exit immediately after changing the page
  }

  // 2. Process any regular printable character that was pressed.
  for (char c : keyState.word) {
    char key = tolower(c); // Convert to lowercase for consistency
    bool shift = keyState.shift;
    
    // Global Panic Key - check for '.'
    if (key == '.') {
      allNotesOff();
      return; // Exit after panic
    }

    // 3. Dispatch to the correct page handler based on uiPage
    switch(uiPage) {
      case 0: handlePerformPageKeys(key, shift); break;
      case 1: handleTransportPageKeys(key);      break;
      case 2: handleSettingsPageKeys(key);       break;
    }
  }
}

void handlePerformPageKeys(char key, bool shift) {
  int noteVel = shift ? min(127, velocity + 20) : velocity;

  switch (key) {
    case 'm':
      mode = (mode + 1) % 5;
      setModeDefaults();
      break;
    case ',': octave = max(2, octave - 1); break;
    case '/': octave = min(7, octave + 1); break;
    case '-': velocity = max(0, velocity - 10); break;
    case '=': case '+': velocity = min(127, velocity + 10); break;
    case ' ': sustainMode = !sustainMode; break;
    default:
      if (mode == 0) handlePianoKey(key, noteVel);
      else if (mode == 1) handleDrumKey(key, noteVel);
      else if (mode == 2) handleChordKey(key, noteVel);
      else if (mode == 3) handleBassKey(key, noteVel);
      else if (mode == 4) handleLeadKey(key, noteVel);
      break;
  }
}

void handleTransportPageKeys(char key) {
  switch(key) {
    case 'r': toggleRecording(); break;
    case 'p': togglePlayback(); break;
    case 'l': listRecordings(); displayInterface(); break; // Redraw after delay
    case 'd': deleteAllRecordings(); M5Cardputer.Display.println("All recordings deleted."); delay(1000); break;
    case '1': loadRecording("song1.mid"); break;
    case '2': loadRecording("song2.mid"); break;
    case '3': loadRecording("song3.mid"); break;
    case '4': loadRecording("song4.mid"); break;
    case '5': loadRecording("song5.mid"); break;
  }
}

void handleSettingsPageKeys(char key) {
  switch(key) {
    case 'm': metronomeEnabled = !metronomeEnabled; break;
    case '-': bpm = max(40, bpm - 5); updateBeatInterval(); break;
    case '=': case '+': bpm = min(240, bpm + 5); updateBeatInterval(); break;
    case ',': metronomeVolume = max(0, metronomeVolume - 8); break;
    case '.': metronomeVolume = min(127, metronomeVolume + 8); break;
    case 'g': 
      globalVolume = max(0, globalVolume - 8);
      sendControlChange(MIDI_CHANNEL, 7, globalVolume);
      break;
    case 'h':
      globalVolume = min(127, globalVolume + 8);
      sendControlChange(MIDI_CHANNEL, 7, globalVolume);
      break;
    case 's': saveSettings(); break;
    case 'l': loadSettings(); break;
    case 'r': resetSettings(); break;
  }
}

// --- Note Playing Modes ---
void handlePianoKey(char key, int vel) {
  int note = -1;
  int baseNote = octave * 12;
  switch (key) {
    case 'z': note = baseNote; break; case 's': note = baseNote + 1; break; case 'x': note = baseNote + 2; break; case 'd': note = baseNote + 3; break; case 'c': note = baseNote + 4; break; case 'v': note = baseNote + 5; break; case 'g': note = baseNote + 6; break; case 'b': note = baseNote + 7; break; case 'h': note = baseNote + 8; break; case 'n': note = baseNote + 9; break; case 'j': note = baseNote + 10; break;
    case 'q': note = baseNote + 12; break; case '2': note = baseNote + 13; break; case 'w': note = baseNote + 14; break; case '3': note = baseNote + 15; break; case 'e': note = baseNote + 16; break; case 'r': note = baseNote + 17; break; case '5': note = baseNote + 18; break; case 't': note = baseNote + 19; break; case '6': note = baseNote + 20; break; case 'y': note = baseNote + 21; break; case '7': note = baseNote + 22; break; case 'u': note = baseNote + 23; break; case 'i': note = baseNote + 24; break;
  }
  if (note != -1) {
    if (sustainMode) addSustainedNote(MIDI_CHANNEL, note);
    else playTemporaryNote(MIDI_CHANNEL, note, vel);
  }
}

void handleDrumKey(char key, int vel) {
  int note = -1;
  switch (key) {
    case 'q': note = 36; break; case 'w': note = 38; break; case 'e': note = 42; break; case 'r': note = 46; break; case 't': note = 49; break; case 'y': note = 51; break; case 'u': note = 39; break; case 'i': note = 43; break; case 'o': note = 47; break; case 'p': note = 50; break;
  }
  if (note != -1) playTemporaryNote(MIDI_DRUM_CHANNEL, note, vel);
}

void handleChordKey(char key, int vel) {
  struct Chord { char key; int notes[4]; int noteCount; };
  int baseNote = octave * 12;
  Chord chords[] = {
    { 'q', {baseNote, baseNote+4, baseNote+7, baseNote+12}, 4 }, { 'w', {baseNote+2, baseNote+5, baseNote+9, baseNote+14}, 4 }, { 'e', {baseNote+4, baseNote+7, baseNote+11, baseNote+16}, 4 }, { 'r', {baseNote+5, baseNote+9, baseNote+12, baseNote+17}, 4 }, { 't', {baseNote+7, baseNote+11, baseNote+14, baseNote+19}, 4 }, { 'y', {baseNote+9, baseNote+12, baseNote+16, baseNote+21}, 4 },
  };
  for (auto &chord : chords) {
    if (key == chord.key) {
      for (int i = 0; i < chord.noteCount; i++) {
        if (chord.notes[i] <= 127) {
            if (sustainMode) addSustainedNote(MIDI_CHANNEL, chord.notes[i]);
            else playTemporaryNote(MIDI_CHANNEL, chord.notes[i], vel);
        }
      }
      break;
    }
  }
}

void handleBassKey(char key, int vel) {
  int note = -1;
  int baseNote = max(1, octave - 2) * 12;
  switch (key) {
    case 'q': note = baseNote; break; case 'w': note = baseNote + 2; break; case 'e': note = baseNote + 4; break; case 'r': note = baseNote + 5; break; case 't': note = baseNote + 7; break; case 'y': note = baseNote + 9; break; case 'u': note = baseNote + 11; break;
  }
  if (note != -1) playTemporaryNote(MIDI_CHANNEL, note, vel);
}

void handleLeadKey(char key, int vel) {
  int note = -1;
  int baseNote = (octave + 1) * 12;
  switch (key) {
    case 'q': note = baseNote; sendPitchBend(10000); break; case 'w': note = baseNote + 2; break; case 'e': note = baseNote + 4; sendPitchBend(6000); break; case 'r': note = baseNote + 5; break; case 't': note = baseNote + 7; sendPitchBend(10000); break; case 'y': note = baseNote + 9; break; case 'u': note = baseNote + 11; break;
  }
  if (note != -1) {
    playTemporaryNote(MIDI_CHANNEL, note, vel);
    sendPitchBend(8192); // Reset pitch bend
  }
}

// --- Note Timing and Management ---
void playTemporaryNote(int channel, int note, int vel) {
  for (int i = 0; i < MAX_TEMP_NOTES; i++) {
    if (!temporaryNotes[i].active) {
      temporaryNotes[i] = {note, channel, millis(), true};
      sendNoteOn(channel, note, vel);
      return;
    }
  }
}

void handleTemporaryNotes() {
  unsigned long currentTime = millis();
  for (int i = 0; i < MAX_TEMP_NOTES; i++) {
    if (temporaryNotes[i].active && (currentTime - temporaryNotes[i].startTime) > NOTE_DURATION_MS) {
      sendNoteOff(temporaryNotes[i].channel, temporaryNotes[i].note, 0);
      temporaryNotes[i].active = false;
    }
  }
}

void addSustainedNote(int channel, int note) {
  for (int i = 0; i < MAX_SUSTAINED_NOTES; i++) {
    if (!sustainedNotes[i].active) {
      sustainedNotes[i] = {note, channel, millis(), true};
      sendNoteOn(channel, note, velocity);
      break;
    }
  }
}

void handleSustainedNotes() {
  unsigned long currentTime = millis();
  for (int i = 0; i < MAX_SUSTAINED_NOTES; i++) {
    if (sustainedNotes[i].active && (currentTime - sustainedNotes[i].startTime) > SUSTAIN_TIME_MS) {
      sendNoteOff(sustainedNotes[i].channel, sustainedNotes[i].note, 0);
      sustainedNotes[i].active = false;
    }
  }
}

// --- Recording & Playback ---
void toggleRecording() { if (isRecording) stopRecording(); else startRecording(); }
void togglePlayback() { if (isPlaying) stopPlayback(); else startPlayback(); }

void startRecording() {
  if (isPlaying) stopPlayback();
  isRecording = true;
  eventCount = 0;
  recordStartTime = millis();
  for (int i = 0; i < MAX_MIDI_EVENTS; i++) recordedEvents[i].active = false;
  currentRecordingName = "rec_" + String(millis()) + ".mid";
}

void stopRecording() {
  isRecording = false;
  if (eventCount > 0) saveRecordingToSD();
}

void startPlayback() {
  if (eventCount == 0) return;
  if (isRecording) stopRecording();
  allNotesOff();
  isPlaying = true;
  playStartTime = millis();
  currentPlayEvent = 0;
}

void stopPlayback() {
  isPlaying = false;
  currentPlayEvent = 0;
  allNotesOff();
}

void handlePlayback() {
  if (!isPlaying || eventCount == 0) return;
  unsigned long currentTime = millis() - playStartTime;
  while (currentPlayEvent < eventCount && recordedEvents[currentPlayEvent].active && recordedEvents[currentPlayEvent].timestamp <= currentTime) {
    MidiEvent& event = recordedEvents[currentPlayEvent];
    MidiSerial.write(event.status);
    MidiSerial.write(event.data1);
    // Do not send a third byte for Program Change or Pitch Bend
    if ((event.status & 0xF0) != MIDI_PROGRAM_CHANGE && (event.status & 0xF0) != MIDI_PITCH_BEND) {
      MidiSerial.write(event.data2);
    }
    currentPlayEvent++;
  }
  if (currentPlayEvent >= eventCount) stopPlayback();
}


// --- Metronome ---
void updateBeatInterval() { beatInterval = 60000 / bpm; }

void handleMetronome() {
  if (!metronomeEnabled) return;
  unsigned long currentTime = millis();
  if (currentTime - lastBeatTime >= beatInterval) {
    lastBeatTime = currentTime;
    sendMetronomeTick();
  }
}

// --- MIDI Sending Functions ---
void sendNoteOn(byte channel, byte pitch, byte vel) {
  MidiSerial.write(MIDI_NOTE_ON | channel);
  MidiSerial.write(pitch);
  MidiSerial.write(vel);
  recordMidiEvent(MIDI_NOTE_ON | channel, pitch, vel);
}

void sendNoteOff(byte channel, byte pitch, byte vel) {
  MidiSerial.write(MIDI_NOTE_OFF | channel);
  MidiSerial.write(pitch);
  MidiSerial.write(vel);
  recordMidiEvent(MIDI_NOTE_OFF | channel, pitch, vel);
}

void sendControlChange(byte channel, byte ctrl, byte val) {
  MidiSerial.write(MIDI_CONTROL_CHANGE | channel);
  MidiSerial.write(ctrl);
  MidiSerial.write(val);
  recordMidiEvent(MIDI_CONTROL_CHANGE | channel, ctrl, val);
}

void sendProgramChange(byte channel, byte program) {
  MidiSerial.write(MIDI_PROGRAM_CHANGE | channel);
  MidiSerial.write(program);
  recordMidiEvent(MIDI_PROGRAM_CHANGE | channel, program, 0);
}

void sendPitchBend(int value) {
  byte lsb = value & 0x7F;
  byte msb = (value >> 7) & 0x7F;
  MidiSerial.write(MIDI_PITCH_BEND | MIDI_CHANNEL);
  MidiSerial.write(lsb);
  MidiSerial.write(msb);
  recordMidiEvent(MIDI_PITCH_BEND | MIDI_CHANNEL, lsb, msb);
}

void sendMetronomeTick() {
  MidiSerial.write(MIDI_NOTE_ON | MIDI_DRUM_CHANNEL);
  MidiSerial.write(METRONOME_NOTE);
  MidiSerial.write(metronomeVolume);
  delay(20);
  MidiSerial.write(MIDI_NOTE_OFF | MIDI_DRUM_CHANNEL);
  MidiSerial.write(METRONOME_NOTE);
  MidiSerial.write(0);
}


// --- File & Settings Management ---
void recordMidiEvent(byte status, byte data1, byte data2) {
  if (!isRecording || eventCount >= MAX_MIDI_EVENTS) return;
  recordedEvents[eventCount] = {millis() - recordStartTime, status, data1, data2, true};
  eventCount++;
}

void saveRecordingToSD() {
  if (eventCount == 0) return;
  String filepath = String(RECORDING_FOLDER) + "/" + currentRecordingName;
  File file = SD.open(filepath.c_str(), FILE_WRITE);
  if (!file) return;
  file.println("MIDI_RECORDING_V2");
  file.println(eventCount);
  for (int i = 0; i < eventCount; i++) {
    if (recordedEvents[i].active) {
      file.printf("%lu,%u,%u,%u\n", recordedEvents[i].timestamp, recordedEvents[i].status, recordedEvents[i].data1, recordedEvents[i].data2);
    }
  }
  file.close();
}

void loadRecording(String filename) {
  String filepath = String(RECORDING_FOLDER) + "/" + filename;
  File file = SD.open(filepath.c_str());
  if (!file) return;
  
  String header = file.readStringUntil('\n');
  if (header.indexOf("MIDI_RECORDING") == -1) { 
    file.close(); 
    return; 
  }

  eventCount = file.readStringUntil('\n').toInt();
  eventCount = min(eventCount, MAX_MIDI_EVENTS);
  for (int i = 0; i < eventCount; i++) {
    String line = file.readStringUntil('\n');
    int c1 = line.indexOf(','), c2 = line.indexOf(',', c1 + 1), c3 = line.indexOf(',', c2 + 1);
    if (c1 > 0 && c2 > c1 && c3 > c2) {
      recordedEvents[i] = {
        (unsigned long)strtoul(line.substring(0, c1).c_str(), NULL, 10),
        (byte)atoi(line.substring(c1 + 1, c2).c_str()),
        (byte)atoi(line.substring(c2 + 1, c3).c_str()),
        (byte)atoi(line.substring(c3 + 1).c_str()),
        true
      };
    }
  }
  file.close();
  currentRecordingName = filename;
}


void listRecordings() {
  File root = SD.open(RECORDING_FOLDER);
  if (!root) return;
  M5Cardputer.Display.clear();
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.setTextColor(YELLOW);
  M5Cardputer.Display.println("Recordings on SD Card:");
  int count = 0;
  File file = root.openNextFile();
  while (file && count < 10) {
    if (!file.isDirectory()) {
      M5Cardputer.Display.setTextColor(WHITE);
      M5Cardputer.Display.println(file.name());
      count++;
    }
    file.close(); // Close file to free memory
    file = root.openNextFile();
  }
  delay(4000);
  root.close();
}

void deleteAllRecordings() {
    File root = SD.open(RECORDING_FOLDER);
    if (!root) return;
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String path = String(RECORDING_FOLDER) + "/" + file.name();
            SD.remove(path);
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
    eventCount = 0; // Clear current memory too
    currentRecordingName = "";
}

void saveSettings() {
  prefs.begin("midi_controller", false);
  prefs.putInt("mode", mode);
  prefs.putInt("program", currentProgram);
  prefs.putInt("velocity", velocity);
  prefs.putInt("octave", octave);
  prefs.putBool("sustain", sustainMode);
  prefs.putInt("bpm", bpm);
  prefs.putInt("globalVol", globalVolume);
  prefs.putInt("metroVol", metronomeVolume);
  prefs.end();
}

void loadSettings() {
  prefs.begin("midi_controller", true);
  mode = prefs.getInt("mode", 0);
  currentProgram = prefs.getInt("program", 0);
  velocity = prefs.getInt("velocity", 100);
  octave = prefs.getInt("octave", 4);
  sustainMode = prefs.getBool("sustain", false);
  bpm = prefs.getInt("bpm", 120);
  globalVolume = prefs.getInt("globalVol", 127);
  metronomeVolume = prefs.getInt("metroVol", 100);
  prefs.end();
}

void resetSettings() {
  mode = 0; currentProgram = 0; velocity = 100; octave = 4;
  sustainMode = false; bpm = 120; globalVolume = 127; metronomeVolume = 100;
  saveSettings();
}

// --- Utility & Display ---
void allNotesOff() {
  for (int i = 0; i < MAX_SUSTAINED_NOTES; i++) {
    if (sustainedNotes[i].active) {
      sendNoteOff(sustainedNotes[i].channel, sustainedNotes[i].note, 0);
      sustainedNotes[i].active = false;
    }
  }
  for (int ch = 0; ch < 16; ch++) {
    sendControlChange(ch, 123, 0); // All Notes Off CC
  }
  sendPitchBend(8192);
}

void setModeDefaults() {
  switch (mode) {
    case 0: currentProgram = 0; break;   // Piano
    case 1: currentProgram = 0; break;   // Drums (uses drum channel)
    case 2: currentProgram = 24; break;  // Acoustic Guitar (Nylon) for chords
    case 3: currentProgram = 33; break;  // Electric Bass (Finger)
    case 4: currentProgram = 80; break;  // Lead 1 (Square)
  }
  sendProgramChange(MIDI_CHANNEL, currentProgram);
}

const char* modeName() {
  switch (mode) {
    case 0: return "Piano"; case 1: return "Drums"; case 2: return "Chords";
    case 3: return "Bass"; case 4: return "Lead"; default: return "Unknown";
  }
}

void displayInterface() {
    M5Cardputer.Display.clear();
    M5Cardputer.Display.setCursor(0, 0);

    switch (uiPage) {
        case 0: // Perform Page
            M5Cardputer.Display.setTextColor(YELLOW);
            M5Cardputer.Display.printf("[PERFORM] Mode: %s\n", modeName());
            M5Cardputer.Display.setTextColor(WHITE);
            M5Cardputer.Display.printf("Oct: %d  Vel: %d  Prog: %d\n", octave, velocity, currentProgram);
            M5Cardputer.Display.setTextColor(sustainMode ? CYAN : WHITE);
            M5Cardputer.Display.printf("Sustain: %s\n", sustainMode ? "ON" : "OFF");
            M5Cardputer.Display.setTextColor(GREEN);
            M5Cardputer.Display.println("\n,/. Oct  -/= Vel  m Mode");
            M5Cardputer.Display.println("Space: Sustain  .: Panic!");
            break;

        case 1: // Transport Page
            M5Cardputer.Display.setTextColor(YELLOW);
            M5Cardputer.Display.println("[TRANSPORT & REC]");
            M5Cardputer.Display.setTextColor(isRecording ? RED : WHITE);
            M5Cardputer.Display.printf("Rec: %s %s\n", isRecording ? "ON " : "OFF", isRecording ? "●" : "");
            M5Cardputer.Display.setTextColor(isPlaying ? GREEN : WHITE);
            M5Cardputer.Display.printf("Play: %s %s\n", isPlaying ? "ON " : "OFF", isPlaying ? "▶" : "");
            M5Cardputer.Display.setTextColor(WHITE);
            M5Cardputer.Display.printf("Events: %d\n", eventCount);
            M5Cardputer.Display.setTextColor(GREEN);
            M5Cardputer.Display.println("\nr:Rec p:Play l:List d:Del");
            M5Cardputer.Display.println("1-5: Load Preset Recording");
            break;

        case 2: // Settings Page
            M5Cardputer.Display.setTextColor(YELLOW);
            M5Cardputer.Display.println("[METRONOME & SETTINGS]");
            M5Cardputer.Display.setTextColor(metronomeEnabled ? CYAN : WHITE);
            M5Cardputer.Display.printf("Metro: %s  BPM: %d\n", metronomeEnabled ? "ON" : "OFF", bpm);
            M5Cardputer.Display.setTextColor(WHITE);
            M5Cardputer.Display.printf("Metro Vol: %d  Master Vol: %d\n", metronomeVolume, globalVolume);
            M5Cardputer.Display.setTextColor(GREEN);
            M5Cardputer.Display.println("\nm:Metro -/+:BPM ,/.:M.Vol");
            M5Cardputer.Display.println("g/h:G.Vol s:Save l:Load r:Reset");
            break;
    }
    
    // Footer on all pages
    M5Cardputer.Display.setCursor(0, M5Cardputer.Display.height() - 12);
    M5Cardputer.Display.setTextColor(ORANGE);
    M5Cardputer.Display.print("Tab: Next Page");
}

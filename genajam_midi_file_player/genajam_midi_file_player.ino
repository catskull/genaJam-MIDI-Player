// User selects a file from the SD card list on the LCD display and plays 
// the music selected.
// Example program to demonstrate the use of the MIDIFile library.
//
// Hardware required:
//  SD card interface - change SD_SELECT for SPI comms.
//  LCD interface - assumed to be 2 rows 16 chars. Change LCD 
//    pin definitions for hardware setup. Uses the MD_UISwitch library 
//    (found at https://github.com/MajicDesigns/MD_UISwitch) to read and manage 
//    the LCD display buttons.
//

#include <SdFat.h>
#include <MD_MIDIFile.h>
#include <MD_UISwitch.h>
#include <LiquidCrystal.h>

#define DEBUG_ON  0

#if DEBUG_ON

#define DEBUG(x)  Serial.print(x)
#define DEBUGX(x) Serial.print(x, HEX)
#define SERIAL_RATE 57600

#else

#define DEBUG(x)
#define DEBUGX(x)
#define SERIAL_RATE 31250

#endif

// SD Hardware defines ---------
// SPI select pin for SD card (SPI comms).
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10).
// Other hardware will be different as documented for that hardware.
const uint8_t SD_SELECT = PIN_PB4;

// LCD display defines ---------
const uint8_t LCD_ROWS = 2;
const uint8_t LCD_COLS = 16;

// LCD user defined characters
char PAUSE = '\1';
uint8_t cPause[8] = { 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x00 };

// LCD Shield pin definitions ---------
// These need to be modified for the LCD hardware setup
const uint8_t LCD_RS = PIN_PD5;
const uint8_t LCD_ENA = PIN_PD4;
const uint8_t LCD_D4 = PIN_PB0;
const uint8_t LCD_D5 = PIN_PB1;
const uint8_t LCD_D6 = PIN_PB2;
const uint8_t LCD_D7 = PIN_PB3;
const uint8_t LCD_KEYS[] = { PIN_PC7, PIN_PC4, PIN_PC3, PIN_PC1, PIN_PC2 }; // up, down, right, left, select

// Library objects -------------
LiquidCrystal LCD(LCD_RS, LCD_ENA, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
SdFat SD;
MD_MIDIFile SMF;
MD_UISwitch_Digital *BTN[ARRAY_SIZE(LCD_KEYS)];

// Playlist handling -----------
const uint8_t FNAME_SIZE = 13;               // file names 8.3 to fit onto LCD display
const char* PLAYLIST_FILE = "PLAYLIST.TXT";  // file of file names
const char* MIDI_EXT = ".MID";               // MIDI file extension
uint16_t  plCount = 0;
char fname[FNAME_SIZE+1];

// Enumerated types for the FSM(s)
enum lcd_state  { LSBegin, LSSelect, LSShowFile };
enum midi_state { MSBegin, MSLoad, MSOpen, MSProcess, MSClose };
enum seq_state  { LCDSeq, MIDISeq };

// MIDI callback functions for MIDIFile library ---------------

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
#if !DEBUG_ON
  if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))
  {
    Serial1.write(pev->data[0] | pev->channel);
    Serial1.write(&pev->data[1], pev->size-1);
  }
  else
    Serial1.write(pev->data, pev->size);
#endif
  DEBUG("\nM T");
  DEBUG(pev->track);
  DEBUG(":  Ch ");
  DEBUG(pev->channel+1);
  DEBUG(" Data ");
  for (uint8_t i=0; i<=pev->size; i++)
  {
    DEBUGX(pev->data[i]);
    DEBUG(' ');
  }
}

void sysexCallback(sysex_event *pev)
// Called by the MIDIFile library when a System Exclusive (sysex) file event needs 
// to be processed thru the midi communications interface. Most sysex events cannot 
// really be processed, so we just ignore it here.
// This callback is set up in the setup() function.
{
  DEBUG("\nS T");
  DEBUG(pev->track);
  DEBUG(": Data ");
  for (uint8_t i=0; i<pev->size; i++)
  {
    DEBUGX(pev->data[i]);
    DEBUG(' ');
  }
}

void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so between songs turn
// off all the notes and sound
{
  midi_event ev;

  // All sound off
  // When All Sound Off is received all oscillators will turn off, and their volume
  // envelopes are set to zero as soon as possible.
  ev.size = 0;
  ev.data[ev.size++] = 0xb0;
  ev.data[ev.size++] = 120;
  ev.data[ev.size++] = 0;

  for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);
}

// LCD Message Helper functions -----------------
void LCDMessage(uint8_t r, uint8_t c, const char *msg, bool clrEol = false)
// Display a message on the LCD screen with optional spaces padding the end
{
  LCD.setCursor(c, r);
  LCD.print(msg);
  if (clrEol)
  {
    c += strlen(msg);
    while (c++ < LCD_COLS)
      LCD.write(' ');
  }
}

void LCDErrMessage(const char *msg, bool fStop)
{
  LCDMessage(1, 0, msg, true);
  DEBUG("\nLCDErr: ");
  DEBUG(msg);
  while (fStop) ;   // stop here if told to
  delay(2000);      // if not stop, pause to show message
}

// Create list of files for menu --------------

uint16_t createPlaylistFile(void)
// create a play list file on the SD card with the names of the files.
// This will then be used in the menu.
{
  SdFile    plFile;   // play list file
  SdFile    mFile;    // MIDI file
  uint16_t  count = 0;// count of files
  char      fname[FNAME_SIZE+1];

  // open/create the play list file
  if (!plFile.open(PLAYLIST_FILE, O_CREAT | O_WRITE))
  {
    LCDErrMessage("PL create fail", true);
  }
  else
  {
    SD.vwd()->rewind();
    while (mFile.openNext(SD.vwd(), O_READ))
    {
      mFile.getName(fname, FNAME_SIZE);

      DEBUG("\nFile ");
      DEBUG(count);
      DEBUG(" ");
      DEBUG(fname);

      if (mFile.isFile())
      {
        if (strcasecmp(MIDI_EXT, &fname[strlen(fname) - strlen(MIDI_EXT)]) == 0)
          // only include files with MIDI extension
        {
          plFile.write(fname, FNAME_SIZE);
          count++;
        }
      }
      mFile.close();
    }
    DEBUG("\nList completed");

    // close the play list file
    plFile.close();
  }

  return(count);
}

// FINITE STATE MACHINES -----------------------------

seq_state lcdFSM(seq_state curSS)
// Handle selecting a file name from the list (user input)
{
  static lcd_state s = LSBegin;
  static uint8_t plIndex = 0;
  static SdFile plFile;  // play list file

  // LCD state machine
  switch (s)
  {
  case LSBegin:
    LCDMessage(0, 0, "Select play:", true);
    if (!plFile.isOpen())
    {
      if (!plFile.open(PLAYLIST_FILE, O_READ))
        LCDErrMessage("PL file no open", true);
    }
    s = LSShowFile;
    break;

  case LSShowFile:
    plFile.seekSet(FNAME_SIZE*plIndex);
    plFile.read(fname, FNAME_SIZE);

    LCDMessage(1, 0, fname, true);
    LCD.setCursor(LCD_COLS-2, 1);
    LCD.print(plIndex == 0 ? ' ' : '<');
    LCD.print(plIndex == plCount-1 ? ' ' : '>');
    s = LSSelect;
    break;

  case LSSelect:
    // Keys are mapped as follows:
    // Select:  move on to the next state in the state machine
    // Left:    use the previous file name (move back one file name)
    // Right:   use the next file name (move forward one file name)
    // Up:      move to the first file name
    // Down:    move to the last file name
    if (BTN[4]->read() == MD_UISwitch::KEY_PRESS) { // Select
      DEBUG("\n>Play");
      curSS = MIDISeq;    // switch mode to playing MIDI in main loop
      s = LSBegin;        // reset for next time
    }

    else if (BTN[3]->read() == MD_UISwitch::KEY_PRESS) { // Left
      DEBUG("\n>Previous");
      if (plIndex != 0)
        plIndex--;
      s = LSShowFile;
    }

    else if (BTN[0]->read() == MD_UISwitch::KEY_PRESS) { // Up
      DEBUG("\n>First");
      plIndex = 0;
      s = LSShowFile;
    }

    else if (BTN[1]->read() == MD_UISwitch::KEY_PRESS) { // Down
      DEBUG("\n>Last");
      plIndex = plCount - 1;
      s = LSShowFile;
    }

    else if (BTN[2]->read() == MD_UISwitch::KEY_PRESS) { // Right
      DEBUG("\n>Next");
      if (plIndex != plCount - 1)
        plIndex++;
      s = LSShowFile;
    }
    break;

  default:
    s = LSBegin;        // reset for next time
    break;
  }  

  return(curSS);
}

seq_state midiFSM(seq_state curSS)
// Handle playing the selected MIDI file
{
  static midi_state s = MSBegin;
  
  switch (s)
  {
  case MSBegin:
    // Set up the LCD 
    LCDMessage(0, 0, "   \1", true);
    LCDMessage(1, 0, "K  >  \xdb", true);   // string of user defined characters
    s = MSLoad;
    break;

  case MSLoad:
    // Load the current file in preparation for playing
    {
      int  err;

      // Attempt to load the file
      if ((err = SMF.load(fname)) == MD_MIDIFile::E_OK)
        s = MSProcess;
      else
      {
        char aErr[16];

        sprintf(aErr, "SMF error %03d", err);
        LCDErrMessage(aErr, false);
        s = MSClose;
      }
    }
    break;

  case MSProcess:
    // Play the MIDI file
    if (!SMF.isEOF())
    {
      if (SMF.getNextEvent())
      {
        char  sBuf[10];
        
        sprintf(sBuf, "T%3d", SMF.getTempo());
        LCDMessage(0, LCD_COLS-strlen(sBuf), sBuf, true);
        sprintf(sBuf, "S%d/%d", SMF.getTimeSignature()>>8, SMF.getTimeSignature() & 0xf);
        LCDMessage(1, LCD_COLS-strlen(sBuf), sBuf, true);
      };
    }    
    else
      s = MSClose;

    // check the keys
    // up, down, right, left, select
    if (BTN[3]->read() == MD_UISwitch::KEY_PRESS) { midiSilence();  SMF.restart(); }        // Rewind (button left)
    else if (BTN[2]->read() == MD_UISwitch::KEY_PRESS) { s = MSClose; }                     // Stop   (button right)
    else if (BTN[0]->read() == MD_UISwitch::KEY_PRESS) { SMF.pause(true); midiSilence(); }  // Pause  (button up)
    else if (BTN[1]->read() == MD_UISwitch::KEY_PRESS) { SMF.pause(false); }                // Start  (button down)
    break;

  case MSClose:
    // close the file and switch mode to user input
    SMF.close();
    midiSilence();
    curSS = LCDSeq;
    // fall through to default state

  default:
    s = MSBegin;
    break;
  }

  return(curSS);
}

void setup(void)
{
  // initialize MIDI output stream
  Serial1.begin(SERIAL_RATE);

  // initialize LCD keys
  for (uint8_t i = 0; i < ARRAY_SIZE(BTN); i++)
  {
    BTN[i] = new MD_UISwitch_Digital(LCD_KEYS[i], LOW);
    BTN[i]->begin();
  }

  // initialize LCD display
  LCD.begin(LCD_COLS, LCD_ROWS);
  LCD.clear();
  LCD.noCursor();
  LCDMessage(0, 0, "  Midi  Player  ", false);
  LCDMessage(1, 0, "  ------------  ", false);

  // Load characters to the LCD
  LCD.createChar(PAUSE, cPause);

  // initialize SDFat
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
    LCDErrMessage("SD init fail!", true);

  plCount = createPlaylistFile();
  if (plCount == 0)
    LCDErrMessage("No files", true);
  
  // initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);

  delay(750);   // allow the welcome to be read on the LCD
}

void loop(void)
// only need to look after 2 things - the user interface (LCD_FSM) 
// and the MIDI playing (MIDI_FSM). While playing we have a different 
// mode from choosing the file, so the FSM will run alternately, depending 
// on which state we are currently in.
{
  static seq_state s = LCDSeq;

  switch (s)
  {
    case LCDSeq:  s = lcdFSM(s);	break;
    case MIDISeq: s = midiFSM(s);	break;
    default: s = LCDSeq;
  }
}

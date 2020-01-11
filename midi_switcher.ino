#include <EEPROM.h>
#include <MIDI.h>
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_7segment matrix = Adafruit_7segment();

/*
 AAA
 F B
 GGG
 E C
 DDD

 0B00GFEDCBA
*/

#define MATRIX_A 0B001110111
#define MATRIX_B 0B001111100
#define MATRIX_C 0B000111001
#define MATRIX_D 0B001011110
#define MATRIX_E 0B001111001
#define MATRIX_I 0B000110000
#define MATRIX_N 0B001010100
#define MATRIX_T 0B001111000
#define MATRIX_O 0B001011100

// init edit

#define SOFTWARE_VERSION 1

// Number of seconds to hold down save button on power up to
// force a reset
#define RESET_DELAY 4

// Matrix raw display values
#define MATRIX_BLANK 0B000000000
#define MATRIX_DASH 0B001000000

// Potentiometer inputs
#define MAX_POT_VALUE 1024
#define TOTAL_POT_STEPS 17
#define CHAN_LEFT_POT 2
#define CHAN_RIGHT_POT 3
float EMA_a = 0.6;   //initialization of EMA alpha
int EMA_S_LEFT = 0;  //initialization of EMA S for input channel
int EMA_S_RIGHT = 0; //initialization of EMA S for output channel

#define DEBUG true

MIDI_CREATE_DEFAULT_INSTANCE();

// byte leftPotChannel = 0;
// byte rightPotChannel = 0;
// byte lastLeftPotChannel = 0;
// byte lastRightPotChannel = 0;
// bool isOutPotDirty = false;

#define IN_A 0
#define IN_B 1
#define IN_C 2
#define IN_D 3

//newt code
byte aChannel = 0;
byte bChannel = 0;
byte out1From = 0;
byte out1Channel = 0;
byte cChannel = 0;
byte dChannel = 0;
byte out2From = 0;
byte out2Channel = 0;

byte leftPotChannel = 0;
byte rightPotChannel = 0;
byte lastLeftPotChannel = 0;
byte lastRightPotChannel = 0;
byte isLeftPotDirty = 0;
byte isRightPotDirty = 0;

byte DIRECTION_LEFT = 0;
byte DIRECTION_NONE = 1;
byte DIRECTION_RIGHT = 2;

byte leftPotDirection = DIRECTION_NONE;
byte rightPotDirection = DIRECTION_NONE;
byte lastLeftPotDirection = DIRECTION_NONE;
byte lastRightPotDirection = DIRECTION_NONE;

// needs to have eprom nuke mega default mode (hold down button for a long time)
// and config mode (hold down button at boot)

//end newt code

// Array to hold inputs and outputs.  Index is input channel, value is output channel.
// 0 = No output, 1 = Midi Channel 1, ..., 16 = Midi Channel 16
// byte channel_map[17] = {MIDI_CHANNEL_OFF, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

// Array to keep track of which channels are active. We use this information to display
// a dot in the LED display next to the input channel (so the user can see visually
// which channels are getting messages)
// bool active_map[17] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};

// EEPROM Addresses
#define VERSION_ADDR 0 // value of 1 if memory has been initialized
#define A_CHAN_ADDR 1
#define B_CHAN_ADDR 2
#define OUT_1_FROM_ADDR 3
#define OUT_1_CHAN_ADDR 4
#define C_CHAN_ADDR 5
#define D_CHAN_ADDR 6
#define OUT_2_FROM_ADDR 7
#define OUT_2_CHAN_ADDR 8

// Stuff that only runs every so often
#define LOOP_DELAY 500
int loopTicks = 0;

#define BLINK_DELAY 20000
int isBlinkingCharacterOn = false;
int blinkTicks = 0;

#define MODE_PERFORMANCE 0
#define MODE_CONFIG 1
int mode = MODE_PERFORMANCE;

// Button
#define BUTTON_PIN 4
int buttonState;

void setup()
{

  // Save Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  buttonState = digitalRead(BUTTON_PIN);

  // Init display
  matrix.begin(0x70);
  matrix.setBrightness(15);

  // Load saved data from EEPROM if it has been initialized
  byte versionNum = EEPROM.read(VERSION_ADDR);
  if (versionNum != SOFTWARE_VERSION)
  {
    reset();
    matrixDisplayDashes();
    delay(4000);
  }

  setMappingsFromEEPROM();

  // matrix.drawColon(false);
  matrixDisplayBlank();

  int counter = 0;
  bool isUserWantingConfigMode = false;
  bool isUserWantingToReset = false;

  //@TODO
  // isUserWantingConfigMode = true;

  while (buttonState == LOW)
  {
    if (counter < (RESET_DELAY * 10))
    {
      if (!isUserWantingConfigMode)
      {
        matrixDisplayWord(MATRIX_E, MATRIX_D, MATRIX_I, MATRIX_T);
      }

      isUserWantingConfigMode = true;
      counter++;
    }
    else
    {
      if (!isUserWantingToReset)
      {
        matrixDisplayWord(MATRIX_I, MATRIX_N, MATRIX_I, MATRIX_T);
      }

      isUserWantingToReset = true;
    }

    buttonState = digitalRead(BUTTON_PIN);
    delay(100);
  }

  // If the user held the button long enough, reset
  if (isUserWantingToReset)
  {
    reset();
  }

  if (isUserWantingConfigMode)
  {
    startConfigMode();
  }
  else
  {
    startPerformanceMode();
  }
}

void matrixDisplayBlank()
{
  matrix.writeDigitRaw(0, MATRIX_BLANK);
  matrix.writeDigitRaw(1, MATRIX_BLANK);
  matrix.writeDigitRaw(3, MATRIX_BLANK);
  matrix.writeDigitRaw(4, MATRIX_BLANK);
  matrix.writeDisplay();
}

void matrixDisplayDashes()
{
  matrix.writeDigitRaw(0, MATRIX_DASH);
  matrix.writeDigitRaw(1, MATRIX_DASH);
  matrix.writeDigitRaw(3, MATRIX_DASH);
  matrix.writeDigitRaw(4, MATRIX_DASH);
  matrix.writeDisplay();
}

void matrixDisplayAllZeroes()
{
  matrix.writeDigitNum(0, 0, false);
  matrix.writeDigitNum(1, 0, false);
  matrix.writeDigitNum(3, 0, false);
  matrix.writeDigitNum(4, 0, false);
  matrix.writeDisplay();
}

void matrixDisplayWord(byte a, byte b, byte c, byte d)
{
  matrix.writeDigitRaw(0, a);
  matrix.writeDigitRaw(1, b);
  matrix.writeDigitRaw(3, c);
  matrix.writeDigitRaw(4, d);
  matrix.writeDisplay();
}

void startPerformanceMode()
{
  mode = MODE_PERFORMANCE;

  // Display Zeroes
  matrixDisplayAllZeroes();
  delay(1000);

  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleControlChange(handleControlChange);
  MIDI.setHandleProgramChange(handleProgramChange);
  MIDI.setHandleAfterTouchPoly(handleAfterTouchPolyPressure);
  MIDI.setHandleAfterTouchChannel(handleAfterTouchChannelPressure);
  MIDI.setHandlePitchBend(handlePitchBend);
  MIDI.setHandleTimeCodeQuarterFrame(handleTimeCodeQuarterFrame);
  MIDI.setHandleSongPosition(handleSongPosition);
  MIDI.setHandleSongSelect(handleSongSelect);
  MIDI.setHandleTuneRequest(handleTuneRequest);
  MIDI.setHandleClock(handleClock);
  MIDI.setHandleStart(handleStart);
  MIDI.setHandleContinue(handleContinue);
  MIDI.setHandleStop(handleStop);
  MIDI.setHandleActiveSensing(handleActiveSensing);
  MIDI.setHandleSystemReset(handleSystemReset);
  // (SysEx messages are not supported)

  MIDI.begin(MIDI_CHANNEL_OMNI);

  MIDI.turnThruOff();
}

void startConfigMode()
{
  mode = MODE_CONFIG;

  // matrixDisplayWord('O', 'K', 'O', 'K');
  matrix.drawColon(true);
}

void reset()
{
  aChannel = 1;
  bChannel = 2;
  out1From = IN_A;
  out1Channel = 1;
  cChannel = 3;
  dChannel = 4;
  out2From = IN_C;
  out2Channel = 2;

  writeToEEPROM();
}

void writeToEEPROM()
{
  // Set the initialized flag
  EEPROM.write(VERSION_ADDR, SOFTWARE_VERSION);

  // Write the various mappings:
  EEPROM.write(A_CHAN_ADDR, aChannel);
  EEPROM.write(B_CHAN_ADDR, bChannel);
  EEPROM.write(OUT_1_CHAN_ADDR, out1Channel);
  EEPROM.write(OUT_1_FROM_ADDR, out1From);
  EEPROM.write(C_CHAN_ADDR, cChannel);
  EEPROM.write(D_CHAN_ADDR, dChannel);
  EEPROM.write(OUT_2_CHAN_ADDR, out2Channel);
  EEPROM.write(OUT_2_FROM_ADDR, out2From);
}

void setMappingsFromEEPROM()
{
  EEPROM.get(A_CHAN_ADDR, aChannel);
  EEPROM.get(B_CHAN_ADDR, bChannel);
  EEPROM.get(OUT_1_FROM_ADDR, out1From);
  EEPROM.get(OUT_1_CHAN_ADDR, out1Channel);

  EEPROM.get(C_CHAN_ADDR, cChannel);
  EEPROM.get(D_CHAN_ADDR, dChannel);
  EEPROM.get(OUT_2_FROM_ADDR, out2From);
  EEPROM.get(OUT_2_CHAN_ADDR, out2Channel);
}

void loop()
{
  buttonState = digitalRead(BUTTON_PIN);

  if (++blinkTicks == 20000)
  {
    isBlinkingCharacterOn = !isBlinkingCharacterOn;
    blinkTicks = 0;
  }

  bool shouldUpdateDisplay = loopTicks == 0;

  switch (mode)
  {
  case MODE_PERFORMANCE:
    runPerformance(shouldUpdateDisplay);
    break;

  case MODE_CONFIG:
    runConfig(shouldUpdateDisplay);
    break;
  }

  // Run the pot inputs and display channels every 100 loops
  if (++loopTicks == LOOP_DELAY)
  {

    // resetActiveStateForAllChannels();

    loopTicks = 0;
  }
}

void runPerformance(bool shouldUpdateDisplay)
{
  MIDI.read();

  if (buttonState == LOW)
  {
    updateChannels();
  }

  if (shouldUpdateDisplay)
  {
    getPerformancePotInputs();
    updatePerformanceDisplay();
  }
  // if (++blinkTicks == BLINK_DELAY)
}

void runConfig(bool shouldUpdateDisplay)
{
  if (buttonState == LOW)
  {
    updateMappings();
  }

  if (shouldUpdateDisplay)
  {
    getConfigPotInputs();
    updateConfigDisplay();
  }
}

void updateMappings()
{
  switch (leftPotChannel)
  {
  case 0:
    aChannel = rightPotChannel;
    EEPROM.write(A_CHAN_ADDR, aChannel);
    break;

  case 1:
    bChannel = rightPotChannel;
    EEPROM.write(B_CHAN_ADDR, bChannel);
    break;

  case 2:
    out1Channel = rightPotChannel;
    EEPROM.write(OUT_1_CHAN_ADDR, out1Channel);
    break;

  case 3:
    cChannel = rightPotChannel;
    EEPROM.write(C_CHAN_ADDR, cChannel);
    break;

  case 4:
    dChannel = rightPotChannel;
    EEPROM.write(D_CHAN_ADDR, dChannel);
    break;

  case 5:
    out2Channel = rightPotChannel;
    EEPROM.write(OUT_2_CHAN_ADDR, out2Channel);
    break;
  }
}

byte getOutChannel(byte inChannel)
{
  // The OMNI channel output takes first prioirty. If it's set
  // then send ALL messages to that channel!
  // if (isOMNIMode())
  // {
  //   return channel_map[MIDI_CHANNEL_OMNI];
  // }

  // return channel_map[in_channel];

  if ((inChannel == aChannel && out1From == IN_A) || (inChannel == bChannel && out1From == IN_B))
  {
    return out1Channel;
  }

  if ((inChannel == cChannel && out2From == IN_C) || (inChannel == dChannel && out2From == IN_D))
  {
    return out2Channel;
  }

  return -1;
}

// bool isOMNIMode()
// {
//   return channel_map[MIDI_CHANNEL_OMNI] != MIDI_CHANNEL_OFF;
// }

// -------------------------------------------
// MIDI Handlers:
// -------------------------------------------

void handleProgramChange(byte channel, byte number)
{
  // markChannelAsActive(channel);

  byte outChannel = getOutChannel(channel);
  if (outChannel == -1)
    return;

  MIDI.sendProgramChange(number, outChannel);
}

void handleNoteOn(byte channel, byte note, byte velocity)
{
  // markChannelAsActive(channel);
  byte outChannel = getOutChannel(channel);
  if (outChannel == -1)
    return;

  MIDI.sendNoteOn(note, velocity, outChannel);
}

void handleNoteOff(byte channel, byte note, byte velocity)
{
  // markChannelAsActive(channel);
  byte outChannel = getOutChannel(channel);
  if (outChannel == -1)
    return;

  MIDI.sendNoteOff(note, velocity, outChannel);
}

void handlePitchBend(byte channel, int bend)
{
  // markChannelAsActive(channel);
  byte outChannel = getOutChannel(channel);
  if (outChannel == -1)
    return;

  MIDI.sendPitchBend(bend, outChannel);
}

void handleControlChange(byte channel, byte number, byte value)
{
  // markChannelAsActive(channel);
  byte outChannel = getOutChannel(channel);
  if (outChannel == -1)
    return;

  MIDI.sendControlChange(number, value, outChannel);
}

void handleAfterTouchPolyPressure(byte channel, byte note, byte pressure)
{
  // markChannelAsActive(channel);
  byte outChannel = getOutChannel(channel);
  if (outChannel == -1)
    return;

  MIDI.sendAfterTouch(note, pressure, outChannel);
}

void handleAfterTouchChannelPressure(byte channel, byte pressure)
{
  // markChannelAsActive(channel);
  byte outChannel = getOutChannel(channel);
  if (outChannel == -1)
    return;

  MIDI.sendAfterTouch(pressure, outChannel);
}

void handleTimeCodeQuarterFrame(byte data)
{
  MIDI.sendTimeCodeQuarterFrame(data);
}

void handleSongPosition(unsigned beats)
{
  MIDI.sendSongPosition(beats);
}

void handleSongSelect(byte songnumber)
{
  MIDI.sendSongSelect(songnumber);
}

void handleTuneRequest()
{
  MIDI.sendTuneRequest();
}

// Real time messages (Messages that don't specify channels)
// (These messages don't have their own sendX method, instead we must use a
// lower-level send method from the library):
void handleClock()
{
  MIDI.sendRealTime(midi::Clock);
}

void handleStart()
{
  MIDI.sendRealTime(midi::Start);
}

void handleContinue()
{
  MIDI.sendRealTime(midi::Continue);
}

void handleStop()
{
  MIDI.sendRealTime(midi::Stop);
}

void handleActiveSensing()
{
  MIDI.sendRealTime(midi::ActiveSensing);
}

void handleSystemReset()
{
  MIDI.sendRealTime(midi::SystemReset);
}

// -------------------------------------------
// Manual input methods:
// -------------------------------------------

void getPerformancePotInputs()
{
  // In pot should read OMNI (0), then 1 to 16
  leftPotChannel = normalizePotInput(emaSmooth(analogRead(CHAN_LEFT_POT), &EMA_S_LEFT), true);
  // Out pot should read OFF, then 1 to 16
  rightPotChannel = normalizePotInput(emaSmooth(analogRead(CHAN_RIGHT_POT), &EMA_S_RIGHT), true);

  if (leftPotChannel > lastLeftPotChannel)
  {
    leftPotDirection = DIRECTION_RIGHT;
  }
  else if (leftPotChannel < lastLeftPotChannel)
  {
    leftPotDirection = DIRECTION_LEFT;
  }

  if (rightPotChannel > lastRightPotChannel)
  {
    rightPotDirection = DIRECTION_RIGHT;
  }
  else if (rightPotChannel < lastRightPotChannel)
  {
    rightPotDirection = DIRECTION_LEFT;
  }

  bool a = out1From == IN_A && leftPotDirection == DIRECTION_RIGHT;
  bool b = out1From == IN_B && leftPotDirection == DIRECTION_LEFT;
  isLeftPotDirty = a || b;
  bool c = out2From == IN_C && rightPotDirection == DIRECTION_RIGHT;
  bool d = out2From == IN_D && rightPotDirection == DIRECTION_LEFT;
  isRightPotDirty = c || d;

  lastLeftPotChannel = leftPotChannel;
  lastRightPotChannel = rightPotChannel;
}

void getConfigPotInputs()
{
  // In pot should read OMNI (0), then 1 to 16
  leftPotChannel = normalizePotInput(emaSmooth(analogRead(CHAN_LEFT_POT), &EMA_S_LEFT), true);
  // Out pot should read OFF, then 1 to 16
  rightPotChannel = normalizePotInput(emaSmooth(analogRead(CHAN_RIGHT_POT), &EMA_S_RIGHT), false);

  int mappedChannel = getConfigMappedChannel(leftPotChannel);

  if (lastLeftPotChannel != leftPotChannel)
  {
    isRightPotDirty = false;
  }

  if (lastRightPotChannel != rightPotChannel)
  {
    isRightPotDirty = true;
  }

  lastLeftPotChannel = leftPotChannel;
  lastRightPotChannel = rightPotChannel;
}

int normalizePotInput(float rawIn, bool zeroIndex)
{
  rawIn = rawIn / MAX_POT_VALUE;
  rawIn = rawIn * (TOTAL_POT_STEPS - 1);
  int out = (int)(rawIn + 0.5f);

  // Putting this here because the channel input starts at zero,
  // but the channel output starts at 1 and goes to "--"
  if (!zeroIndex)
  {
    out += 1;
  }
  return out;
}

int emaSmooth(int sensorValue, int *EMA_S)
{
  *EMA_S = (EMA_a * sensorValue) + ((1 - EMA_a) * (*EMA_S));
  return *EMA_S;
}

// -------------------------------------------
// Display methods:
// -------------------------------------------

int getConfigMappedChannel(int leftPotValue)
{
  int mappedChannel = 0;

  switch (leftPotValue)
  {
  case 0:
    mappedChannel = aChannel;
    break;

  case 1:
    mappedChannel = bChannel;
    break;

  case 2:
    mappedChannel = out1Channel;
    break;

  case 3:
    mappedChannel = cChannel;
    break;

  case 4:
    mappedChannel = dChannel;
    break;

  case 5:
    mappedChannel = out2Channel;
    break;

  default:
    mappedChannel = -1;
    break;
  }

  return mappedChannel;
}

void updateConfigDisplay()
{
  // //@TODO
  // bool isOmni = isOMNIMode();

  // // Display the colons if in omni mode to indicate locked outputs
  // matrix.drawColon(isOmni);

  // // We display the left dot if we got a MIDI message on that channel:
  // displayLeft(leftPotChannel, isChannelActive(leftPotChannel));

  // // If the out pot has been rotated (isOutPotDirty=true) then we display
  // // the current actual out pot value along with the 'dirty' dot.
  // // If we're in omni mode, ignore this case.
  // if (isOutPotDirty && (!isOmni || leftPotChannel == 0))
  // {
  //   displayRight(rightPotChannel, true);
  // }
  // // Otherwise, show what the current mapping is for the given in pot channel.
  // else
  // {
  //   displayRight(getOutChannel(leftPotChannel), false);
  // }

  int mappedChannel = getConfigMappedChannel(leftPotChannel);

  switch (leftPotChannel)
  {
  case 0:
    matrix.writeDigitRaw(0, MATRIX_A);
    matrix.writeDigitRaw(1, MATRIX_BLANK);
    break;

  case 1:
    matrix.writeDigitRaw(0, MATRIX_B);
    matrix.writeDigitRaw(1, MATRIX_BLANK);
    break;

  case 2:
    matrix.writeDigitRaw(0, MATRIX_O);
    matrix.writeDigitNum(1, 1);
    break;

  case 3:
    matrix.writeDigitRaw(0, MATRIX_C);
    matrix.writeDigitRaw(1, MATRIX_BLANK);
    break;

  case 4:
    matrix.writeDigitRaw(0, MATRIX_D);
    matrix.writeDigitRaw(1, MATRIX_BLANK);
    break;

  case 5:
    matrix.writeDigitRaw(0, MATRIX_O);
    matrix.writeDigitNum(1, 2);
    break;

  default:
    matrix.writeDigitRaw(0, MATRIX_DASH);
    matrix.writeDigitRaw(1, MATRIX_DASH);
    break;
  }

  matrix.writeDisplay();

  /*
  if (rightPotChannel != mappedChannel)
  {
    isRightPotDirty = true;
  }
  else
  {
    isRightPotDirty = false;
  }
  */

  if (isRightPotDirty)
  {
    displayRight(rightPotChannel, mappedChannel != rightPotChannel);
  }
  else
  {
    displayRight(mappedChannel, false);
  }
}

void updatePerformanceDisplay()
{
  //@TODO
  matrix.writeDigitRaw(0, MATRIX_BLANK);
  matrix.writeDigitRaw(1, MATRIX_BLANK);
  matrix.writeDigitRaw(3, MATRIX_BLANK);
  matrix.writeDigitRaw(4, MATRIX_BLANK);

  if (out1From == IN_A)
  {
    matrix.writeDigitRaw(0, MATRIX_A);

    if (isLeftPotDirty && isBlinkingCharacterOn)
    {
      matrix.writeDigitRaw(1, MATRIX_B);
    }
  }
  else
  {
    matrix.writeDigitRaw(1, MATRIX_B);

    if (isLeftPotDirty && isBlinkingCharacterOn)
    {
      matrix.writeDigitRaw(0, MATRIX_A);
    }
  }

  if (out2From == IN_C)
  {
    matrix.writeDigitRaw(3, MATRIX_C);

    if (isRightPotDirty && isBlinkingCharacterOn)
    {
      matrix.writeDigitRaw(4, MATRIX_D);
    }
  }
  else
  {
    matrix.writeDigitRaw(4, MATRIX_D);

    if (isRightPotDirty && isBlinkingCharacterOn)
    {
      matrix.writeDigitRaw(3, MATRIX_C);
    }
  }

  matrix.writeDisplay();
}

void displayLeft(byte val, bool dot)
{
  int leftDigit = val / 10;
  if (leftDigit == 0)
  {
    matrix.writeDigitRaw(0, MATRIX_BLANK);
  }
  else
  {
    matrix.writeDigitNum(0, (val / 10), false);
  }
  matrix.writeDigitNum(1, val % 10, dot);
  matrix.writeDisplay();
}

// NOTE: MIDI_CHANNEL_OFF is actually 17, but we want to display that
// on the LCD as "--".
void displayRight(int val, bool dot)
{
  if (val == MIDI_CHANNEL_OFF)
  {
    // Display "--"
    matrix.writeDigitRaw(3, MATRIX_DASH);
    matrix.writeDigitRaw(4, MATRIX_DASH);
  }
  else
  {
    int leftDigit = val / 10;
    if (leftDigit == 0)
    {
      matrix.writeDigitRaw(3, MATRIX_BLANK);
    }
    else
    {
      matrix.writeDigitNum(3, (val / 10), false);
    }
    matrix.writeDigitNum(4, val % 10, dot);
  }

  matrix.writeDisplay();
}

void updateChannels()
{
  if (isLeftPotDirty)
  {
    if (out1From == IN_A)
    {
      out1From = IN_B;
    }
    else
    {
      out1From = IN_A;
    }
  }

  if (isRightPotDirty)
  {
    if (out2From == IN_C)
    {
      out2From = IN_D;
    }
    else
    {
      out2From = IN_C;
    }
  }

  isLeftPotDirty = false;
  isRightPotDirty = false;
  leftPotDirection = DIRECTION_NONE;
  rightPotDirection = DIRECTION_NONE;

  EEPROM.write(OUT_1_FROM_ADDR, out1From);
  EEPROM.write(OUT_2_FROM_ADDR, out2From);
}
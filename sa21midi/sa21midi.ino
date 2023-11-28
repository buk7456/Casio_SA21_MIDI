/*******************************************************************************
  Arduino uno (Atmega328p) code for Casio_SA-21 MIDI mod
  BUK   buk7456@gmail.com

  Guiding Docs used:
  - The MIDI specification pdf
  -> Also see the official MIDI website

*******************************************************************************/

#define SERIAL_RATE      115200

#define NOTE_ON_CMD      0x90
#define NOTE_OFF_CMD     0x80
#define PITCH_BEND_CMD   0xA0
#define PROG_CHANGE_CMD  0xC0
#define CTRL_CHANGE_CMD  0xB0

uint8_t noteOnVelocity = 127;
uint8_t noteOffVelocity = 127;

//--------pins---------
const int8_t ledPin = A4;
const int8_t pitchWheelPin = A5;
const int8_t rowPin[8] = {2, 3, 4, 5, 6, 7, 8, 9};
const int8_t columnPin[8] = {10, 11, 12, 13, A0, A1, A2, A3};

//------piano key matrix---
uint8_t keyToMidiMap[8][4];
bool    keyPressed[8][4];

//-----buttons matrix------
//array to store pressed buttons
bool buttonPressed[8][4];
bool buttonArr[32]; //buttonPressed as a 1d array
enum {
  _G = 0, _H, _I, _J, _K, _D, _B, _E, _L, _M, _N, _O,
  _P, _F, _C, _A, _R, _S, _Q, _Z, _Y, _X, _W, _V, _U, _T
}; //button map

//-------------------------
bool shiftLastState = HIGH, octaveLastState = HIGH;
bool transposeLastState = HIGH, programLastState = HIGH;
bool shiftPressed = false;
bool octavePressed = false;
bool transposePressed = false;
bool programPressed = false;

uint8_t startNote = 29;

uint8_t channel = 0x00;

uint8_t prevKeyMap = 29;
bool isDefaultMapping = false; 


//declarations
void activateColumn(uint8_t col);
void noteOn(uint8_t row, uint8_t col);
void noteOff(uint8_t row, uint8_t col);
void allNotesOff();
void setKeyMap(uint8_t _startingNote);
void processButton();

//------------------------------------------------------------------------------

void setup()
{
  for(uint8_t i = 0; i < 8; i++)
  {
    pinMode(columnPin[i], OUTPUT);
    pinMode(rowPin[i], INPUT);
  }

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  setKeyMap(startNote); //setup the keyMap. see the piano keys to pitch table

  Serial.begin(SERIAL_RATE);
}

//------------------------------------------------------------------------------

void loop()
{
  uint32_t loopStartTime = millis();
  
  ///-----------------SCAN KEY MATRIX-----------------
  ///Scan and process piano keys
  for(uint8_t col = 0; col < 4; col++)
  {
    activateColumn(col);
    delayMicroseconds(30);
    for(uint8_t row = 0; row < 8; row++)
    {
      //get row values at this column
      uint8_t rowVal = digitalRead(rowPin[row]);
      //send midi msg for piano keys pressed
      if(rowVal && !keyPressed[row][col])
      {
        keyPressed[row][col] = true;
        noteOn(row, col);
      }
      //send midi msg piano keys released
      if(!rowVal && keyPressed[row][col])
      {
        keyPressed[row][col] = false;
        noteOff(row, col);
      }
    }
  }
  
  ///-------------SCAN BUTTON MATRIX---------------
  for (uint8_t col = 0; col < 4; col ++)
  {
    activateColumn(col + 4);
    delayMicroseconds(30);
    for(uint8_t row = 0; row < 8; row++) 
    {
      uint8_t rowVal = digitalRead(rowPin[row]);
      //buttons pressed
      if(rowVal && !buttonPressed[row][col])
      {
        buttonPressed[row][col] = true;
        processButton();
      }
      //buttons released
      if(!rowVal && buttonPressed[row][col])
        buttonPressed[row][col] = false;
    }
  }
  
  ///---------------- READ PITCH WHEEL ----------------
  //readPitchWheel();
  
  /// ------ LIMIT LOOP RATE
  uint8_t loopTime = millis() - loopStartTime;
  if(loopTime < 10)
    delay(10 - loopTime);
}

//------------------------------------------------------------------------------

void processButton()
{
  //copy buttonPressed 2d array to 1d array for easy lookup of buttons
  uint8_t k = 0;
  for(uint8_t j = 0; j < 4; j += 1)
  {
    for(uint8_t i = 0; i < 8; i += 1)
    {
      buttonArr[k] = buttonPressed[i][j];
      k++;
    }
  }

  ///--- Octave up and down, transpose up and down
  //to change octave, we subtract or add 12 to keymap
  //to transpose, add or subtract 1 to keymap

  if(buttonArr[_A]) //set octave down
  {
    allNotesOff();
    setKeyMap(startNote - 12);
    isDefaultMapping = false;
  }
  else if(buttonArr[_B]) //set octave up
  {
    allNotesOff();
    setKeyMap(startNote + 12);
    isDefaultMapping = false;
  }

  if(buttonArr[_C]) //set transpose down
  {
    allNotesOff();
    setKeyMap(startNote - 1);
    isDefaultMapping = false;
  }
  else if(buttonArr[_D]) //set transpose up
  {
    allNotesOff();
    setKeyMap(startNote + 1);
    isDefaultMapping = false;
  }

  ///--- Switch channel
  //support five channels for now
  if (buttonArr[_V])
  {
    allNotesOff();
    channel = 0x00;
  }
  else if (buttonArr[_W])
  {
    allNotesOff();
    channel = 0x01;
  }
  else if (buttonArr[_X])
  {
    allNotesOff();
    channel = 0x02;
  }
  else if (buttonArr[_Y])
  {
    allNotesOff();
    channel = 0x03;
  }
  else if (buttonArr[_Z])
  {
    allNotesOff();
    channel = 0x04;
  }

  ///--- Change note on, off velocities
  if (buttonArr[_G] && noteOnVelocity >= 17)
    noteOnVelocity -= 10;
  else if (buttonArr[_H] && noteOnVelocity <= 117)
    noteOnVelocity += 10;
  
  if (buttonArr[_L] && noteOffVelocity >= 17)
    noteOffVelocity -= 10;
  else if (buttonArr[_M] && noteOffVelocity <= 117)
    noteOffVelocity += 10;
  
  ///--- Quickly switch to default key map and back
  if(buttonArr[_F])
  {
    allNotesOff();
    if(!isDefaultMapping)
    {
      prevKeyMap = keyToMidiMap[0][0]; //store existing mapping start note
      setKeyMap(29); //set default keymap
      isDefaultMapping = true; //set flag
    }
    else
    {
      isDefaultMapping = false;
      setKeyMap(prevKeyMap);
    } 
  }
}

//------------------------------------------------------------------------------

void setKeyMap(uint8_t _startingNote)
{
  uint8_t note = _startingNote;
  if (note > 96 || note < 0)
    return;

  startNote = note;
  //light up LED to show we've not transposed
  digitalWrite(ledPin, (startNote % 12 == 5));

  for(uint8_t col = 0; col < 4; col++)
  {
    for(uint8_t row = 0; row < 8; row++)
    {
      keyPressed[row][col] = false;
      keyToMidiMap[row][col] = note;
      note++;
    }
  }
}

//------------------------------------------------------------------------------

void activateColumn(uint8_t col)
{
  //turn off all columns
  for(uint8_t i = 0; i < 8; i++)
    digitalWrite(columnPin[i], LOW);
  //turn on a single column
  digitalWrite(columnPin[col], HIGH);
}

//------------------------------------------------------------------------------

void noteOn(uint8_t row, uint8_t col)
{
  Serial.write(NOTE_ON_CMD | channel);
  Serial.write(keyToMidiMap[row][col]);
  Serial.write(noteOnVelocity);
}

//------------------------------------------------------------------------------

void noteOff(uint8_t row, uint8_t col)
{
  Serial.write(NOTE_OFF_CMD | channel);
  Serial.write(keyToMidiMap[row][col]);
  Serial.write(noteOffVelocity);
}

//------------------------------------------------------------------------------

void allNotesOff()
{
  //turn off all previously pressed but unreleased notes
  for(uint8_t col = 0; col < 4; col++)
  {
    for(uint8_t row = 0; row < 8; row++)
    {
      if(keyPressed[row][col])
      {
        noteOff(row, col);
        keyPressed[row][col] = false;
      }
    }
  }
}

/*
  xdsp_32_tm1640.ino - Support for TM1640 seven-segment/matrix displays for Tasmota

  Copyright (C) 2023  Carsten Wolters
  Copyright (C) 2021  Ajith Vasudevan

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_DISPLAY
#ifdef USE_DISPLAY_TM1640
/*********************************************************************************************\
  This driver enables the display of numbers (both integers and floats) and basic text
  on the inexpensive TM1640-based seven-segment/matrix modules.

  Raw segments can also be displayed.

  In addition, it is also possible to set brightness (8 levels), clear the display, scroll text.

  To use, compile Tasmota with USE_DISPLAY and USE_DISPLAY_TM1640, or build the tasmota-display env.

  For TM1640:
  Connect the TM1640 display module's pins to any free GPIOs of the ESP8266 module
  and assign the pins as follows from Tasmota's GUI:

  DIO hardware pin --> "TM1640 DIO"
  CLK hardware pin --> "TM1640 CLK"

  Once the GPIO configuration is saved and the ESP8266/ESP32 module restarts,
  set the Display Model to 32 and Display Mode to 0
  using the command "Backlog DisplayModel 32 ; DisplayMode 0"

  After the ESP8266/ESP32 module restarts again, turn ON the display with the command "Power 1"

  Now, the following "Display" commands can be used:


  DisplayClear

                               Clears the display, command: "DisplayClear"


  DisplayNumber         num [,position {0-(Settings->display_width-1))} [,leading_zeros {0|1} [,length {1 to Settings->display_width}]]]

                               Clears and then displays number without decimal. command e.g., "DisplayNumber 1234"
                               Control 'leading zeros', 'length' and 'position' with  "DisplayNumber 1234, <position>, <leadingZeros>, <length>"
                               'leading zeros' can be 1 or 0 (default), 'length' can be 1 to Settings->display_width, 'position' can be 0 (left-most) to Settings->display_width (right-most).
                               See function description below for more details.

  DisplayNumberNC       num [,position {0-(Settings->display_width-1))} [,leading_zeros {0|1} [,length {1 to Settings->display_width}]]]

                               Display integer number as above, but without clearing first. e.g., "DisplayNumberNC 1234". Usage is same as above.



  DisplayFloat          num [,position {0-(Settings->display_width-1)} [,precision {0-Settings->display_width} [,length {1 to Settings->display_width}]]]

                               Clears and then displays float (with decimal point)  command e.g., "DisplayFloat 12.34"
                               See function description below for more details.



  DisplayFloatNC        num [,position {0-(Settings->display_width-1)} [,precision {0-Settings->display_width} [,length {1 to Settings->display_width}]]]

                               Displays float (with decimal point) as above, but without clearing first. command e.g., "DisplayFloatNC 12.34"
                               See function description below for more details.



  DisplayRaw            position {0-(Settings->display_width-1)},length {1 to Settings->display_width}, num1 [, num2[, num3[, num4[, ...upto Settings->display_width numbers]]]]]

                               Takes upto Settings->display_width comma-separated integers (0-255) and displays raw segments. Each number represents a
                               7-segment digit. Each 8-bit number represents individual segments of a digit.
                               For example, the command "DisplayRaw 0, 4, 255, 255, 255, 255" would display "[8.8.8.8.]"



  DisplayText           text [, position {0-(Settings->display_width-1)} [,length {1 to Settings->display_width}]]

                               Clears and then displays basic text.  command e.g., "DisplayText ajith vasudevan"
                               Control 'length' and 'position' with  "DisplayText <text>, <position>, <length>"
                               'length' can be 1 to Settings->display_width, 'position' can be 0 (left-most) to Settings->display_width-1 (right-most)
                               A caret(^) symbol in the text input is dispayed as the degrees(°) symbol. This is useful for displaying Temperature!
                               For example, the command "DisplayText 22.5^" will display "22.5°".


  DisplayTextNC         text [, position {0-Settings->display_width-1} [,length {1 to Settings->display_width}]]

                               Clears first, then displays text. Usage is same as above.



  DisplayScrollText     text [, num_loops]

                              Displays scrolling text indefinitely, until another Display- command (other than DisplayScrollText 
                              or DisplayScrollDelay is issued). Optionally, stop scrolling after num_loops iterations.



  DisplayScrollDelay delay {0-15}   // default = 4

                               Sets the speed of text scroll. Smaller delay = faster scrolling.

  DisplayLevel          num {0-100}

                               Display a horizontal bar graph (0-100) command e.g., "DisplayLevel 50" will display [||||    ]
\*********************************************************************************************/

#define XDSP_32 32

#define CMD_MAX_LEN 55
#define LEVEL_MIN 0
#define LEVEL_MAX 100
#define SCROLL_MAX_LEN 50
//#undef USE_DISPLAY_MODES1TO5

#include <WEMOS_Matrix_GFX.h>

const uint8_t icons[][64] PROGMEM = {
{ B00111100,
  B01000010,
  B10100101,
  B10000001,
  B10100101,
  B10011001,
  B01000010,
  B00111100 },
{ B00010000,
  B00011000,
  B00011100,
  B00011110,
  B00011100,
  B00011000,
  B00010000,
  B00000000 },
{ B00000000,
  B01111110,
  B01111110,
  B01111110,
  B01111110,
  B01111110,
  B01111110,
  B00000000 },
{ B00000000,
  B01111110,
  B01100110,
  B01000010,
  B01000010,
  B01100110,
  B01111110,
  B00000000 },
{ B11111111,
  B10000001,
  B10011001,
  B10111101,
  B10111101,
  B10011001,
  B10000001,
  B11111111 },
{ B00000000,
  B01100110,
  B01100110,
  B01100110,
  B01100110,
  B01100110,
  B01100110,
  B00000000 }
};

MLED *tm1640display;

enum display_types
{
  TM1640
};

struct
{
  char scroll_text[CMD_MAX_LEN];
  char display_text[1];
  char msg[60];
  char model_name[8];
  uint8_t scroll_delay = 4;
  uint8_t scroll_index = 0;
  uint8_t iteration = 0;
  uint8_t scroll_counter = 0;
  uint8_t scroll_counter_max = 3;
  uint8_t display_type = TM1640;
  uint8_t digit_order[6] = { 0, 1, 2, 3, 4, 5 };

  bool init_done = false;
  bool scroll = false;
  bool show_clock = false;
  bool clock_24 = false;
} TM1640Data;

/*********************************************************************************************\
* Init function
\*********************************************************************************************/

void TM1640Init(void)
{
  if (PinUsed(GPIO_TM1640CLK) && PinUsed(GPIO_TM1640DIO))
  {
    TM1640Data.display_type = TM1640;
    if ((!Settings->display_width || Settings->display_width > 1))
    {
      Settings->display_width = 1;
      Settings->display_options.type = 0;
    }
  }
  else
  {
    return;
  }

  Settings->display_model = XDSP_32;
  Settings->display_cols[0] = Settings->display_width;
  Settings->display_height = 1;
  Settings->display_rows = Settings->display_height;
  uint8_t dimmer16 = GetDisplayDimmer16();
  if(!dimmer16 || dimmer16 < 2 || dimmer16 > 15) SetDisplayDimmer(50);

  if (TM1640 == TM1640Data.display_type)
  {
    strcpy_P(TM1640Data.model_name, PSTR("TM1640"));
    tm1640display = new MLED(3, Pin(GPIO_TM1640DIO), Pin(GPIO_TM1640CLK));
    AddLog(LOG_LEVEL_INFO, PSTR("DSP: INIT(if (TM1640 == TM1640Data.display_type))"));
  }
  TM1640ClearDisplay();
  TM1640Dim();
  TM1640Data.init_done = true;
  AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s with %d digits (type %d)"), TM1640Data.model_name, Settings->display_width, Settings->display_options.type);
}

// Function to fix order of hardware digits for different TM1640 variants
void TM1640SetDigitOrder(void) {
  if (0 == Settings->display_options.type) {
    for (uint32_t i = 0; i < 6; i++) {
      TM1640Data.digit_order[i] = i;
    }
  }
  else if (1 == Settings->display_options.type) {
    TM1640Data.digit_order[0] = 2;
    TM1640Data.digit_order[1] = 1;
    TM1640Data.digit_order[2] = 0;
    TM1640Data.digit_order[3] = 5;
    TM1640Data.digit_order[4] = 4;
    TM1640Data.digit_order[5] = 3;
  }
}

/*********************************************************************************************\
* Displays number without decimal, with/without leading zeros, specifying start-position
* and length, optionally skipping clearing display before displaying the number.
* commands: DisplayNumber   num [,position {0-(Settings->display_width-1)} [,leading_zeros {0|1} [,length {1 to Settings->display_width}]]]
*           DisplayNumberNC num [,position {0-(Settings->display_width-1)} [,leading_zeros {0|1} [,length {1 to Settings->display_width}]]]    // "NC" --> "No Clear"
\*********************************************************************************************/
bool CmndTM1640Number(bool clear)
{
  char sNum[CMD_MAX_LEN];
  char sLeadingzeros[CMD_MAX_LEN];
  char sPosition[CMD_MAX_LEN];
  char sLength[CMD_MAX_LEN];
  uint8_t length = 0;
  bool leadingzeros = false;
  uint8_t position = 0;

  uint32_t num = 0;

  switch (ArgC())
  {
  case 4:
    subStr(sLength, XdrvMailbox.data, ",", 4);
    length = atoi(sLength);
  case 3:
    subStr(sLeadingzeros, XdrvMailbox.data, ",", 3);
    leadingzeros = atoi(sLeadingzeros);
  case 2:
    subStr(sPosition, XdrvMailbox.data, ",", 2);
    position = atoi(sPosition);
  case 1:
    subStr(sNum, XdrvMailbox.data, ",", 1);
    num = atof(sNum);
  }

  if ((position < 0) || (position > (Settings->display_width - 1)))
    position = 0;

  AddLog(LOG_LEVEL_DEBUG, PSTR("TM7: num %d, pos %d, lead %d, len %d"), num, position, leadingzeros, length);

  if (clear)
    TM1640ClearDisplay();

  char txt[30];
  snprintf_P(txt, sizeof(txt), PSTR("%d"), num);
  if (!length)
    length = strlen(txt);
  if ((length < 0) || (length > Settings->display_width))
    length = Settings->display_width;

  char pad = (leadingzeros ? '0' : ' ');
  uint32_t i = position;
  uint8_t rawBytes[1];

  for (; i < position + (length - strlen(txt)); i++)
  {
    if (i > Settings->display_width)
      break;
    if (TM1640 == TM1640Data.display_type)
    {
      if (i > 2)
        break;
      tm1640display->print(i);
      tm1640display->writeDisplay();
    }
  }

  for (uint32_t j = 0; i < position + length; i++, j++)
  {
    if (i > Settings->display_width)
      break;
    if (txt[j] == 0)
      break;
  }

  return true;
}

/*********************************************************************************************\
* Displays number with decimal, specifying position, precision and length,
* optionally skipping clearing display before displaying the number.
* commands: DisplayFloat   num [,position {0-(Settings->display_width-1)} [,precision {0-Settings->display_width} [,length {1 to Settings->display_width}]]]
*           DisplayFloatNC num [,position {0-(Settings->display_width-1)} [,precision {0-Settings->display_width} [,length {1 to Settings->display_width}]]]    // "NC" --> "No Clear"
\*********************************************************************************************/
bool CmndTM1640Float(bool clear)
{

  char sNum[CMD_MAX_LEN];
  char sPrecision[CMD_MAX_LEN];
  char sPosition[CMD_MAX_LEN];
  char sLength[CMD_MAX_LEN];
  uint8_t length = 0;
  uint8_t precision = Settings->display_width;
  uint8_t position = 0;

  float fnum = 0.0f;

  switch (ArgC())
  {
  case 4:
    subStr(sLength, XdrvMailbox.data, ",", 4);
    length = atoi(sLength);
  case 3:
    subStr(sPrecision, XdrvMailbox.data, ",", 3);
    precision = atoi(sPrecision);
  case 2:
    subStr(sPosition, XdrvMailbox.data, ",", 2);
    position = atoi(sPosition);
  case 1:
    subStr(sNum, XdrvMailbox.data, ",", 1);
    fnum = atof(sNum);
  }

  if ((position < 0) || (position > (Settings->display_width - 1)))
    position = 0;
  if ((precision < 0) || (precision > Settings->display_width))
    precision = Settings->display_width;

  if (clear)
    TM1640ClearDisplay();

  char txt[30];
  ext_snprintf_P(txt, sizeof(txt), PSTR("%*_f"), precision, &fnum);

  if (!length)
    length = strlen(txt);
  if ((length <= 0) || (length > Settings->display_width))
    length = Settings->display_width;

  AddLog(LOG_LEVEL_DEBUG, PSTR("TM7: num %4_f, prec %d, len %d"), &fnum, precision, length);

  return true;
}

// /*********************************************************************************************\
// * Clears the display
// * Command:  DisplayClear
// \*********************************************************************************************/
bool CmndTM1640Clear(void)
{
  TM1640ClearDisplay();
  sprintf(TM1640Data.msg, PSTR("Cleared"));
  XdrvMailbox.data = TM1640Data.msg;
  AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s cleared, data: %s"), TM1640Data.model_name, TM1640Data.msg);
  return true;
}

// /*********************************************************************************************\
// * Clears the display
// \*********************************************************************************************/
void TM1640ClearDisplay(void)
{
  if (TM1640 == TM1640Data.display_type)
  {
    tm1640display->clear();
    tm1640display->writeDisplay();
    AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s cleared, data: %s"), TM1640Data.model_name, TM1640Data.msg);
  }
}

/*********************************************************************************************\
* Display scrolling text
* Command:   DisplayTM1640Data.scroll_text text
\*********************************************************************************************/
bool CmndTM1640ScrollText(void)
{

  char sString[SCROLL_MAX_LEN + 1];
  char sMaxLoopCount[CMD_MAX_LEN];
  uint8_t maxLoopCount = 0;

  switch (ArgC())
  {
  case 2:
    subStr(sMaxLoopCount, XdrvMailbox.data, ",", 2);
    maxLoopCount = atoi(sMaxLoopCount);
  case 1:
    subStr(sString, XdrvMailbox.data, ",", 1);
  }

  if (maxLoopCount < 0)
    maxLoopCount = 0;

  AddLog(LOG_LEVEL_DEBUG, PSTR("TM7: sString %s, maxLoopCount %d"), sString, maxLoopCount);

  TM1640Data.scroll_counter_max = maxLoopCount;

  if (strlen(sString) > SCROLL_MAX_LEN)
  {
    snprintf(TM1640Data.msg, sizeof(TM1640Data.msg), PSTR("Text too long. Length should be less than %d"), SCROLL_MAX_LEN);
    XdrvMailbox.data = TM1640Data.msg;
    return false;
  }
  else
  {
    snprintf(TM1640Data.scroll_text, sizeof(TM1640Data.scroll_text), PSTR("                                                               "));
    snprintf(TM1640Data.scroll_text, Settings->display_width + sizeof(TM1640Data.scroll_text), PSTR("        %s"), &sString);
    TM1640Data.scroll_text[strlen(sString) + Settings->display_width] = 0;
    TM1640Data.scroll_index = 0;
    TM1640Data.scroll = true;
    TM1640Data.scroll_counter = 0;
    return true;
  }
}

/*********************************************************************************************\
* Sets the scroll delay for scrolling text.
* Command:  DisplayTM1640Data.scroll_delay delay {0-15}    // default = 4
\*********************************************************************************************/
bool CmndTM1640ScrollDelay(void)
{
  if (ArgC() == 0)
  {
    XdrvMailbox.payload = TM1640Data.scroll_delay;
    return true;
  }
  if (TM1640Data.scroll_delay < 0)
    TM1640Data.scroll_delay = 0;
  TM1640Data.scroll_delay = XdrvMailbox.payload;
  return true;
}

/*********************************************************************************************\
* Scrolls a given string. Called every 50ms
\*********************************************************************************************/
void TM1640ScrollText(void)
{
  if(!TM1640Data.scroll) return;
  TM1640Data.iteration++;
  if (TM1640Data.scroll_delay)
    TM1640Data.iteration = TM1640Data.iteration % TM1640Data.scroll_delay;
  else
    TM1640Data.iteration = 0;
  if (TM1640Data.iteration)
    return;

  if (TM1640Data.scroll_index > strlen(TM1640Data.scroll_text))
  {
    TM1640Data.scroll_index = 0;
    TM1640Data.scroll_counter++;
    if(TM1640Data.scroll_counter_max != 0 && (TM1640Data.scroll_counter >= TM1640Data.scroll_counter_max)) {
      TM1640Data.scroll = false;
      return;
    }    
  }
  uint8_t rawBytes[1];
  for (uint32_t i = 0, j = TM1640Data.scroll_index; i < 1 + strlen(TM1640Data.scroll_text); i++, j++)
  {
    if (i > (Settings->display_width - 1))
    {
      break;
    }
    // rawBytes[0] = tm1640display->encode(TM1640Data.scroll_text[j]);
    bool dotSkipped = false;
    if (TM1640Data.scroll_text[j + 1] == '.')
    {
      dotSkipped = true;
      rawBytes[0] = rawBytes[0] | 128;
      j++;
    }
    else if (TM1640Data.scroll_text[j] == '^')
    {
      rawBytes[0] = 1 | 2 | 32 | 64;
    }
    if (!dotSkipped && TM1640Data.scroll_text[j] == '.')
    {
      j++;
      TM1640Data.scroll_index++;
      // rawBytes[0] = tm1640display->encode(TM1640Data.scroll_text[j]);
    }
    if (TM1640Data.scroll_text[j + 1] == '.')
    {
      rawBytes[0] = rawBytes[0] | 128;
    }
  }
  TM1640Data.scroll_index++;
}

/*********************************************************************************************\
* Displays a horizontal bar graph. Takes a percentage number (0-100) as input
* Command:   DisplayLevel level {0-100}
\*********************************************************************************************/
bool CmndTM1640Level(void)
{
  uint16_t val = XdrvMailbox.payload;
  if ((val < LEVEL_MIN) || (val > LEVEL_MAX))
  {
    Response_P(PSTR("{\"Error\":\"Level should be a number in the range [%d, %d]\"}"), LEVEL_MIN, LEVEL_MAX);
    return false;
  }

  uint8_t totalBars = 2 * Settings->display_width;
  AddLog(LOG_LEVEL_DEBUG, PSTR("TM7: TM1640Data.model_name %s CmndTM1640Level totalBars=%d"), TM1640Data.model_name, totalBars);
  float barsToDisplay = totalBars * val / 100.0f;
  char txt[5];
  ext_snprintf_P(txt, sizeof(txt), PSTR("%*_f"), 1, &barsToDisplay);
  AddLog(LOG_LEVEL_DEBUG, PSTR("TM7: TM1640Data.model_name %s CmndTM1640Level barsToDisplay=%s"), TM1640Data.model_name, txt);
  char s[4];
  ext_snprintf_P(s, sizeof(s), PSTR("%0_f"), &barsToDisplay);
  uint8_t numBars = atoi(s);
  AddLog(LOG_LEVEL_DEBUG, PSTR("TM7: CmndTM1640Level numBars %d"), numBars);

  TM1640ClearDisplay();
  uint8_t rawBytes[1];
  for (int i = 1; i <= numBars; i++)
  {
    uint8_t digit = (i - 1) / 2;
    uint8_t value = (((i % 2) == 0) ? 54 : 48);
  }
  return true;
}

/*********************************************************************************************\
* Display arbitrary data on the display module
* Command:   DisplayRaw position {0-(Settings->display_width-1)},length {1 to Settings->display_width}, a [, b[, c[, d[...upto Settings->display_width]]]]
* where a,b,c,d... are upto Settings->display_width numbers in the range 0-255, each number (byte)
* corresponding to a single 7-segment digit. Within each byte, bit 0 is segment A,
* bit 1 is segment B etc. The function may either set the entire display
* or any desired part using the length and position parameters.
\*********************************************************************************************/
bool CmndTM1640Raw(void)
{
  AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s CmndTM1640Raw : %d"), TM1640Data.model_name, 1);
  uint8_t DATA[6] = {0, 0, 0, 0, 0, 0};

  char as[CMD_MAX_LEN];
  char bs[CMD_MAX_LEN];
  char cs[CMD_MAX_LEN];
  char ds[CMD_MAX_LEN];
  char es[CMD_MAX_LEN];
  char fs[CMD_MAX_LEN];

  char sLength[CMD_MAX_LEN];
  char sPos[CMD_MAX_LEN];

  uint32_t position = 0;
  uint32_t length = 0;

  switch (ArgC())
  {
  case 8:
    subStr(fs, XdrvMailbox.data, ",", 8);
    DATA[5] = atoi(fs);
  case 7:
    subStr(es, XdrvMailbox.data, ",", 7);
    DATA[4] = atoi(es);
  case 6:
    subStr(ds, XdrvMailbox.data, ",", 6);
    DATA[3] = atoi(ds);
  case 5:
    subStr(cs, XdrvMailbox.data, ",", 5);
    DATA[2] = atoi(cs);
  case 4:
    subStr(bs, XdrvMailbox.data, ",", 4);
    DATA[1] = atoi(bs);
  case 3:
    subStr(as, XdrvMailbox.data, ",", 3);
    DATA[0] = atoi(as);
  case 2:
    subStr(sLength, XdrvMailbox.data, ",", 2);
    length = atoi(sLength);
  case 1:
    subStr(sPos, XdrvMailbox.data, ",", 1);
    position = atoi(sPos);
  }

  if (!length)
    length = ArgC() - 2;
  if (length < 0 || length > Settings->display_width)
    length = Settings->display_width;
  if (position < 0 || position > (Settings->display_width - 1))
    position = 0;

  AddLog(LOG_LEVEL_DEBUG, PSTR("TM7: a %d, b %d, c %d, d %d, e %d, f %d, len %d, pos %d"),
         DATA[0], DATA[1], DATA[2], DATA[3], DATA[4], DATA[5], length, position);

  return true;
}

bool CmndTM1640Dots(void)
{
  AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s CmndTM1640Dots : %s"), TM1640Data.model_name, XdrvMailbox.data);
  char sString[CMD_MAX_LEN + 1];
  uint32_t index = 0;
  switch (ArgC())
  {
  case 1:
    subStr(sString, XdrvMailbox.data, ",", 1);
    index = atoi(sString);
    AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s CmndTM1640Dots:index : %d"), TM1640Data.model_name, index);
  }

  AddLog(LOG_LEVEL_INFO, PSTR("TM7: icons %d"), icons[index]);
  TM1640ClearDisplay();
  tm1640display->drawBitmap(0, 0, icons[index], 8, 8, 1);
  // tm1640display->drawPixel(index, index, 1);
  tm1640display->writeDisplay();

  return true;
}

/*********************************************************************************************\
* Display a given string.
* Text can be placed at arbitrary location on the display using the length and
* position parameters without affecting the rest of the display.
* Command:   DisplayText text [, position {0-(Settings->display_width-1)} [,length {1 to Settings->display_width}]]
\*********************************************************************************************/
bool CmndTM1640Text(bool clear)
{
  char sString[CMD_MAX_LEN + 1];
  char sPosition[CMD_MAX_LEN];
  char sLength[CMD_MAX_LEN];
  uint8_t length = 0;
  uint8_t position = 0;

  switch (ArgC())
  {
  case 3:
    subStr(sLength, XdrvMailbox.data, ",", 3);
    length = atoi(sLength);
  case 2:
    subStr(sPosition, XdrvMailbox.data, ",", 2);
    position = atoi(sPosition);
  case 1:
    subStr(sString, XdrvMailbox.data, ",", 1);
  }

  if ((position < 0) || (position > (Settings->display_width - 1)))
    position = 0;

  AddLog(LOG_LEVEL_DEBUG, PSTR("TM7: sString %s, pos %d, len %d"), sString, position, length);

  if (clear)
    TM1640ClearDisplay();

  if (!length)
    length = strlen(sString);
  if ((length < 0) || (length > Settings->display_width))
    length = Settings->display_width;

  uint32_t i = position;
  if (TM1640 == TM1640Data.display_type)
  {
    AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s DisplayText: %s"), TM1640Data.model_name, sString);
    tm1640display->setCursor(0,0);
    tm1640display->print(sString);
    tm1640display->writeDisplay();
  }
  return true;
}

/*********************************************************************************************\
* Displays a clock.
* Command: DisplayClock 1   // 12-hour format
*          DisplayClock 2   // 24-hour format
*          DisplayClock 0   // turn off clock and clear
\*********************************************************************************************/
bool CmndTM1640Clock(void)
{

  TM1640Data.show_clock = XdrvMailbox.payload;

  if (ArgC() == 0)
    XdrvMailbox.payload = 1;
  if (XdrvMailbox.payload > 1) {
    TM1640Data.clock_24 = true;
    XdrvMailbox.payload = 2;
  } else {
    TM1640Data.clock_24 = false;
    XdrvMailbox.payload = 1;
  }

  AddLog(LOG_LEVEL_DEBUG, PSTR("TM7: TM1640Data.show_clock %d, TM1640Data.clock_24 %d"), TM1640Data.show_clock, TM1640Data.clock_24);

  TM1640ClearDisplay();
  return true;
}

/*********************************************************************************************\
* refreshes the time if clock is displayed
\*********************************************************************************************/
void TM1640ShowTime()
{
  uint8_t hr = RtcTime.hour;
  uint8_t mn = RtcTime.minute;
  // uint8_t hr = 1;
  // uint8_t mn = 0;
  char z = ' ';
  if (TM1640Data.clock_24)
  {
    z = '0';
  }
  else
  {
    if (hr > 12)
      hr -= 12;
    if (hr == 0)
      hr = 12;
  }

  char tm[5];
  if (hr < 10)
  {
    if (mn < 10)
      snprintf(tm, sizeof(tm), PSTR("%c%d0%d"), z, hr, mn);
    else
      snprintf(tm, sizeof(tm), PSTR("%c%d%d"), z, hr, mn);
  }
  else
  {
    if (mn < 10)
      snprintf(tm, sizeof(tm), PSTR("%d0%d"), hr, mn);
    else
      snprintf(tm, sizeof(tm), PSTR("%d%d"), hr, mn);
  }
}

/*********************************************************************************************\
* This function is called for all Display functions.
\*********************************************************************************************/
bool TM1640MainFunc(uint8_t fn)
{
  bool result = false;
  if(fn != FUNC_DISPLAY_SCROLLDELAY) TM1640Data.scroll = false;
  if (XdrvMailbox.data_len > CMD_MAX_LEN)
  {
    Response_P(PSTR("{\"Error\":\"Command text too long. Please limit it to %d characters\"}"), CMD_MAX_LEN);
    return false;
  }
  AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s bool TM1640MainFunc(uint8_t fn) : %d"), TM1640Data.model_name, fn);
  switch (fn)
  {
  case FUNC_DISPLAY_CLEAR:
    result = CmndTM1640Clear();
    break;
  case FUNC_DISPLAY_NUMBER:
    result = CmndTM1640Number(true);
    break;
  case FUNC_DISPLAY_NUMBERNC:
    result = CmndTM1640Number(false);
    break;
  case FUNC_DISPLAY_FLOAT:
    result = CmndTM1640Float(true);
    break;
  case FUNC_DISPLAY_FLOATNC:
    result = CmndTM1640Float(false);
    break;
  case FUNC_DISPLAY_RAW:
    result = CmndTM1640Raw();
    break;
  case FUNC_DISPLAY_DOTS:
    result = CmndTM1640Dots();
    break;
  case FUNC_DISPLAY_SEVENSEG_TEXT:
    result = CmndTM1640Text(true);
    break;
  case FUNC_DISPLAY_SEVENSEG_TEXTNC:
    result = CmndTM1640Text(false);
    break;
  case FUNC_DISPLAY_LEVEL:
    result = CmndTM1640Level();
    break;
  case FUNC_DISPLAY_SCROLLTEXT:
    result = CmndTM1640ScrollText();
    break;
  case FUNC_DISPLAY_SCROLLDELAY:
    result = CmndTM1640ScrollDelay();
    break;
  case FUNC_DISPLAY_CLOCK:
    result = CmndTM1640Clock();
    break;
  }
  return result;
}

void TM1640Dim(void)
{
  // GetDisplayDimmer16() = 0 - 15
  uint8_t brightness = GetDisplayDimmer16() >> 1; // 0 - 7
  AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s void TM1640Dim(void) : %d"), TM1640Data.model_name, brightness);

  if (TM1640 == TM1640Data.display_type)
  {
    tm1640display->intensity = brightness; // 0 - 7
    tm1640display->writeDisplay();
  }
}

/*********************************************************************************************/

#ifdef USE_DISPLAY_MODES1TO5

void TM1640Print(char *txt)
{
  for (uint32_t i = 0; i < Settings->display_cols[0]; i++)
  {
    

  }
}

void TM1640Center(char *txt)
{
  char line[Settings->display_cols[0] + 2];

  int len = strlen(txt);
  int offset = 0;
  if (len >= Settings->display_cols[0])
  {
    len = Settings->display_cols[0];
  }
  else
  {
    offset = (Settings->display_cols[0] - len) / 2;
  }
  memset(line, 0x20, Settings->display_cols[0]);
  line[Settings->display_cols[0]] = 0;
  for (uint32_t i = 0; i < len; i++)
  {
    line[offset + i] = txt[i];
  }
  TM1640Print(line);
}

/*
bool TM1640PrintLog(void) {
  bool result = false;

  disp_refresh--;
  if (!disp_refresh) {
    disp_refresh = Settings->display_refresh;
    if (!disp_screen_buffer_cols) { DisplayAllocScreenBuffer(); }

    char* txt = DisplayLogBuffer('\337');
    if (txt != nullptr) {
      uint8_t last_row = Settings->display_rows -1;

      strlcpy(disp_screen_buffer[last_row], txt, disp_screen_buffer_cols);
      DisplayFillScreen(last_row);

      AddLog(LOG_LEVEL_DEBUG, PSTR(D_LOG_DEBUG "[%s]"), disp_screen_buffer[last_row]);

      TM1640Print(disp_screen_buffer[last_row]);

      result = true;
    }
  }
  return result;
}
*/

void TM1640Time(void)
{
  char line[Settings->display_cols[0] + 1];

  if (Settings->display_cols[0] >= 8)
  {
    snprintf_P(line, sizeof(line), PSTR("%02d %02d %02d"), RtcTime.hour, RtcTime.minute, RtcTime.second);
  }
  else if (Settings->display_cols[0] >= 6)
  {
    snprintf_P(line, sizeof(line), PSTR("%02d%02d%02d"), RtcTime.hour, RtcTime.minute, RtcTime.second);
  }
  else
  {
    snprintf_P(line, sizeof(line), PSTR("%02d%02d"), RtcTime.hour, RtcTime.minute);
  }
  TM1640Center(line);
}

void TM1640Date(void)
{
  char line[Settings->display_cols[0] + 1];

  if (Settings->display_cols[0] >= 8)
  {
    snprintf_P(line, sizeof(line), PSTR("%02d-%02d-%02d"), RtcTime.day_of_month, RtcTime.month, RtcTime.year - 2000);
  }
  else if (Settings->display_cols[0] >= 6)
  {
    snprintf_P(line, sizeof(line), PSTR("%02d%02d%02d"), RtcTime.day_of_month, RtcTime.month, RtcTime.year - 2000);
  }
  else
  {
    snprintf_P(line, sizeof(line), PSTR("%02d%02d"), RtcTime.day_of_month, RtcTime.month);
  }
  TM1640Center(line);
}

void TM1640Refresh(void)
{ // Every second
  if (!disp_power || !Settings->display_mode)
  {
    AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s Every Second"), TM1640Data.model_name);
    return;
  } // Mode 0 is User text

  switch (Settings->display_mode)
  {
  case 1: // Time
    TM1640Time();
    break;
  case 2: // Date
    TM1640Date();
    break;
  case 3: // Time
    if (TasmotaGlobal.uptime % Settings->display_refresh)
    {
      TM1640Time();
    }
    else
    {
      TM1640Date();
    }
    break;
    /*
    case 4:  // Mqtt
      TM1640PrintLog();
      break;
    case 5: {  // Mqtt
      if (!TM1640PrintLog()) { TM1640Time(); }
      break;
    }
*/
  }
}

#endif // USE_DISPLAY_MODES1TO5

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdsp32(uint32_t function)
{
  bool result = false;
  if (FUNC_DISPLAY_INIT_DRIVER == function)
  {
    TM1640Init();
    AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s bool Xdsp32(uint32_t function) : %s"), TM1640Data.model_name, "TM1640Init done");
  }
  else if (TM1640Data.init_done && (XDSP_32 == Settings->display_model))
  {
    switch (function)
    {
    case FUNC_DISPLAY_EVERY_50_MSECOND:
      if (disp_power && !Settings->display_mode)
      {
        if (TM1640Data.scroll)
        {
          TM1640ScrollText();
        }
        if (TM1640Data.show_clock)
        {
          TM1640ShowTime();
        }
      }
      break;
#ifdef USE_DISPLAY_MODES1TO5
    case FUNC_DISPLAY_EVERY_SECOND:
      TM1640Refresh();
      break;
#endif // USE_DISPLAY_MODES1TO5
    case FUNC_DISPLAY_MODEL:
      result = true;
      break;
    case FUNC_DISPLAY_SEVENSEG_TEXT:
    case FUNC_DISPLAY_CLEAR:
    case FUNC_DISPLAY_NUMBER:
    case FUNC_DISPLAY_FLOAT:
    case FUNC_DISPLAY_NUMBERNC:
    case FUNC_DISPLAY_FLOATNC:
    case FUNC_DISPLAY_RAW:
    case FUNC_DISPLAY_DOTS:
    case FUNC_DISPLAY_LEVEL:
    case FUNC_DISPLAY_SEVENSEG_TEXTNC:
    case FUNC_DISPLAY_SCROLLTEXT:
    case FUNC_DISPLAY_SCROLLDELAY:
    case FUNC_DISPLAY_CLOCK:
      AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s FUNC_DISPLAY_CLOCK : %d"), TM1640Data.model_name, FUNC_DISPLAY_CLOCK);
      if (disp_power && !Settings->display_mode)
      {
        TM1640Data.show_clock = false;
        result = TM1640MainFunc(function);
      }
      break;
    case FUNC_DISPLAY_DIM:
      AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s FUNC_DISPLAY_DIM : %d"), TM1640Data.model_name, FUNC_DISPLAY_DIM);
      TM1640Dim();
      break;
    case FUNC_DISPLAY_POWER:
      AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s FUNC_DISPLAY_POWER : %d"), TM1640Data.model_name, FUNC_DISPLAY_POWER);
      if (!disp_power)
      {
        TM1640ClearDisplay();
        AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s bool Xdsp32(uint32_t function) : %s"), TM1640Data.model_name, "after TM1640ClearDisplay()");
      }
      break;
    }
  }
  return result;
}

#endif // USE_DISPLAY_TM1640
#endif // USE_DISPLAY

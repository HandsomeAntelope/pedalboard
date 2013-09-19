/**
 * Code for the custom made MIDI pedalBoard from Alex.
 *
 */
#include <Keypad.h>
#include <Wire.h>
#include <Deuligne.h>
#include <EEPROM.h>
#include <MIDI.h>

#define MENU_ENTER      0xA0
#define MENU_EXIT       0xA1

#define MENU_CONTINUE   0xB0
#define KEY_UP          0x81
#define KEY_DOWN        0x82
#define KEY_LEFT        0x83
#define KEY_RIGHT       0x80
#define KEY_ENTER       0x84

#define DEBUG

int serial_clock = 5;
int serial_data_out = 6;

int led_latch = 7;
int aux_disp_latch = 8;


/** display buffer. */
static char dispbuf[20];

/* KEYPAD SECTION */
const byte ROWS = 3;		//three rows
const byte COLS = 5;		//five columns
char keys[ROWS][COLS] = {
    { 1,  2,  3,  4,  5},
    { 6,  7,  8,  9, 10},
    {11, 12, 13, 14, 15},
};

byte rowPins[ROWS] = { 2, 3, 4 };				//connect to the row pinouts of the keypad
byte colPins[COLS] = { 13, 12, 11, 10, 9};		//connect to the column pinouts of the keypad

Keypad keypad = Keypad (makeKeymap (keys), rowPins, colPins, ROWS, COLS);

static Deuligne lcd;

byte midi_channel = 1;

static uint16_t led_state = 0;

/** Switch configuration data */
struct button_data
{
    int key;
    int number;
    int state;
    char type;
} cc_data;

#define EXPRESSION_COUNT 4
#define EXPR_DATA_LEN    1
#define EXPR_DATA_OFFSET 32	//(16buttons * 2)

struct expression_data
{
    int pin;
    int value;
    int cc;
} exp_data[EXPRESSION_COUNT] =
{
  {A1, 0, 0},
  {A2, 0, 0},
  {A3, 0, 0},
  {A4, 0, 0},
};

struct button_data buttons_config[15];

static char *menu_items[] = {
  "Midi Chnl",
  "Brightness"
};

#define MENU_ITEM_COUNT (sizeof(menu_items) / sizeof(char *))

#define STATE_RUN 0
#define STATE_MENU 1

int patch = 0;
int bank = 0;

uint16_t leds_state;
int (*current_menu) (int) = NULL;

void
load_expression_data (int expr)
{
    exp_data[expr].cc = EEPROM.read (EXPR_DATA_OFFSET + expr * EXPR_DATA_LEN);
    //exp_data[expr].cc = EEPROM.read(EXPR_DATA_OFFSET + i*EXPR_DATA_LEN);
}

void
load_button_data (int key)
{
#ifdef DEBUG
    Serial.print ("Load data for: ");
    Serial.print (key);
    Serial.print (EEPROM.read (key * 2), HEX);
    Serial.print (" ");
    Serial.println (EEPROM.read (key * 2 + 1), HEX);
#endif
    cc_data.key = key;
    cc_data.type = EEPROM.read (key * 2);
    if (cc_data.type != 'P' && cc_data.type != 'C')
        cc_data.type = 'P';
    cc_data.number = EEPROM.read (key * 2 + 1);
    if (cc_data.number > 127 || cc_data.number < 0)
        cc_data.number = 0;
}

void
save_data (int key)
{
#ifdef DEBUG
    Serial.print ("Save data for: ");
    Serial.print (key);
    Serial.print (cc_data.type, HEX);
    Serial.print (" ");
    Serial.println (cc_data.number, HEX);
#endif
    EEPROM.write (key * 2, cc_data.type);
    EEPROM.write (key * 2 + 1, cc_data.number);
    buttons_config[key - 1] = cc_data;
}

void
setup ()
{
    Wire.begin ();
    lcd.init ();

    lcd.clear ();
    lcd.backLight (true);
    lcd.setCursor (0, 0);
    lcd.print ("Pedalboard 1.0");
    delay (1000);
    
    MIDI.begin ();

    for (int i = 1; i < 16; i++) {
        load_button_data (i);
        buttons_config[i - 1] = cc_data;
    }

    for (int i = 0; i < EXPRESSION_COUNT; i++) {
        load_expression_data (i);
    }

    lcd.clear ();
    MIDI.setInputChannel (MIDI_CHANNEL_OMNI);
    MIDI.turnThruOff ();
    MIDI.setHandleClock (handleClock);
    pinMode(serial_clock, OUTPUT);      
    pinMode(serial_data_out, OUTPUT);      
    pinMode(aux_disp_latch, OUTPUT);      
#define PULSE 1
    digitalWrite(serial_clock, !PULSE);
}

void
handleClock (void)
{
    static int clock = 0;

    lcd.setCursor (0, 0);
    lcd.print (clock++);
}

int
select_cc_menu (int key)
{
    struct button_data *data = &cc_data;

    Serial.print ("select_cc_menu: ");
    Serial.println (key);

    memset (dispbuf, ' ', 16);
    switch (key)
    {
    case MENU_ENTER:
		/** Init data */
      break;
    case KEY_UP:
      data->number++;
      if (data->number > 127)
	    data->number = 127;
      break;
    case KEY_DOWN:
      data->number--;
      if (data->number < 0)
	    data->number = 0;
      break;
    case KEY_LEFT:
    case KEY_RIGHT:
      data->type = data->type == 'P' ? 'C' : 'P';
      break;
    case KEY_ENTER:
      save_data (data->key);
      return MENU_EXIT;
    default:
      if (key > 0 && key < 0x80) load_button_data (key);
    }
    snprintf (dispbuf, 16, "%cC# %3d         ", data->type, data->number);
    lcd.setCursor (0, 1);
    lcd.print (dispbuf);
    Serial.println ("select_cc_menu exit CONTINUE");
    return MENU_CONTINUE;
}

int
main_menu (int key)
{
  int ret = MENU_CONTINUE;
  switch (key)
    {
    case MENU_ENTER:
      cc_data.key = -1;
      lcd.clear ();
      lcd.setCursor (0, 0);
      lcd.print ("Select key      ");
      break;

    default:
      if (key < 0x80 && key > 0)
	{
	  cc_data.key = key;
	  ret = select_cc_menu (key);
	}
      else if (cc_data.key != -1)
	{
	  ret = select_cc_menu (key);
	  Serial.print ("return: ");
	  Serial.println (ret);
	  if (ret == MENU_EXIT)
	    return MENU_EXIT;
	}
      break;
    }

  if (cc_data.key != -1)
    {
      snprintf (dispbuf, 16, "Button: %2d", cc_data.key);
    }
  else
    {
      sprintf (dispbuf, "Button: <Press>");
    }
  lcd.setCursor (0, 0);
  lcd.print (dispbuf);
  return ret;
}

int
run_menu (int key)
{
  if (current_menu == NULL)
    current_menu = main_menu;

  return current_menu (key);
}

int
getKey ()
{
    static int8_t oldkey = -1;
    char key = keypad.getKey ();

    if (key != 0) {
        Serial.println (key);
        return key;
    }

    key = lcd.get_key ();		// read the value from the sensor & convert into key press

    if (key != oldkey) {
        delay (50);		// wait for debounce time
        key = lcd.get_key ();	// read the value from the sensor & convert into key press
        if (key != oldkey) {
            oldkey = key;
            if (key >= 0) {
    	        return key + 0x80;
            }
	    }
    }
    return -1;
}

void
handle_analog ()
{
    for (int i = 0; i < 1/*EXPRESSION_COUNT*/; i++) {
        int newValue = analogRead (exp_data[i].pin);
        Serial.print("a"); Serial.print(i); Serial.print("=");Serial.println(newValue);
        if (exp_data[i].value != newValue) {
            exp_data[i].value = newValue;
            lcd.setCursor (10, 0);
            sprintf (dispbuf, "E%d:%3d",
                    i, newValue);
            lcd.print(dispbuf);
            MIDI.sendControlChange (exp_data[i].cc,
                    exp_data[i].value, midi_channel);
	    }
    }
}

void
handle_button (struct button_data *d)
{
    if (d->type == 'P') {
        MIDI.sendProgramChange (d->number, midi_channel);
    } else if (d->type == 'C') {
        MIDI.sendControlChange (d->number, d->state == 0 ? 1 : 0, midi_channel);
    }
}
  static int offset = 0;

void
loop ()
{
    static int state = STATE_RUN;
    int key = getKey ();

    if (key != -1) {
        Serial.print ("Key: ");
        Serial.println (key);
      if (key == KEY_UP) {
         offset ++; 
        if (offset > 15) offset = 0;
      } 
    }

    switch (state) {
    case STATE_RUN:
        if (key == KEY_ENTER) {
            state = STATE_MENU;
            run_menu (MENU_ENTER);
            lcd.clear ();
            break;
        } else if (key > 0 && key < 0x80) {
            patch = key;
            lcd.setCursor (10, 0);
            sprintf (dispbuf, "%cC:%3d",
                    buttons_config[key - 1].type,
                    buttons_config[key - 1].number);
            lcd.print (dispbuf);
        }
        lcd.setCursor (0, 0);
        lcd.print ("RUN ");
        lcd.setCursor (0, 1);
        snprintf (dispbuf, 16, "B%02d P%02d", bank, patch);
        lcd.print (dispbuf);
        aux_disp_print(dispbuf);
        break;
    case STATE_MENU:
        if (MENU_EXIT == run_menu (key)) {
            state = STATE_RUN;
            lcd.clear ();
        }
        break;
    }

    handle_analog ();
    MIDI.read ();
}

void shiftout(uint8_t data) {
    for (int i =0; i < 8; i++) {
        digitalWrite(serial_data_out, (data & 0x80) ? HIGH : LOW);
        digitalWrite(serial_clock, PULSE);
        digitalWrite(serial_clock, !PULSE);
        data <<= 1;
    }
}

void latch_aux_disp() 
{
    digitalWrite(aux_disp_latch, 1);
    digitalWrite(aux_disp_latch, 0);
}

void update_leds() {

    /*latch*/
    digitalWrite(led_latch, 1);
    digitalWrite(led_latch, 0);
}

int set_led(int led) {
    led_state |= (1<<led);    
    update_leds();
}

int clear_led(int led) {
    led_state &= ~(1<<led);
    update_leds();    
}


void aux_disp_print(char * str) {
  Serial.print("Offset: "); Serial.println(offset);
  for (int i = 0; i < 8; i++) {

        uint16_t tmp = getdata(str[i]);
        shiftout(tmp & 0xff);    
        shiftout(tmp >> 8);
    }
    latch_aux_disp();
}





uint16_t getdata(char c) {        
    uint16_t result = 0;

    switch (c) {
        case ' ': result = 0b0000000000000000; break;
        case '0': result = 0b0010010000111111; break;
        case '1': result = 0b0000000000000110; break;
        case '2': result = 0b0000000011011011; break;
        case '3': result = 0b0000000010001111; break;
        case '4': result = 0b0001001011100000; break;
        case '5': result = 0b0000000011101101; break;
        case '6': result = 0b0000000011111101; break;
        case '7': result = 0b0000000000000111; break;
        case '8': result = 0b0000000011111111; break;
        case '9': result = 0b0000000011101111; break;
        case 'A': result = 0b0000000011110111; break;
        case 'B': result = 0b0001001010001111; break;
        case 'C': result = 0b0000000000111001; break;
        case 'D': result = 0b0001001000001111; break;
        case 'E': result = 0b0000000001111001; break;
        case 'F': result = 0b0000000001110001; break;
        case 'G': result = 0b0000000010111101; break;
        case 'H': result = 0b0000000011110110; break;
        case 'I': result = 0b0001001000001001; break;
        case 'J': result = 0b0000000000011110; break;
        case 'K': result = 0b0000110001110000; break;
        case 'L': result = 0b0000000000111000; break;
        case 'M': result = 0b0000010100110110; break;
        case 'N': result = 0b0000100100110110; break;
        case 'O': result = 0b0000000000111111; break;
        case 'P': result = 0b0000000011110011; break;
        case 'Q': result = 0b0000100000111111; break;
        case 'R': result = 0b0000100011110011; break;
        case 'S': result = 0b0000000011101101; break;
        case 'T': result = 0b0001001000000001; break;
        case 'U': result = 0b0000000000111110; break;
        case 'V': result = 0b0010010000110000; break;
        case 'W': result = 0b0010100000110110; break;
        case 'X': result = 0b0010110100000000; break;
        case 'Y': result = 0b0001010100000000; break;
        case 'Z': result = 0b0010010000001001; break;
        case '*': result = 0b0011111111000000; break;
    }
    return (result << 10 & 0xfc00) | (result >> 6) & 0x03ff;
}



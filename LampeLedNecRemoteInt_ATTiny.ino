#include "Arduino.h"

#if defined (__AVR_ATtiny45__) || (__AVR_ATtiny85__)

#include "PinChangeInterruptSimple.h"         // mimics attachInterrupt() but a PCI for ATtiny

#define SDI PB0 // GREEN ON LED CHAIN
#define CLK PB2 // BLUE ON LED CHAIN
#define IR_PIN PB4 // IR SENSOR
#define MES_PIN PB3 // MESURE
#define DEBUG_PIN PB1 // DEBUG
#define BIT_DUR 880    // Full bit Duration Infrared reciever
#define LATCH_DELAY 65 // LED STRIP RESET
#endif

#if defined (__AVR_ATmega328P__)
const int CLK = 4;
const int SDI = 5;
const int IR_PIN = 2;
const int IR_PIN_INT = INT0;
const int BIT_DUR = 888;    // Full bit Duration Infrared reciever
const int MES_PIN = 3;          // MESURE
const int DEBUG_PIN = 6;        // DEBUG
const int LATCH_DELAY = 650;    // LED STRIP RESET
#endif



#define STRIP_LENGTH 7 // LEDs on this strip
#define LEDS_BYTES STRIP_LENGTH*3

long strip_colors[STRIP_LENGTH];
int first_ptr = 0;
byte colors[LEDS_BYTES];
byte programm = 1;
byte last_programm = 1;
#define NB_COLORS 6
const long default_colors[] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFF00 };
const byte key_codes[] = {  69,  70,  71,  68,  64,  67,   7,  21,   9,  22,  25,  13,  12,  24,  94,   8,  28,  90,  66,  82, 74  };
const char key_chars[] = { 'A', 'B', 'C', 'D', 'E', 'F', '-', '+', '=', '0', '*', '#', '1', '2', '3', '4', '5', '6', '7', '8', '9' };

char convKey(byte key_code) {
  for(byte b=0;b<sizeof key_codes;b++){
    if(key_code==key_codes[b]) return key_chars[b];
  }
  return 0;
}

//------------------------------------- Télécommande IR -------------------------------------------

volatile int ir_key = -1;

// MAX 180 us on ATTiny @ 1MHz
// 18 us on ATMega @ 16MHz
void irRemote_ISR() {
  static unsigned long irq_micros = 0;
  static byte address = 0;
  static byte command = 0;
  static byte buffer = 0;
  static byte error = 0;
  static char bit_cpt = -2;
  unsigned long us = micros();
  digitalWrite(MES_PIN, HIGH);

  unsigned long elapsed = us - irq_micros;
  irq_micros = us;

  if(elapsed>6750) {
    bit_cpt = -1;
  }
  else if(bit_cpt==-1 && digitalRead(IR_PIN)==LOW) {
    if(elapsed>3500) {
      bit_cpt = 0;
      error = 0;
    }
  }
  else if(bit_cpt>=0 && bit_cpt<32) {
    if(digitalRead(IR_PIN)==LOW) {
       buffer >>= 1;
       if(elapsed>1125) buffer |= 0x80;
       bit_cpt++;
       if(bit_cpt==8) {
         address = buffer;
       }
       else if(bit_cpt==16) {
         buffer = ~buffer;
         if(buffer!=address) error++;
       }
       else if(bit_cpt==24) {
         command = buffer;
       }
       else if(bit_cpt==32) {
         buffer = ~buffer;
         if(buffer!=command) error++;
         if(error==0) ir_key = command;
       }       
    }
  }
/*  if(elapsed > (BIT_DUR<<4)) { // FIRST PULSE > 16 BIT_DUR
      bit_cpt = 0;
      buffer = 0;
  }
  else if(elapsed > BIT_DUR+(BIT_DUR>>1)) { // LONG PULSE > 1.5 BIT_DUR
      bit_cpt++;    
  }
  bit_cpt++;

  if(bit_cpt&1) {
    buffer <<= 1;
    if(digitalRead(IR_PIN)==LOW) { // RISING (Logique inversée)
      buffer |= 1;
    }
  }

  if(bit_cpt==27) { 
    byte bh = buffer>>8;
    if(old_buffer_h != bh) { // evite les répétitions de touche
      ir_key = buffer & 0x1FF;
      old_buffer_h = bh;
    }
  }
  // il peut y avoir une interruption supplémentaire (bit_cpt==28) pour les touches finissant par 1
*/
  digitalWrite(MES_PIN, LOW);
  return;
}

//------------------------------------- LED Strip -------------------------------------------------


//Takes the current strip color array and pushes it out
void post_frame (void) {
  int ptr = first_ptr * 3;
  for(int i=0;i<LEDS_BYTES;i++) {    
     byte b = colors[ptr];
     for(int j=0;j<8;j++) {
      digitalWrite(CLK, LOW); //Only change data when clock is low
      digitalWrite(SDI, b & 0x80 ? HIGH : LOW);      
      digitalWrite(CLK, HIGH); //Data is latched when clock goes high
      b = b << 1;
    }
    
    ptr++;
    if(ptr==LEDS_BYTES) ptr = 0;    
  }

  //Pull clock low to put strip into reset/post mode
  digitalWrite(CLK, LOW);
  delayMicroseconds(LATCH_DELAY); //Wait for 500us to go into reset
  //noInterrupts();
  //interrupts();
}

void scrollUp(void) {
  first_ptr++;
  if(first_ptr==STRIP_LENGTH) first_ptr = 0;
}

void scrollDown(void) {
  if(first_ptr==0) first_ptr = STRIP_LENGTH;
  first_ptr--;
}

long rgbToLong(long r, long g, long b) {
  return (r<<16) | (g<<8) | b;
}

long hsv2rgb(unsigned int hue, unsigned int sat, unsigned int val) {
    unsigned int i = hue/60;
    unsigned int bottom = ((255 - sat) * val)>>8;
    unsigned int top = val;
    byte rising  = ((top-bottom)  *(hue%60   )  )  /  60  +  bottom;
    byte falling = ((top-bottom)  *(60-hue%60)  )  /  60  +  bottom;   
    switch(i) {
        case 0: return rgbToLong(top, rising, bottom);
        case 1: return rgbToLong(falling, top, bottom);
        case 2: return rgbToLong(bottom, top, rising);
        case 3: return rgbToLong(bottom, falling, top);
        case 4: return rgbToLong(rising, bottom, top);
        case 5: return rgbToLong(top, bottom, falling);
    }
}

void setLedColor(int led, long color) {
  int ptr = led * 3;
  colors[ptr++] = (color >> 16) & 0xFF;
  colors[ptr++] = (color >> 8) & 0xFF;
  colors[ptr++] = color & 0xFF;
  //strip_colors[led] = (r<<16) | (g<<8) | b;
}
/*
long getLedColor(int led) {
  int ptr = led * 3;
  long color;
  color = colors[ptr++];
  color <<= 8;
  color |= colors[ptr++];
  color <<= 8;
  color |= colors[ptr++];
  return color;
}
*/

void setup() {
  #if defined (__AVR_ATmega328P__)
  Serial.begin(9600);
  #endif
  pinMode(SDI, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(MES_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT);
  pinMode(DEBUG_PIN, OUTPUT);
  digitalWrite(MES_PIN, LOW);
  digitalWrite(CLK, LOW); 
  digitalWrite(DEBUG_PIN, LOW);
  
  #if defined (__AVR_ATmega328P__)
  attachInterrupt(IR_PIN_INT, irRemote_ISR, CHANGE);
  #endif
  #if defined (__AVR_ATtiny45__) || (__AVR_ATtiny85__)
  attachPcInterrupt(IR_PIN, irRemote_ISR, CHANGE);
  #endif
  
}

void setStripColor(unsigned long color) {
  for(int i=0;i<STRIP_LENGTH;i++) {
    setLedColor(i, color);
  }
}

void hueRotator() {
  static int hue = 0;
  hue++;
  if(hue==360) hue = 0;
  setStripColor(hsv2rgb(hue, 255, 255));   
}

void disco() {
  if(programm!=last_programm) {
    for(int i=0;i<STRIP_LENGTH;i++) {
      setLedColor(i, default_colors[random(NB_COLORS)]);
    }
  }
  scrollUp();
}

void disco2() {
  static int hue = 0;
  const int div = 4;
  static int cpt = div;
  if(cpt--!=0) return;
  cpt = div;
  hue++;
  if(hue==NB_COLORS) hue = 0;
  setStripColor(default_colors[hue]);
}

void loop() {   
  int key = ir_key;
  ir_key = -1;

  switch(programm) {
    case 1:
      setStripColor(0xFFFFFF);
      break;
    case 2:
      setStripColor(0xFF0000);
      break;
    case 3:
      setStripColor(0x00FF00);
      break;
    case 4:
      setStripColor(0x0000FF);
      break;
    case 5:
      setStripColor(0xFF00FF);
      break;
    case 6:
      setStripColor(0x00FFFF);
      break;
    case 7:
      setStripColor(0xFFFF00);
      break;
    case 8: 
      hueRotator();
      break;    
    case 9: 
      disco();
      break;    
    case 0: 
      disco2();
      break;    
    default: // noir
      setStripColor(0x000000);    
  }

  last_programm = programm;
  
  if(key>=0) {
    programm = convKey(key) - '0';
  }

  post_frame();
  delay(112);

}



/* 
 * Author: Adrian Jimenez <victoradrianjimenez@gmail.com>
 */

#include "SoftPWM.h"
#include <EEPROM.h>

#define NUM_LEDS 6      // 1 to 6
#define NUM_BUTTONS 1   // 0 to 2

#define BUTTON_TIME 500
#define LOOP_DELAY 0
#define FADE_TIME 0
#define MAX_LED_LUM 16

#define NUM_PROGRAMS 10  // 1 to N
#define MEM_ADDRESS_1 0   // 0 to 1024

byte c_hls[NUM_LEDS][3];
byte c_pos[NUM_LEDS];
byte c_program = 0;
byte c_i = 0;
byte n_rgb[3];
unsigned long time_count_mseg, time_count = 0, time_count_2 = 0;
unsigned long last_time_count[NUM_BUTTONS];
byte r, g, b;
float lum_s;


/********************************************************************
 * Configuracion
 ********************************************************************/
void setup() {

  //Serial.begin(9600);
  byte i;

  //Disable interrupts
  cli();

  /*** IRQ Buttons ***/

  #if NUM_BUTTONS > 0
    pinMode(2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(2), programSelector1, FALLING);
  #endif
  #if NUM_BUTTONS > 1
    pinMode(3, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(3), programSelector2, FALLING);
  #endif
  pinMode(A6, INPUT);
  pinMode(A7, INPUT);
  
  /*** TIMER ***/

  //set timer0 interrupt at 0.5kHz
  TCNT0  = TCCR0A = TCCR0B = 0;
  // set compare match register for 2khz increments
  OCR0A = 124; // = (16*10^6) / (500*256) - 1 (must be <256)
  // turn on CTC mode
  TCCR0A |= (1 << WGM01);
  // Set CS01 and CS00 bits for 256 prescaler
  TCCR0B |= (1 << CS02) | (0 << CS00); 
  // enable timer compare interrupt
  TIMSK0 |= (1 << OCIE0A);


  /*** PWM ***/

  // initialize
  SoftPWMBegin();
  // create and set pin i to 0 (off)
  for (i=0; i<=1; i++){
    SoftPWMSet(i, 0);
  }
  for (i=4; i<=13; i++){
    SoftPWMSet(i, 0);
  }
  for (i=0; i<=5; i++){
    SoftPWMSet(A0 + i, 0);
  }
  // set fade time for all pins to x ms
  SoftPWMSetFadeTime(ALL, FADE_TIME, FADE_TIME);

  //aumento velocidad PWM (al limite)
  OCR2A = 53; //>= 53
  TCCR2B = (TCCR2B & 0xF8) | (0 << CS22) | (1 << CS21) | (0 << CS20) ; //prescaler 8

  /*** PROGRAM ***/  

  c_program = EEPROM.read(MEM_ADDRESS_1) % NUM_PROGRAMS;
  init_program();
  
  /*** RANDOM ***/
  
  //randomSeed(analogRead(7));
  
  //allow interrupts
  sei();
}

/********************************************************************
 * Programa principal
 ********************************************************************/
void loop(){
  //lectura de potenciometro externo (si esta disponible)
  //delay(LOOP_DELAY);
}

/********************************************************************
 * ISR pin 2, selector del programa
 * Parametros:
 * - last_time_count (global): momento desde ultima pulsación.
 * - time_count_mseg (global): cuenta milisegundos.
 * - c_program (global): numero de programa.
 ********************************************************************/
void programSelector1(){
  //evitar efecto rebote, ignoro si no pasan 500ms entre presionadas
  if (time_count_mseg - last_time_count[0] > BUTTON_TIME){
    //calculo el id del proximo programa
    c_program = (c_program + 1) % NUM_PROGRAMS; 
    //inicializar nuevo programa
    init_program();
    //Serial.println(c_program);
    //guardo en memoria eeprom (last code line)
    EEPROM.write(MEM_ADDRESS_1, c_program);
    last_time_count[0] = time_count_mseg;
  }
}

/********************************************************************
 * ISR pin 3, selector secundario
 ********************************************************************/
#if NUM_BUTTONS > 1
void programSelector2(){
}
#endif

/********************************************************************
 * Inicializar programa al seleccionarlo.
 * Parametros:
 * - c_program (global): numero de programa.
 ********************************************************************/
void init_program(){
  switch(c_program){
    case 1:
      init_program_1(); break;
    case 2:
      init_program_2(); break;
    case 3:
      init_program_5(); break;
    case 4: case 5: case 6: case 7: case 8: case 9:
      init_program_4(); break;
    default: 
      init_program_0();
  }
}

/********************************************************************
 * timer0 interrupt 1kHz
 * Parametros:
 * - c_program (global): numero de programa.
 * - time_count_mseg (global): cuenta milisegundos.
 ********************************************************************/
ISR(TIMER0_COMPA_vect){
  switch (c_program){
    case 1: step_program_1(MAX_LED_LUM, 255); break;
    case 2: step_program_2(MAX_LED_LUM, 255); break;
    case 3: step_program_5(MAX_LED_LUM); break;
    
    //Lum variable, diferentes para cada led
    case 4: step_program_4(0, 255); break; //rojo
    case 5: step_program_4(43, 255); break; //amarillo
    case 6: step_program_4(85, 255); break; //verde
    case 7: step_program_4(128, 255); break; //celeste
    case 8: step_program_4(170, 255); break; //azul
    case 9: step_program_4(212, 255); break; //morado
    
    //case 10 step_program_0(255); break;
    default: step_program_0(MAX_LED_LUM);
  }
  time_count_mseg+=2;
}

/********************************************************************
 * Actualizar color de todos los leds RGB.
 * Parametros:
 * - c_i (global): iterador de leds.
 * - c_hls (global): vector de valores HLS (uno por led).
 * - n_rgb (global): nuevo color RGB de un led.
 ********************************************************************/
void update_leds(byte n){
  to_RGB(c_hls[n][0], c_hls[n][1], c_hls[n][2]); //, r, g, b);
  //D2 y D3 reservado para interrupciones (boton externo)
  //A6 y A7 no pueden usarse como pines digitales en Nano
  switch (n){
    case 0:
      SoftPWMSet(0, r); 
      SoftPWMSet(1, g);
      SoftPWMSet(4, b);
      break;
    case 1:
      SoftPWMSet(5, r);
      SoftPWMSet(6, g);
      SoftPWMSet(7, b);
      break;
    case 2:
      SoftPWMSet(8, r);
      SoftPWMSet(9, g);
      SoftPWMSet(10, b);
      break;
    case 3:
      SoftPWMSet(11, r);
      SoftPWMSet(12, g);
      SoftPWMSet(13, b);
      break;
    case 4:
      SoftPWMSet(A0, r);
      SoftPWMSet(A1, g);
      SoftPWMSet(A2, b);
      break;
    case 5:
      SoftPWMSet(A3, r);
      SoftPWMSet(A4, g);
      SoftPWMSet(A5, b);
      break;
  }
}

/********************************************************************
 * HLS to RGB
 * Convierte un vector en el espacio de color HLS en el espacio de color RGB
 ********************************************************************/
void to_RGB(byte h, byte l, byte s){ //, byte &r, byte &g, byte &b){
  switch ((int)(h / 42.5)){
  case 0:
    if (l < 128){
      b = l * (1 - s / 255.0);
      r = 2 * l - b;
    }
    else{
      r = l * (1 - s / 255.0) + s;
      b = 2 * l - r;
    }
    g = b - h * (b - r) / 42.5;
    break;
  case 1:
    if (l < 128){
      b = l * (1 - s / 255.0);
      g = 2 * l - b;
    }
    else{
      g = l * (1 - s / 255.0) + s;
      b = 2 * l - g;
    }
    r = g - (h - 42.5) * (g - b) / 42.5;
    break;
  case 2:
    if (l < 128){
      r = l * (1 - s / 255.0);
      g = 2 * l - r;
    }
    else{
      g = l * (1 - s / 255.0) + s;
      r = 2 * l - g;
    }
    b = r - (h - 85.0) * (r - g) / 42.5;
    break;
  case 3:
    if (l < 128){
      r = l * (1 - s / 255.0);
      b = 2 * l - r;
    }
    else{
      b = l * (1 - s / 255.0) + s;
      r = 2 * l - b;
    }
    g = b - (h - 127.5) * (b - r) / 42.5;
    break;
  case 4:
    if (l < 128){
      g = l * (1 - s / 255.0);
      b = 2 * l - g;
    }
    else{
      b = l * (1 - s / 255.0) + s;
      g = 2 * l - b;
    }
    r = g - (h - 170.0) * (g - b) / 42.5;
    break;
  default:
    if (l < 128){
      g = l * (1 - s / 255.0);
      r = 2 * l - g;
    }
    else{
      r = (l * (1 - s / 255.0)) + s;
      g = 2 * l - r;
    }
    b = r - (h - 212.5) * (r - g) / 42.5;
  }
}

/**
 * Parametros de programas:
 * - time_count (global): contador de milisegundos.
 * - c_pos[N] (global): parametro usado por cada led.
 */

/********************************************************************
 * PROGRAM 0 (default)
 * Luz blanca. 
 * Lum ajustable.
 ********************************************************************/
void init_program_0(){
  for(c_i=0; c_i<NUM_LEDS; c_i++){
    c_hls[c_i][0] = 0;
    c_hls[c_i][1] = 0;
    c_hls[c_i][2] = 0;
    update_leds(c_i);
  }
  time_count = 0;
}
void step_program_0(byte lum){
  //si paso un periodo de X milisegundos
  if (time_count == 0){
    for(c_i=0; c_i<NUM_LEDS; c_i++){
      c_hls[c_i][0] = 0;
      c_hls[c_i][1] = lum;
      c_hls[c_i][2] = 0;
      update_leds(c_i);
    }
    time_count = 1;
  }
}

/********************************************************************
 * PROGRAM 1
 * - Hue variable, diferente para cada led (periodo de 30 seg).
 * - Lum ajustable.
 * - Sat fija.
 ********************************************************************/
void init_program_1(){
  for(c_i=0; c_i<NUM_LEDS; c_i++){
    c_hls[c_i][0] = 255.0 / NUM_LEDS * c_i;
    c_hls[c_i][1] = 0;
    c_hls[c_i][2] = 0;
    update_leds(c_i);
  }
  time_count = 0;
}
void step_program_1(byte lum, byte sat){
  for(c_i=0; c_i<NUM_LEDS; c_i++){
    if (time_count == c_i){
      c_hls[c_i][0]++;
      c_hls[c_i][1] = lum;
      c_hls[c_i][2] = sat;
      update_leds(c_i);
    }
  }
  time_count = (time_count + 1) % 10; //5000÷2÷256
}


/********************************************************************
 * PROGRAM 2
 * - Hue variable, igual para cada led (periodo de 30 seg).
 * - Lum ajustable.
 * - Sat fija.
 ********************************************************************/
void init_program_2(){
  for(c_i=0; c_i<NUM_LEDS; c_i++){
    c_hls[c_i][0] = 0;
    c_hls[c_i][1] = 0;
    c_hls[c_i][2] = 0;
    update_leds(c_i);
  }
  time_count = 0;
}
void step_program_2(byte lum, byte sat){
  for(c_i=0; c_i<NUM_LEDS; c_i++){
    if (time_count == c_i){
      c_hls[c_i][0]++;
      c_hls[c_i][1] = lum;
      c_hls[c_i][2] = sat;
      update_leds(c_i);
    }
  }
  time_count = (time_count + 1) % 10; //5000÷2÷256
}

/********************************************************************
 * PROGRAM 4
 * - Hue ajustable.
 * - Lum variable, diferentes para cada led (periodo de 30 seg).
 * - Sat fijo.
 ********************************************************************/
void init_program_4(){
  for(c_i=0; c_i<NUM_LEDS; c_i++){
    c_pos[c_i] = 255.0 / NUM_LEDS * c_i; //lum varying [160-255]
    c_hls[c_i][0] = 0;
    c_hls[c_i][1] = 0;
    c_hls[c_i][2] = 0;
    update_leds(c_i);
  }
  time_count = 0;
}
void step_program_4(byte hue, byte sat){
  for(c_i=0; c_i<NUM_LEDS; c_i++){
    if (time_count == c_i){
      lum_s = sin(c_pos[c_i]++ * 0.024543693) - 0.5; //2*pi/256
      c_hls[c_i][0] = hue;
      c_hls[c_i][1] = MAX_LED_LUM * ((lum_s > 0 )?lum_s:0);
      c_hls[c_i][2] = sat;
      update_leds(c_i);
    }
  }
  time_count = (time_count + 1) % 10; //5000÷2÷256
}

/********************************************************************
 * PROGRAM 5
 * - Hue variable, un LED a la vez haciendo circulo.
 * - Lum ajustable.
 * - Sat fija.
 ********************************************************************/
void init_program_5(){
  for(c_i=0; c_i<NUM_LEDS; c_i++){
    c_hls[c_i][0] = 0; //hue in range [0-255]
    c_hls[c_i][1] = 0;
    c_hls[c_i][2] = 255;
    c_pos[c_i] = 255.0 / NUM_LEDS * c_i; //rotatios range [0-255]
    update_leds(c_i);
  }
  time_count = time_count_2 = 0;
}
void step_program_5(byte lum){
  for(c_i=0; c_i<NUM_LEDS; c_i++){
    if (time_count == c_i){
      lum_s = (sin(c_pos[c_i] * 0.024543693) - 0.5) * 2; //2*pi/256
      c_hls[c_i][0]++;
      c_hls[c_i][1] = MAX_LED_LUM * ((lum_s > 0 )?lum_s:0);
      c_hls[c_i][2] = 255;
      update_leds(c_i);
    }
    if (time_count_2 == c_i){
      c_pos[c_i]++;
    }
  }
  time_count = (time_count + 1) % 10; //5000÷2÷256
  //completo un giro en 10 segundos (39=10000/255)
  time_count_2 = (time_count_2 + 1) % 19; //10000÷2÷256
}

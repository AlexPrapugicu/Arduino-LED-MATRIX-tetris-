#include <bitswap.h>
#include <chipsets.h>
#include <color.h>
#include <colorpalettes.h>
#include <colorutils.h>
#include <controller.h>
#include <cpp_compat.h>
#include <dmx.h>
#include <FastLED.h>
#include <fastled_config.h>
#include <fastled_delay.h>
#include <fastled_progmem.h>
#include <fastpin.h>
#include <fastspi.h>
#include <fastspi_bitbang.h>
#include <fastspi_dma.h>
#include <fastspi_nop.h>
#include <fastspi_ref.h>
#include <fastspi_types.h>
#include <hsv2rgb.h>
#include <led_sysdefs.h>
#include <lib8tion.h>
#include <noise.h>
#include <pixelset.h>
#include <pixeltypes.h>
#include <platforms.h>
#include <power_mgt.h>

#include <SoftwareSerial.h>
//#include "FONTS.h"

// NISTE DEFINEURI PENTRU DIRECTII
#define TOUP 1
#define TODOWN 2
#define TOLEFT 3
#define TORIGHT 4

#define FIELD_WIDTH 8
#define FIELD_HEIGHT 8

SoftwareSerial SQUISH(10,11);   // MODUL BLUETOOTH
// nu mere pe pinii 0 si 1 cu SoftwareSerila

const uint8_t LEDNUM = FIELD_WIDTH*FIELD_HEIGHT;

#define BRICK_SIZE 4        // dim max pentru piese, pot fi si mai mici
#define BRICK_OFFSET -1     // asta e offset pt piese spawnate nou
  
#define INITIAL_SPEED 500   // mic delay intre dropul pieselor
#define SPEED_INCREMENT 70  // cu atat creste viteza pieselor 
#define LEVEL_UP 2          // atatea randuri pana la nivel nou

// uint8_t - int pe 8 biti, ok pt memorie, nu consuma 4 bytes ca un int normal
// se putea folosi si byte dar asta pare mai intuitiv

typedef struct FIELD
{
  uint8_t pixels[FIELD_WIDTH][FIELD_HEIGHT+1];  // +1 folosit pt coliziunea pieselor de jos  
  unsigned long color[FIELD_WIDTH][FIELD_HEIGHT];
}FIELD;

FIELD field;  // aici e declarat terenul

typedef struct BRICK
{
  boolean enabled;  // piese e !enabled cand e asezata
  int POS_X;
  int POS_Y;
  int YOFFSET;      // pt plasament in capatul de sus al fieldului
  uint8_t Brick_size;
  uint8_t pixels[FIELD_WIDTH][FIELD_HEIGHT];
  unsigned int long color;
}BRICK;

BRICK activeBrick;  // aici e declarata o piesa activa

typedef struct RandomBrick
{
  int YOFFSET;  // la fel ca in structura BRICK
  uint8_t Brick_size;
  uint8_t pixels[FIELD_WIDTH][FIELD_HEIGHT];
  unsigned int long color;
}RandomBrick;

// DEFINEURI PENTRU BUTOANE BLUETOOTH
 #define  BUTTON_NOPE  0 
 #define  BUTTON_UP    1 
 #define  BUTTON_DOWN  2 
 #define  BUTTON_LEFT  3 
 #define  BUTTON_RIGHT  4 
 #define  BUTTON_START  5 
 #define  BUTTON_EXIT  6


// DE MODIFICAT CU HEX PENTRU CULORILE PE CARE LE VREI
// aici definite culorile pentru piese
#define  GREEN  0x008000 
#define  RED    0xFF0000 
#define  BLUE   0x0000FF 
#define  YELLOW 0xFFFF00 
#define  CHOCOLATE  0xD2691E 
#define  PURPLE 0xFF00FF 
#define  WHITE  0XFFFFFF
#define  AQUA   0x00FFFF
#define  HOTPINK 0xFF1493
#define  DARKORANGE 0xFF8C00

// defines for leds
#define FAST_LED_CHIPSET WS2812B
#define FAST_LED_DATA_PIN 5
#define MAX_BRIGHTNESS 240  // 255 is max but i limited
#define MIN_BRIGHTNESS 10   // minimum
const int brightnessPIN = A0;

// aici definite piesele 
RandomBrick brickLib[7] = {
  {
      1,//yoffset when adding brick to field
      4,
      { {0,0,0,0},
        {0,1,1,0},
        {0,1,1,0},
        {0,0,0,0}
      },
      WHITE
  },
  {
      0,
      4,
      { {0,1,0,0},
        {0,1,0,0},
        {0,1,0,0},
        {0,1,0,0}
      },
      GREEN
  },
  {
      1,
      3,
      { {0,0,0,0},
        {1,1,1,0},
        {0,0,1,0},
        {0,0,0,0}
      },
      BLUE
  },
  {
      1,
      3,
      { {0,0,1,0},
        {1,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
      },
      YELLOW
  },
  {
      1,
      3,
      { {0,0,0,0},
        {1,1,1,0},
        {0,1,0,0},
        {0,0,0,0}
      },
      AQUA
  },
  {
      1,
      3,
      { {0,1,1,0},
        {1,1,0,0},
        {0,0,0,0},
        {0,0,0,0}
      },
      HOTPINK
  },
  {
      1,
      3,
      { {1,1,0,0},
        {0,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
      },
      RED}
};
#define PIXELS FIELD_WIDTH*FIELD_HEIGHT
// PANA AICI DEFINE-URI SI FUNCTII NECESARE PENTRU BRICKS AND FIELD
CRGB leds[PIXELS];  //ledurile

void initPixels()
{
  FastLED.addLeds<FAST_LED_CHIPSET,FAST_LED_DATA_PIN,GRB>(leds,PIXELS);
}

void dodelay(int len)
{
  FastLED.delay(len);
}
//fct delay

//DE AICI PROGRAMEL PENTRU TETRIS --- FUNCTIILE

uint16_t brickSpeed;
unsigned long RowsThisLevel;
unsigned long RowsInTotal;

boolean tetrisOver;


//Functie care initializeaza fieldul

void clearField()
{
  uint8_t x, y;
  for(y = 0; y < FIELD_HEIGHT; y++)
  {
    for(x = 0; x < FIELD_WIDTH;x++)
    {
      field.pixels[x][y] = 0;
      field.color[x][y] = 0;
    }
  }
  for(x = 0; x < FIELD_WIDTH; x++)
  {
    field.pixels[x][FIELD_HEIGHT] = 1;
    // asta e randul ala unde se face testul de coliziune pt piese
  }
}
//Functie pentru crearea unei noi piese

void newActiveBrick()
{
 randomSeed(analogRead(5)/millis());
 uint8_t selectedBrick = random(7);
 unsigned int long selectedColor = brickLib[selectedBrick].color;
 activeBrick.Brick_size =  brickLib[selectedBrick].Brick_size;
 activeBrick.YOFFSET =  brickLib[selectedBrick].YOFFSET;
 activeBrick.POS_X = FIELD_WIDTH / 2 - activeBrick.Brick_size / 2;  
 activeBrick.POS_Y = BRICK_OFFSET - activeBrick.YOFFSET;
 activeBrick.enabled = true;

 activeBrick.color = selectedColor;

// pune piesa pe leduri
 uint8_t x, y;
 for (y = 0; y < BRICK_SIZE; y++) {
    for (x = 0; x < BRICK_SIZE; x++) {
      activeBrick.pixels[x][y] = (brickLib[selectedBrick]).pixels[x][y];
    }
  }
  //verifica daca se lovesc deja
  if(checkCollision_FIELD(&activeBrick))
  {
    tetrisOver = true;
  }
}
//functia checkCollision pt mai sus pt field

boolean checkCollision_FIELD(BRICK *brick)
{
  uint8_t brickX, brickY;
  uint8_t fieldX, fieldY;

  for(brickY = 0; brickY < BRICK_SIZE; brickY++)
  {
    for(brickX = 0; brickX < BRICK_SIZE; brickX++)
    {
      fieldX = (*brick).POS_X + brickX;
      fieldY = (*brick).POS_Y + brickY;  
      if(((*brick).pixels[brickX][brickY] == 1) && (field.pixels[fieldX][fieldY] == 1))
      {
        return true;
      } 
    }
  }
  return false;
}
// check collision cu marginile
boolean checkCollision_SIDE(BRICK *brick)
{
  uint8_t brickX, brickY;
  uint8_t fieldX, fieldY;

  for(brickY = 0; brickY < BRICK_SIZE; brickY++)
  {
    for(brickX = 0; brickX < BRICK_SIZE; brickX++)
    {
      fieldX = (*brick).POS_X + brickX;
      fieldY = (*brick).POS_Y + brickY;  
      if(fieldX < 0 || fieldX >= FIELD_WIDTH)
      {
        return true;  
      }
    }
  }
  return false;
}

void initGame()
{
  clearField();
  brickSpeed = INITIAL_SPEED;
  RowsThisLevel = 0;
  RowsInTotal = 0;
  tetrisOver = false;
  // aaaand 
  newActiveBrick();
}

boolean tetrisRunning = false;

//Functie citire input
uint8_t currentControl = BUTTON_NOPE; // initializat pe niciun buton apasat

void readInput()
{
  currentControl = BUTTON_NOPE;

  if(SQUISH.available() > 0)
  {
    uint8_t incomingInput = SQUISH.read();
    switch(incomingInput)
    {
      case 49: currentControl = BUTTON_UP;break;
      case 50: currentControl = BUTTON_DOWN;break;
      case 51: currentControl = BUTTON_LEFT;break;
      case 52: currentControl = BUTTON_RIGHT;break;
      case 53: currentControl = BUTTON_START;break;
      case 54: currentControl = BUTTON_EXIT;break;
    }  
  }
}

int long getPixel(int pos)
{
  // impachetate culorile intr-un long ca sa nu retii culorile in int separate
  return (leds[pos].red << 16) + (leds[pos].green<<8) + leds[pos].b;
}

// FUNCTIE OPRIRE LEDURI CU FADE

void dim(float dimval)
{
  for(uint8_t i = 0; i < (FIELD_WIDTH*FIELD_HEIGHT);i++)
  {
    int currentColor = getPixel(i);
    // aiic se despacheteaza
    int long red = ((currentColor & 0xFF0000)>>16);
    int green = ((currentColor & 0x00FF00)>>8);
    int blue = (currentColor & 0x0000FF);

    red = red*dimval;
    green = green*dimval;
    blue = blue*dimval;

    currentColor = (red<<16) + (green<<8) + blue;
    setPixel(i,currentColor);
  }
}

void runTetris()
{
  initGame();

  unsigned long lastUpdate = 0;

  tetrisRunning = true;

  while(tetrisRunning)
  {
    unsigned long currentTime;
    // se arunca piese si se citesc comenzile de la SQUISH
    do
    {
      readInput();
      if(currentControl != BUTTON_NOPE)
      {
        controlActiveBrick();
        Field();
        // se modifica valorile si se updateaza terenul
      }
      if(tetrisOver)break;
      currentTime = millis();
      // se retine timp pt marirea vitezei
    }
    while((currentTime - lastUpdate) < brickSpeed);
   
    lastUpdate = currentTime;

      if(tetrisOver)
      {
        // fade out animation :)
        for(int i = 0; i < 64; i++)
        {
          dim(0.75);
          showPixels();
          delay(30);
          }
          // aici intra ceva cod pt microsd

          // codu nu e aici ci intr-un alt fisier, probabil va fi inclus ".h" mai tarziu
          //char buff[4];
          //int len = sprintf(buff, "%i",RowsInTotal);

          //scrollText(buff,len,WHITE);

          tetrisRunning = false;
          break;
        }
        if(activeBrick.enabled)
        {
          shiftBrick(TODOWN);
        }
        else
        {
          checkFullLines();
          newActiveBrick();
          lastUpdate = millis();
        }
        Field();
  }
  // iar aia de faede
  for(int i = 0; i < 64; i++)
  {
     dim(0.75);
     showPixels();
     delay(30);
  }
}

// FUNCTIE verificare daca e linie jos plina

void checkFullLines()
{
  int x, y;
  int minimY = 0;
  for(y = (FIELD_HEIGHT -1); y >= minimY; y--)  // ia in considereare si linia aia de coliziune
  {
    uint8_t suma = 0;
    for(x = 0; x < FIELD_WIDTH; x++)
    {
      suma = suma + field.pixels[x][y];
    }
    if(suma >= FIELD_WIDTH)
    {
      // ii plin deci jet
      for(x = 0; x < FIELD_WIDTH; x++)
      {
        field.pixels[x][y] = 0;
        Field();
        delay(240);
      }
      // muta tot in jos cu 1 poz
      moveDown(y);
      y++;
      minimY++;
      Field();
      delay(240);

      RowsThisLevel++;
      RowsInTotal++;
      // se incrementeaza si pt punctaj
      if(RowsThisLevel >= LEVEL_UP)
      {
        RowsThisLevel = 0;
        brickSpeed = brickSpeed - SPEED_INCREMENT;
        if(brickSpeed < 200)
        {
          brickSpeed = 200;
        }
      }
    }
  }
}

// FUNCTIE MUTARE TOAET LINII IN JOS 

void moveDown(uint8_t pos)
{
  if(pos == 0)
  {//adica daca e prika de sus, nu ai ce muta
    return;
  }
  uint8_t x,y;
  for(y = pos-1; y > 0; y--)
  {
    for(x = 0; x < FIELD_WIDTH;x++)
    {
      field.pixels[x][y+1] = field.pixels[x][y];
      field.color[x][y+1] = field.pixels[x][y];
    }
  }
}

// FUNCTIE DE ADAUGARE IN ZID DE BRICKS

void addBrickToField()
{
  uint8_t brickX, brickY;
  uint8_t fieldX,fieldY;
  for(brickY = 0; brickY < BRICK_SIZE; brickY++)
  {
    for(brickX = 0; brickX < BRICK_SIZE; brickX++)
    {    
      fieldX = activeBrick.POS_X + brickX;
      fieldY = activeBrick.POS_Y + brickY;

      if(fieldX >= 0 && fieldY >= 0 && fieldX < FIELD_WIDTH && fieldY < FIELD_HEIGHT && activeBrick.pixels[brickX][brickY])
      {
        // VERIFICA DACA BRICK E IN FIELD
        field.pixels[fieldX][fieldY] = activeBrick.pixels[brickX][brickY];
        field.color[fieldX][fieldY] = activeBrick.color;
      }
    }
  }
}


// FUNCTIA DE MUTARE PIESA
void shiftBrick(int where)
{
  if(where == TODOWN)
  {
    activeBrick.POS_Y++; 
  }
  else if(where == TORIGHT)
  {
     activeBrick.POS_X++;
  }
  else if(where == TOLEFT)
  {
    activeBrick.POS_X--;
  }

  if((checkCollision_SIDE(&activeBrick)) || (checkCollision_FIELD(&activeBrick)))
  {
    if(where == TODOWN)
    {
      activeBrick.POS_Y--;
      addBrickToField();
      activeBrick.enabled = false;
    } 
    else if(where == TOLEFT)
    {
      activeBrick.POS_X++;
    }
    else if(where == TORIGHT)
    {
      activeBrick.POS_X--;
    }
  }
 }

// FUNCTIA DE ROTATIE BRICK

BRICK temporaryBrick;

void rotateBrick()
{
  /*
   copiaza piesa activa in structura sia poi ii face un shift de pozitii in matricea aia care alcatuieste piesa
   ea fiind pusa pe pozitiile 0 1 2 3 la alea de 4 piese si 0 1 2 la 3 piese, gen nu ajung epana jos  
  */
  uint8_t x, y;
   for (y = 0; y < BRICK_SIZE; y++) 
   {
      for (x = 0; x < BRICK_SIZE; x++) 
      {
        temporaryBrick.pixels[x][y] = activeBrick.pixels[x][y]; //copiaza in tempbrick aia activa
      }
   }
   temporaryBrick.POS_X = activeBrick.POS_X;
   temporaryBrick.POS_Y = activeBrick.POS_Y;
   temporaryBrick.Brick_size = activeBrick.Brick_size;

   if(temporaryBrick.Brick_size == 4)
   {
      temporaryBrick.pixels[0][0] = activeBrick.pixels[0][3];
      temporaryBrick.pixels[0][1] = activeBrick.pixels[1][3];
      temporaryBrick.pixels[0][2] = activeBrick.pixels[2][3];
      temporaryBrick.pixels[0][3] = activeBrick.pixels[3][3];
      temporaryBrick.pixels[1][0] = activeBrick.pixels[0][2];
      temporaryBrick.pixels[1][1] = activeBrick.pixels[1][2];
      temporaryBrick.pixels[1][2] = activeBrick.pixels[2][2];
      temporaryBrick.pixels[1][3] = activeBrick.pixels[3][2];
      temporaryBrick.pixels[2][0] = activeBrick.pixels[0][1];
      temporaryBrick.pixels[2][1] = activeBrick.pixels[1][1];
      temporaryBrick.pixels[2][2] = activeBrick.pixels[2][1];
      temporaryBrick.pixels[2][3] = activeBrick.pixels[3][1];
      temporaryBrick.pixels[3][0] = activeBrick.pixels[0][0];
      temporaryBrick.pixels[3][1] = activeBrick.pixels[1][0];
      temporaryBrick.pixels[3][2] = activeBrick.pixels[2][0];
      temporaryBrick.pixels[3][3] = activeBrick.pixels[3][0];
   }
   else if(temporaryBrick.Brick_size == 3)
   {
      temporaryBrick.pixels[0][0] = activeBrick.pixels[0][2];
      temporaryBrick.pixels[0][1] = activeBrick.pixels[1][2];
      temporaryBrick.pixels[0][2] = activeBrick.pixels[2][2];
      temporaryBrick.pixels[1][0] = activeBrick.pixels[0][1];
      temporaryBrick.pixels[1][1] = activeBrick.pixels[1][1];
      temporaryBrick.pixels[1][2] = activeBrick.pixels[2][1];
      temporaryBrick.pixels[2][0] = activeBrick.pixels[0][0];
      temporaryBrick.pixels[2][1] = activeBrick.pixels[1][0];
      temporaryBrick.pixels[2][2] = activeBrick.pixels[2][0];
      // alealalte nu le muta
      temporaryBrick.pixels[0][3] = 0;
      temporaryBrick.pixels[1][3] = 0;
      temporaryBrick.pixels[2][3] = 0;
      temporaryBrick.pixels[3][3] = 0;
      temporaryBrick.pixels[3][2] = 0;
      temporaryBrick.pixels[3][1] = 0;
      temporaryBrick.pixels[3][0] = 0;
   }
   else
   {
      SQUISH.println("BRICK SIZE ERROR !!!"); 
   }

   if((!checkCollision_SIDE(&temporaryBrick)) && (!checkCollision_FIELD(&temporaryBrick)))
   {
      for (y = 0; y < BRICK_SIZE; y++) 
      {
        for (x = 0; x < BRICK_SIZE; x++) 
        {
          activeBrick.pixels[x][y] = temporaryBrick.pixels[x][y];
        }
      }
   }
}

// FUNCTIA DE PRINTARE TEREN

void Field()
{
  int x,y;
  for (x = 0; x < FIELD_WIDTH; x++) 
   {
      for (y = 0; y < FIELD_HEIGHT; y++) 
      {
        uint8_t brickPix = 0;
        if(activeBrick.enabled)
        { // se testeaza daca se vede brick adica daca e in teren vizibila
          if((x >= activeBrick.POS_X) && (x < (activeBrick.POS_X + (activeBrick.Brick_size)))
            && (y >= activeBrick.POS_Y) && (y < (activeBrick.POS_Y + (activeBrick.Brick_size))))
            {
              //daca da
              brickPix = (activeBrick.pixels)[x - activeBrick.POS_X][y - activeBrick.POS_Y];
            }
        }
        if(field.pixels[x][y] == 1)
        {
          setFieldPixel(x,y,field.color[x][y]);
        }
        else if (brickPix == 1)
        {
          setFieldPixel(x,y,activeBrick.color);
        }
        else
        {
          setFieldPixel(x,y,0x000000);
        }
      }
   }
   showPixels();
}

void showPixels()
{
  int value = map(analogRead(brightnessPIN), 0, 1023, 0, MAX_BRIGHTNESS);
   FastLED.setBrightness(constrain(value, MIN_BRIGHTNESS, MAX_BRIGHTNESS));
   FastLED.show(); 
}


// functie setFieldPixel()

void setFieldPixel(int x, int y, int long color)
{
  setPixel(y%2 ? y * FIELD_WIDTH + x : y * FIELD_WIDTH + ((FIELD_HEIGHT-1)-x),color);
}
// pt mai sus
void setPixel(int pos, int long color)
{
  leds[pos] = CRGB(color);
}

// FUNCTIA controlActiveBrick();

void controlActiveBrick()
{
  switch(currentControl)
  {
    case BUTTON_UP:rotateBrick();break;
    case BUTTON_DOWN:shiftBrick(TODOWN);break;
    case BUTTON_RIGHT:shiftBrick(TORIGHT);break;
    case BUTTON_LEFT:shiftBrick(TOLEFT);break;
    case BUTTON_EXIT:tetrisRunning = false;break;
  }
}
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { rainbow, fancyRainbow, sparkle, runBoyColor, juggle, pulse };

uint8_t pattern_index = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns



void rainbow() 
{
  fill_rainbow( leds, 64, gHue, 7);   //functie din fastled libr
}

void fancyRainbow() 
{
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(64) ] += CRGB::White;
  }
}

void sparkle() 
{
  
  fadeToBlackBy( leds, 64, 10);
  FastLED.delay(1000/120);
  int pos = random16(64);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
  FastLED.delay(1000/120);
}

void runBoyColor()
{
  fadeToBlackBy( leds, 64, 20);
  int pos = beatsin16( 13, 0, 64-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}

void pulse()
{
  uint8_t BeatsPerMinute = 40;
  CRGBPalette16 palette = RainbowColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 199);
  for( int i = 0; i < 64; i++) {
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  fadeToBlackBy( leds, 64, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, 64-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void nextPattern()
{
  pattern_index = (pattern_index + 1) % ARRAY_SIZE( gPatterns);
}

void setup() {
  Serial.begin(9600);
  SQUISH.begin(9600);

  initPixels();
  showPixels();
}

void loop() {
  uint8_t input;
  gPatterns[pattern_index]();
      FastLED.show();  
      // delay for smoooooothness
      FastLED.delay(1000/120); // 120 framse /s 
      EVERY_N_MILLISECONDS( 20 ) { gHue++; } 
      EVERY_N_SECONDS( 15 ) { nextPattern(); } // schimba animatia la 15 sec
      if(SQUISH.available() > 0)
      {
        input = SQUISH.read();
        Serial.println(input);
        if(input == 53)runTetris();
      }
}

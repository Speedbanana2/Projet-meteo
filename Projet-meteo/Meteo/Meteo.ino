// Core graphics library
#include "Adafruit_GFX.h"
// Hardware-specific library for ST7735
#include "Adafruit_ST7735.h"
#include <SPI.h>

// La girouette
const short WIND_VANE = A1;
const short TFT_CS = 10;
const short TFT_RST = 9;
const short TFT_DC = 8;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define PI 3.1415926535897932384626433832795
const short ANEMOMETRE = 2;
// Capteur de pression
const short PRESSURE_SENSOR = A2;

// https://www.youtube.com/watch?v=KHrTqdmYoAk
// Lien pour m'aider

// tft.setRotation(1) --> 90degrees
// tft.setRotation(2) --> 180degrees
// tft.setRotation(3) --> 270degrees

// Structure pour gérer les positions sur l'écran
struct Vector2
{
  short x;
  short y;
};

// La direction du vent
char windDirection[4];
// L'angle de la girouette
float angle = 0;
// L'angle précédent de la girouette
float previousAngle = 0;

// Gestion du temps
float deltaTime = 0;
// Temps fixe que delta Time doit dépasser pour relever la vitesse
float fixedTime = 0.50;

// Nombre de tours
short nbRide = 0;

// Vitesse du vent
// 2.4 km/h = 1 désactivation du switch par seconde
float currentSpeed = 0;
bool canReadSpeed = true;

// Lecture du port analogique liée au capteur de pression
int rawValue;
const short OFFSET = 410;
const short FULL_SCALE = 9630;
// Pression final en kPa
float pressure;


// Variable pour l'angle
Vector2 anglePosition;
// Variable pour la direction du vent
Vector2 windDirectionPosition;
// Variable pour la vitesse
Vector2 speedPosition;
// Variable pour la position de la flèche
Vector2 arrowPosition[3];
// La position de départ à l'angle 0 de la flèche
Vector2 startArrowPosition[3];
// Variable pour définir la position de la flèche
Vector2 middleArrowPosition;
// Variable pour la pression
Vector2 pressurePosition;
// Variable pour le titre de la pression
Vector2 pressureTitlePosition;

void setup()
{
  Serial.begin(9600);
  Serial.println("Hello");
  // Init ST7735S chip, black tab
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLACK);

  // La rotation se fait sur tous l'écran
  // ça à une impacte sur tous les éléments graphiques
  // Rotation de 270 degrées
  tft.setRotation(3);

  initialise();
  
  displayAngleTitle();
  displaySpeedTitle();
  displayPressureTitle();
  
  // On active la résistance de pull up
  pinMode(ANEMOMETRE, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ANEMOMETRE), getTime, FALLING);

  delay(1000);
}

// Initialiser toutes les variables
void initialise()
{
  // Ligne horizontale
  tft.fillRect(0, (tft.height()/2) - 5, tft.width() - 1, 5, ST7735_RED);

  // Ligne Vertical
  tft.fillRect(tft.width()/2, 0, 5, tft.height() - 1, ST7735_RED);

  anglePosition = setPosition((tft.width()/2) + 20, (tft.height()/2) - 30);
  windDirectionPosition = setPosition(tft.width() - 30, tft.height() - 10);
  speedPosition = setPosition(20, (tft.height()/2) - 30);

  pressurePosition = setPosition(20, tft.height() - 20);
  pressureTitlePosition = setPosition(20, (tft.height()/2) + 10);
  
  startArrowPosition[0] = setPosition((tft.width()/2) + 40, (tft.height()/2) + 30);
  startArrowPosition[1] = setPosition((tft.width()/2) + 30, (tft.height()/2) + 25);
  startArrowPosition[2] = setPosition((tft.width()/2) + 30, (tft.height()/2) + 35);

  for(int i = 0; i < sizeof(arrowPosition)/4; i++)
  {
    arrowPosition[i] = startArrowPosition[i];
  }

  middleArrowPosition = setPosition((tft.width()/2) + 35, (tft.height()/2) + 30);
}

void loop()
{
  Serial.println("A");
  float angleValue = analogRead(WIND_VANE);
  
  canReadSpeed = true;

  restartTime();
  displaySpeed();

  getAngle(angleValue);
  displayAngle();
  displayWindDirection();

  pressureCalcul();
  displayPressure();

  turnArrow();
  displayArrow();
}

// Afficher le titre des angles
void displayAngleTitle()
{
  tft.setCursor(anglePosition.x, anglePosition.y - 30);
  tft.print("Angle");

  tft.setCursor(anglePosition.x - 10, anglePosition.y - 20);
  tft.print("en degres");
}

// Afficher les angles
void displayAngle()
{
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.setCursor(anglePosition.x, anglePosition.y);
  tft.print(angle);
}

// Afficher les directions du vent
void displayWindDirection()
{
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.setCursor(windDirectionPosition.x, windDirectionPosition.y);
  tft.print(windDirection);
}

// Afficher la flèche
void displayArrow()
{
  Vector2 pos1 = arrowPosition[0];
  Vector2 pos2 = arrowPosition[1];
  Vector2 pos3 = arrowPosition[2];
  
  tft.fillTriangle(pos1.x, pos1.y, pos2.x, pos2.y, pos3.x, pos3.y, ST7735_WHITE);
}

// Tourner le triangle en fonction de la girouette (de la direction du vent)
void turnArrow()
{
  if(previousAngle == angle)
  {
    return;
  }

  previousAngle = angle;
  
  hideArrow();
  
  int x, y;

  for(int i = 0; i < sizeof(arrowPosition)/4; i++)
  {
    // Help
    // https://www.stashofcode.fr/rotation-dun-point-autour-dun-centre/#:~:text=Dans%20cette%20situation%2C%20le%20point,%2D%20R%20*%20sin%20(%CE%B1)
    arrowPosition[i] = startArrowPosition[i];

    x = arrowPosition[i].x - middleArrowPosition.x;
    y = arrowPosition[i].y - middleArrowPosition.y;

    arrowPosition[i].x = x * cos(degresToRadiant()) + y * sin(degresToRadiant()) + middleArrowPosition.x;
    arrowPosition[i].y = - x * sin(degresToRadiant()) + y * cos(degresToRadiant()) + middleArrowPosition.y;
  }
}

// Cacher la flèche
void hideArrow()
{
  Vector2 pos1 = arrowPosition[0];
  Vector2 pos2 = arrowPosition[1];
  Vector2 pos3 = arrowPosition[2];
  
  tft.fillTriangle(pos1.x, pos1.y, pos2.x, pos2.y, pos3.x, pos3.y, ST7735_BLACK);
}

void displaySpeedTitle()
{
  tft.setCursor(speedPosition.x, speedPosition.y - 30);
  tft.print("Vitesse");

  tft.setCursor(speedPosition.x, speedPosition.y - 20);
  tft.print("en Km/h");
}

// Afficher la vitesse
void displaySpeed()
{
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.setCursor(speedPosition.x, speedPosition.y);
  tft.print(currentSpeed);
}

// évènement
void getTime()
{
  if(!canReadSpeed)
  {
    return;
  }
  
  canReadSpeed = false;
  nbRide ++;
}

// Calcul de la vitesse de l'anemomètre
void calculSpeed()
{
  // 2.4 km/h = 1 switch désactivé par secondes
  
  currentSpeed = nbRide * 2.4;
  
  Serial.print("Nombre de tours : ");
  Serial.print(nbRide);
  Serial.print(" vitesse en km/h : ");
  Serial.println(currentSpeed);
}

// Gestion du temps
void restartTime()
{
  deltaTime += 0.01;
  if(deltaTime >= fixedTime)
  {
    calculSpeed();

    if(fixedTime >= 1.00)
    {
      deltaTime = 0.00;
      nbRide = 0;
      fixedTime = 0.50;
      return;
    }

    fixedTime = 1.00;
  }
}

// Récupérer l'angle
void getAngle(float angleValue)
{
  if(angleValue > 930)
  {
    // West
    angle = 270;
    sprintf(windDirection, "  W");
    return;
  }
  
  else if(angleValue > 870)
  {
    // Nord West
    angle = 315;
    sprintf(windDirection, " NW");
    return;
  }
  
  else if(angleValue > 810)
  {
    // West Nord West
    angle = 292.5;
    sprintf(windDirection, "WNW");
    return;
  }
  
  else if(angleValue > 750)
  {
    // Nord
    angle = 0;
    sprintf(windDirection, "  N");
    return;
  }
  
  else if(angleValue > 670)
  {
    // Nord Nord West
    angle = 337.5;
    sprintf(windDirection, "NNW");
    return;
  }
  
  else if(angleValue > 610)
  {
    // Sud West
    angle = 225;
    sprintf(windDirection, " SW");
    return;
  }
  
  else if(angleValue > 570)
  {
    // West Sud West
    angle = 247.5;
    sprintf(windDirection, "WSW");
    return;
  }
  
  else if(angleValue > 440)
  {
    // Nord Est
    angle = 45;
    sprintf(windDirection, " NE");
    return;
  }
  
  else if(angleValue > 390)
  {
    // Nord Nord Est
    angle = 22.5;
    sprintf(windDirection, "NNE");
    return;
  }
  
  else if(angleValue > 270)
  {
    // Sud
    angle = 180;
    sprintf(windDirection, "  S");
    return;
  }
  
  else if(angleValue > 230)
  {
    // Sud Sud West
    angle = 202.5;
    sprintf(windDirection, "SSW");
    return;
  }
  
  else if(angleValue > 175)
  {
    // Sud Est
    angle = 135;
    sprintf(windDirection, " SE");
    return;
  }
  
  else if(angleValue > 120)
  {
    // Sud Sud Est    
    angle = 157.5;
    sprintf(windDirection, "SSE");
    return;
  }
  
  else if(angleValue > 90)
  {
    // Est
    angle = 90;
    sprintf(windDirection, "  E");
    return;
  }
  
  else if(angleValue > 70)
  {
    // Est Nord Est
    angle = 67.5;
    sprintf(windDirection, "ENE");
    return;
  }
  
  else if(angleValue >= 0)
  {
    // Est Sud Est
    angle = 112.5;
    sprintf(windDirection, "ESE");
    return;
  }
}

// Calculer la pression
void pressureCalcul()
{
  rawValue = 0;
  for (int x = 0; x < 10; x++)
  {
    rawValue = rawValue + analogRead(PRESSURE_SENSOR);
  }
  pressure = (rawValue - OFFSET) * 700.0 / (FULL_SCALE - OFFSET); // pressure conversion
}

// Afficher la pression
void displayPressure()
{
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.setCursor(pressurePosition.x, pressurePosition.y);
  tft.print(pressure);
}

// Afficher le titre de la pression
void displayPressureTitle()
{
  tft.setCursor(pressureTitlePosition.x, pressureTitlePosition.y);
  tft.print("Pression");

  tft.setCursor(pressureTitlePosition.x, pressureTitlePosition.y + 10);
  tft.print("en kPa");
}

// Conversion des degrés en radiant
float degresToRadiant()
{
  float rad = angle * PI / 180;
  return rad;
}

// Données une position de type vecteur 2D à un éléments graphique
Vector2 setPosition(int posX, int posY)
{
  Vector2 newPosition;

  newPosition.x = posX;
  newPosition.y = posY;

  return newPosition;
}

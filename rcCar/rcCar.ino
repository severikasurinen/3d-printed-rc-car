#include <EEPROM.h>
#include <Servo.h>
#include <SPI.h>
#include <RF24.h>
#include <avr/wdt.h>

Servo servos[2];
RF24 receiver(4, 5);

const int enablePin = 6, fwdPin = 7, bwdPin = 8, servoPins[] = { 9, 10 } /*{ front, rear }*/, ledPins[] = { 2, 3 } /*{ power, connection }*/, lightPin = A0, channels[] = { 80, 76, 72, 70 }, midPoints[] = { 13, 13 } /*servo offset from middle*/;
const float timeout = 0.5 /*time of no data received until disconnecting (in seconds)*/;
const byte addresses[][4] = {"rcRX","rcTX"};

unsigned long lastTransmission = 0;
int vals[] = { 0, 0, 0, 0, 0 } /*{ channel, light, speed, rotation, rotation compensation }*/, rotComp;
bool rcConnected = false;

void setup()
{
  Serial.begin(115200);
  
  pinMode(enablePin, OUTPUT);
  pinMode(fwdPin, OUTPUT);
  pinMode(bwdPin, OUTPUT);

  for(int i=0; i<2; i++)
  {
    servos[i].attach(servoPins[i]);
    pinMode(ledPins[i], OUTPUT);
  }

  pinMode(lightPin, OUTPUT);

  digitalWrite(ledPins[0], HIGH);

  // set to default
  Turn(0);
  Drive(0);
  digitalWrite(lightPin, LOW);

  // read selected channel from memory
  vals[0] = EEPROM.read(0)-1;
  if(vals[0] > 3 || vals[0] < 0)
  {
    vals[0] = 0;
    EEPROM.write(0, 1);
  }
  Serial.print("Channel: ");
  Serial.println(vals[0]+1);

  // initialize receiver
  receiver.begin();
  receiver.setChannel(channels[vals[0]]);
  receiver.setPALevel(RF24_PA_MAX);
  receiver.setDataRate(RF24_1MBPS);
  receiver.setRetries(5,10);
  
  receiver.openWritingPipe(addresses[0]);
  receiver.openReadingPipe(1, addresses[1]);
  receiver.startListening();
}

void loop()
{
  if(!rcConnected)
  {
    if(receiver.available())
    {
      lastTransmission = millis();
      
      unsigned int data;
      while (receiver.available())
      {
        receiver.read(&data, sizeof(data));
      }
      
      if(data > 0) // transmitter received acknowledgement message
      {
        rcConnected = true;
        digitalWrite(ledPins[1], HIGH);
        Serial.println("Connection established");
      }
    }
    delay(10);
  }
  else
  {
    if(receiver.available())
    {
      lastTransmission = millis();
      
      int data[5];
      while(receiver.available())
      {
        receiver.read(&data, sizeof(data));
      }

      if(data[0] != vals[0])
      {
        // change channel
        EEPROM.write(0, data[0]+1);
        Restart();
      }

      for(int i=0; i<5; i++)
      {
        vals[i] = data[i];
      }
      
      Serial.print("RX Light: ");
      Serial.println(vals[1]);

      digitalWrite(lightPin, vals[1]);
      
      float curSpeed = map(vals[2], 0, 180, -90, 90)/90.0;
      if(curSpeed > -0.2 && curSpeed < 0.2) // if speed stick is close to middle, set speed to 0
      {
        curSpeed = 0;
      }
      Drive(curSpeed);
      
      Serial.print("RX Speed: ");
      Serial.println(curSpeed);
      
      float curRot = map(vals[3], 0, 180, -45, 45);
      
      Serial.print("RX Rotation: ");
      Serial.println(curRot);
      
      rotComp = map(vals[4], 0, 180, -30, 30);
      
      Serial.print("RX Rotation Compensation: ");
      Serial.println(rotComp);
      
      Turn(curRot);
    }
    else if(millis() - lastTransmission >= timeout*1000) // not receiving data and too long since last time?
    {
      rcConnected = false;
      Serial.println("Connection lost");

      // set to default
      digitalWrite(ledPins[1], LOW);
      Turn(0);
      Drive(0);
      digitalWrite(lightPin, LOW);
    }
    delay(10);
  }
}

void Drive(float driveSpeed) // drive speed from -1 to 1
{
  analogWrite(enablePin, round(abs(driveSpeed*255)));
  
  if(driveSpeed > 0) // drive forward
  {
    digitalWrite(fwdPin, HIGH);
    digitalWrite(bwdPin, LOW);
  }
  else if(driveSpeed < 0) // drive backward
  {
    digitalWrite(bwdPin, HIGH);
    digitalWrite(fwdPin, LOW);
  }
  else // stop
  {
    digitalWrite(fwdPin, LOW);
    digitalWrite(bwdPin, LOW);
  }
}

void Turn(int dir) // direction from -45 to 45 degrees
{
  int lDirs[] = { -dir+90+midPoints[0]-rotComp, +dir+90-midPoints[1] };

  for(int i=0; i<2; i++)
  {
    servos[i].write(lDirs[i]);
  }
}

void Restart() // Restart Arduino
{
  wdt_enable(WDTO_60MS);
  while(1){}
}

#include <EEPROM.h>
#include <SPI.h>
#include <RF24.h>
#include <avr/wdt.h>

RF24 transmitter(7, 8);

const int ledPins[] = { 4, 5, 6 } /*{ power, connection, light }*/, channelPins[] = { A2, A3, A4, A5 }, btnPins[] = { 2, 3 } /*{ channel, light }*/, potPins[] = { A6, A7, A0 } /*{ speed, rotation, rotation compensation }*/, channels[] = { 80, 76, 72, 70 }, rangeMin[] = { 800, 220, 0 } /*potentiometer min values*/, rangeMax[] { 240, 800, 1023 } /*potentiometer max values*/, changeThreshold = 2 /*threshold for transmitting new values*/;
const float timeout = 0.5 /*time of no successful transmission until disconnecting (in seconds)*/, minFreq = 0.3 /*minimum transmission frequency (in seconds)*/;
const byte addresses[][4] = {"rcRX","rcTX"};

unsigned long lastTransmission = 0;
int connectionStatus = 0, curVals[] = { 0, 0, 0, 0, 0 } /*{ channel, light, speed, rotation, rotation compensation }*/, lastVals[5];
bool btnDown[] = { false, false };

void setup()
{
  Serial.begin(115200);
  
  for(int i=0; i<2; i++)
  {
    pinMode(btnPins[i], INPUT);
  }
  for(int i=0; i<3; i++)
  {
    pinMode(ledPins[i], OUTPUT);
  }
  for(int i=0; i<4; i++)
  {
    pinMode(channelPins[i], OUTPUT);
  }
  
  digitalWrite(ledPins[0], HIGH);
  
  // read selected channel from memory
  curVals[0] = EEPROM.read(0)-1;
  if(curVals[0] > 3 || curVals[0] < 0)
  {
    curVals[0] = 0;
    EEPROM.write(0, 1);
  }
  lastVals[0] = curVals[0];
  digitalWrite(channelPins[curVals[0]], HIGH);
  
  Serial.print("Channel: ");
  Serial.println(curVals[0]+1);
  
  // initialize transmitter
  transmitter.begin();
  transmitter.setChannel(channels[curVals[0]]);
  transmitter.setPALevel(RF24_PA_MAX);
  transmitter.setDataRate(RF24_1MBPS);
  transmitter.setRetries(5,10);
  
  transmitter.openWritingPipe(addresses[1]);
  transmitter.openReadingPipe(1, addresses[0]);
}

void loop()
{
  if(connectionStatus < 2)
  {
    Serial.println("Transmitting");
    int data = connectionStatus;
    if(transmitter.write(&data, sizeof(data)))
    {
      lastTransmission = millis();
      
      Serial.print("Sent: ");
      Serial.println(data);
      
      connectionStatus++;
      if(connectionStatus > 1) // received 2 acknowledgement messages from receiver
      {
        digitalWrite(ledPins[1], HIGH);
        Serial.println("Connection established");
      }
    }
    else // transmission failed
    {
      if(!btnDown[0])
      {
        if(digitalRead(btnPins[0]))
        {
          btnDown[0] = true;
        }
      }
      else if(!digitalRead(btnPins[0]))
      {
        btnDown[0] = false;
        
        curVals[0]++;
        if(curVals[0] > 3)
        {
          curVals[0] = 0;
        }

        // change channel
        EEPROM.write(0, curVals[0]+1);
        Restart();
      }
    }
    delay(10);
  }
  else
  {
    for(int i=0; i<3; i++) // map potentiometer values to range 0-180
    {
      curVals[2+i] = max(min(map(analogRead(potPins[i]), rangeMin[i], rangeMax[i], 0, 180), 180), 0);
    }
    
    if(!btnDown[0])
    {
      if(digitalRead(btnPins[0]))
      {
        btnDown[0] = true;
      }
    }
    else if(!digitalRead(btnPins[0])) // stopped pressing channel button
    {
      btnDown[0] = false;
      
      curVals[0]++;
      if(curVals[0] > 3)
      {
        curVals[0] = 0;
      }
    }

    if(!btnDown[1])
    {
      if(digitalRead(btnPins[1]))
      {
        btnDown[1] = true;
      }
    }
    else if(!digitalRead(btnPins[1])) // stopped pressing light button
    {
      btnDown[1] = false;
      
      curVals[1] = !curVals[1];
    }
    digitalWrite(ledPins[2], curVals[1]);

    bool changed = false;
    for(int i=2; i<5; i++)
    {
      if(abs(curVals[i] - lastVals[i]) >= changeThreshold)
      {
        changed = true;
        break;
      }
    }
    if(curVals[0] != lastVals[0] || curVals[1] != lastVals[1] || changed || millis() - lastTransmission >= minFreq*1000) // send data?
    {
      SendData();
    }
    
    if(millis() - lastTransmission >= timeout*1000) // too long since last successfull transmission?
    {
      connectionStatus = 0;
      digitalWrite(ledPins[1], LOW);
      Serial.println("Connection lost");
    }
    
    delay(10);
  }
}

void SendData() // Transmit current values
{
  int data[5];
  
  Serial.print("TX Channel: ");
  Serial.println(curVals[0]);
  
  Serial.print("TX Light: ");
  Serial.println(curVals[1]);
  
  Serial.print("TX Speed: ");
  Serial.println(curVals[2]);
  
  Serial.print("TX Rotation: ");
  Serial.println(curVals[3]);
  
  Serial.print("TX Rotation Compensation: ");
  Serial.println(curVals[4]);
  
  for(int i=0; i<5; i++)
  {
    data[i] = curVals[i];
  }
  
  if(transmitter.write(&data, sizeof(data)))
  {
    lastTransmission = millis();

    if(curVals[0] != lastVals[0])
    {
      // change channel
      EEPROM.write(0, curVals[0]+1);
      Restart();
    }
    
    for(int i=0; i<5; i++)
    {
      lastVals[i] = curVals[i];
    }
  }
  else
  {
    Serial.println("Transmission failed");
    delay(10);
  }
}

void Restart() // Restart Arduino
{
  wdt_enable(WDTO_60MS);
  while(1){}
}

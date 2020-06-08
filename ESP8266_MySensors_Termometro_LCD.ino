/*
  ESP8266_MySensors_Termometro_LCD
  
  Incluir bibliotecas:
    - NTPClient by Fabrice Weinberg

  Hardware
    - ESP8266 (NodeMCU) + LCD ST7735 de 2.2"    

    by Macelo Campos 
    www.marcelocampos.cc
    https://github.com/MarceloCampos
*/

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>        //-|_<relógio NTP>
#include <NTPClient.h>      //-|

#define TFT_BACK_COLOR ST7735_BLACK

char* ssid     = "-----";
char* password = "----";
char* host = "192.168.0.110";  // <<--- prencher aqui o endereço IP do Gateway MySensors
char buf[64];
char daysOfTheWeek[7][12] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};

uint16_t port = 5003;
int lineRodape = 110;
int lineTemp = 34;
int lineHour = 90;
int lineHeader = 10;

const int qtdSensores = 2;
int _debug = 0;

const long utcOffsetInSeconds = (3600 * -3);  // UTC / Time Zone do Brasil = -3
unsigned long timeout = 0;

String otp;

struct SensoresStruct
{
  int SensorNumero;  
  String SensorNome; 
  int SensorTipoNode1;
  int SensorTipoNode2;
};
SensoresStruct sensores[qtdSensores];

struct NrfMessage
{
  int nodeId;
  int childSensorId;
  int command;
  int ack;
  int type;
  String payload; 
};

enum MySensorsCommand
{
  _presentation = 0,
  _set = 1,
  req = 2,
  internal = 3, 
  stream = 4
};
MySensorsCommand command;

enum MySensorsType
{
  V_TEMP = 0, 
  V_VAR1 = 24,
  V_VOLTAGE = 38
};
MySensorsType type;

#define D3 0
#define D4 2 
#define D5 14 
#define D7 13 
#define D8 15 
#define TFT_CS     D8
#define TFT_RST    D4  
#define TFT_DC     D3
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST); 
#define TFT_SCLK D5   
#define TFT_MOSI D7 

WiFiClient client;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.br", utcOffsetInSeconds);

void setup(void)
{  
  otp.reserve(64 + 1);
  
  tft.initR(INITR_BLACKTAB); 
  tft.setRotation(3);  

  printText(ST7735_WHITE, 1, "Inicio OK", true, 10, lineRodape);  
  delay(500);

  loadSensores(); 
     
  Serial.begin(115200);
  Serial.print("\r\n\r\n> Conectando WiFi: ");
  Serial.println(ssid);

  connectSTA();
      
  connectServer();    // obs: apaga Display
  
  showSensoresSerial(); 

  timeClient.begin();

  otp = sensores[0].SensorNome + "        " + sensores[1].SensorNome;
  otp.toCharArray(buf, otp.length()+2);  
  printText(ST7735_WHITE, 1, buf, false, 16, lineTemp + 20);

  otp = "MySensors";
  otp.toCharArray(buf, otp.length()+1); 
  printText(ST7735_WHITE, 2, buf, false, 24, lineHeader );
    
  delay(500);
} 

void loop()
{
  if(client.connected())
  {
    if(client.available())
    {
      String resp = getServerData();
      if( resp != "" && resp.indexOf("Gateway startup complete") == -1 );
        analizaServerData(resp);
    }
  }
  else
  {
    delay(2000);
    connectServer();
  }

  if(millis() - timeout >= 1000)
  {
    updateTime();
    timeout = millis();
  }
} 

void updateTime()
{
  int tmp = 0;
  char toErase[] = {218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 0x00};
  timeClient.update();

//  Serial.print(daysOfTheWeek[timeClient.getDay()]);
//  Serial.print(", ");
//  Serial.print(timeClient.getHours());
//  Serial.print(":");
//  Serial.print(timeClient.getMinutes());
//  Serial.print(":");
//  Serial.println(timeClient.getSeconds());
  //Serial.println(timeClient.getFormattedTime());  

  otp = "";
  otp += String(daysOfTheWeek[timeClient.getDay()]) + " ";
  
  otp += stringFormat(String(timeClient.getHours()), 2) + ":";
  otp += stringFormat(String(timeClient.getMinutes()), 2) + ":";  
  otp += stringFormat(String(timeClient.getSeconds()), 2);
  Serial.println(otp);
  
  otp.toCharArray(buf, otp.length()+1); 
  printText(TFT_BACK_COLOR, 1, toErase, false, 32, lineHour );  // apaga antes de imprimir 
  printText(ST7735_WHITE, 1, buf, false, 32, lineHour );
 
}

String stringFormat(String mx, int _size)
{
    while(mx.length() < _size)
      mx = "0" + mx;
      
    return mx;
}

void analizaServerData(String resp)
{
  NrfMessage message = stringSplit(resp);
    
  if(_debug > 0)
  {
    Serial.print( "> ");
    Serial.print(message.nodeId); Serial.print("-");  
    Serial.print(message.childSensorId); Serial.print("-");
    Serial.print(message.command); Serial.print("-");
    Serial.print(message.ack); Serial.print("-");
    Serial.print(message.type); Serial.print("-");
    Serial.print(message.payload);
    Serial.println(" <");
  }
  for( int i=0; i< qtdSensores; i++)
  {
    if(message.nodeId == sensores[i].SensorNumero)  // cadastrado ?
    {
      switch(message.command)
      {
        case _set: 
          if(message.type == V_TEMP )
          {
            int Col = 0;
            
            if(i == 0)
              Col = 14;
            else if (i == 1)
              Col = 86;
                          
            otp = message.payload + "C"; // ° = ASCII 248
            otp.toCharArray(buf, otp.length()+1); 
            Serial.print("$ Temp sensor "); Serial.print(sensores[i].SensorNome);           
            Serial.print(" : "); Serial.print( message.payload); Serial.println(" oC");
            
            printTextTemp(ST7735_WHITE, 2, buf, false, Col, lineTemp);
          }
          break;
      }
    }
  }
}
/*
  int nodeId;
  int childSensorId;
  int command;
  int ack;
  int type;
  String payload; 
  node-id ; child-sensor-id ; command ; ack ; type ; payload
*/
void loadSensores()
{   
  sensores[0].SensorNumero = 28;
  sensores[0].SensorNome = "Int.";

  sensores[1].SensorNumero = 18;
  sensores[1].SensorNome = "Ext.";
}

void showSensoresSerial()
{
  Serial.println("$ Listagem sensores:");
  for(int i=0; i < qtdSensores; i++)
  {
    Serial.print("\t> Nr: ");Serial.print(sensores[i].SensorNumero);
    Serial.print(", Descr: ");Serial.println(sensores[i].SensorNome);   
  }
  Serial.print("$ Total Sensores Cadastrados: "); Serial.println(qtdSensores);
}

void printTextTemp(int textColor, int textSize, char *textToPrint, bool isToBlankScreen, int xPos, int yPos)
{
  char toErase[] = {218, 218, 218, 218, 218, 0x00};  
  int k = 0;
    
  printText(TFT_BACK_COLOR, 2, toErase, false, xPos, yPos); // apaga antes de imprimir
  printText(ST7735_WHITE, 2, textToPrint, false, xPos, yPos);
}


void printText(int textColor, int textSize, char *textToPrint, bool isToBlankScreen, int xPos, int yPos)
{
  if(isToBlankScreen)
  {
    blanScreen();
  } 
  
  tft.setCursor(xPos,yPos);
  tft.setTextColor(textColor);
  tft.setTextSize(textSize);
  tft.print(textToPrint);   
}

void blanScreen()
{
  tft.fillScreen(TFT_BACK_COLOR);  
  tft.drawRoundRect(2, 2, 155,124 , 6, ST77XX_BLUE);  // moldura maior, todo display

  tft.drawRect(2, 28, 155, 39, ST77XX_BLUE);  // moldura menor, temperaturas
  tft.drawLine( 79, 28, 79, 67, ST77XX_BLUE);  // moldura menor, temperaturas

}


void connectSTA()
{
  otp = "Conectando " + String(ssid);
  otp.toCharArray(buf, otp.length()+1);  
  printText(ST7735_WHITE, 1, buf, true, 10, lineRodape);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
 
  otp = "WiFi IP: " + IpAddress2String(WiFi.localIP());

  otp.toCharArray(buf, otp.length()+2);  
  printText(ST7735_WHITE, 1, buf, true, 10, lineRodape);
  
  Serial.print("\r\n> ");
  Serial.println(otp);  
  delay(2000);
}

void connectServer()
{    
  otp = host + String(':') + String(port); 
  otp.toCharArray(buf, otp.length()+1);  
  printText(ST7735_WHITE, 1, buf, true, 10, lineRodape); 
  
  Serial.print("> Conectando Gtw: ");
  Serial.print(host);
  Serial.print(':');
  Serial.print(port);
  
  if (!client.connect(host, port)) 
  {
    Serial.println (", Falhou");
    otp = " Erro";
    otp.toCharArray(buf, otp.length()+1);
    printText(ST7735_WHITE, 1, buf, false, 120, lineRodape); 
    
    delay(1000);
    WiFiClient client;
    delay(5000);
    //return;
  }
  else  
  {
    Serial.println(", Conectado");

    otp = " OK  ";
    otp.toCharArray(buf, otp.length()+1);
    printText(ST7735_WHITE, 1, buf, false, 120, lineRodape); 
  }
}

String getServerData() 
{ 
  String ret = "";   
//  Serial.print("> ");
 
  while (client.available()) 
  {
    char ch = static_cast<char>(client.read());
    ret += ch;  
  }
//    Serial.print(ret);
    return ret;   
}

String IpAddress2String(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ;
}

NrfMessage stringSplit(String strToSplit)
{
  NrfMessage nrfMessage;
  char buf[128];
  
  strToSplit.toCharArray(buf, strToSplit.length());   
  
//  char *tok = strtok(buf, ";");      
//  if (!tok) 
//    return nrfMessage;

  char *nodeId = strtok(buf, ";");
  if (!nodeId)
    return nrfMessage;        
  nrfMessage.nodeId = atoi(nodeId);
  
  char *childSensorId = strtok(NULL, ";");
  if (!childSensorId)
    return nrfMessage;      
  nrfMessage.childSensorId = atoi(childSensorId);
  
  char *command = strtok(NULL, ";");
  if (!command)
    return nrfMessage;   
  nrfMessage.command = atoi(command);
   
  char *ack = strtok(NULL, ";");
  if (!ack)
    return nrfMessage;
  nrfMessage.ack = atoi(ack);
  
  char *type = strtok(NULL, ";");
  if (!type)
    return nrfMessage;
  nrfMessage.type = atoi(type);
  
  char *payload = strtok(NULL, ";");
  if (!payload)
    return nrfMessage;
  nrfMessage.payload = String(payload);

  return nrfMessage;    
}

  // node-id ; child-sensor-id ; command ; ack ; type ; payload
  // nodeId  childSensorId command ack type payload
  // 28;0;1;0;24;5.15
  // 28;255;3;0;0;106
  // 28;0;1;0;0;20.2



  


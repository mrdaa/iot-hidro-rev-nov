#include <DHT.h>
#include <ESP8266WiFi.h>
#include <SocketIoClient.h>
#include <EEPROM.h>
#include <GravityTDS.h>

SocketIoClient socket; // socket io
GravityTDS gravityTds; //tds

//Setup Global Variable
String id = "1471984882";
bool IsConnect = false;

//Variable kalibrasi
float currWl = 0.0;
int nilaiToleransiWl = 12;

//Variable automatisasi
bool Mode_Prototype = true;
bool RelayStatus1 = false;
int modesekarang = 1;

bool RelayStatus2 = false;

int NilaiWl = 29;
int NilaiTemp = 31;

//define your sensors here
#define DHTTYPE DHT11
#define trigPin D6
#define echoPin D7
#define dhtPin D8 // ganti jadi d8 D2 dipakek buat RTC nantinya
#define relay1 D3
#define relay2 D4
#define TdsSensorPin A0

DHT dht(dhtPin, DHTTYPE);
float temperature = 25, tdsValue;
long duration;
float distance;
const int max_hight = 14;

// Setting WiFi
char ssid[] = "a";
char pass[] = "123456789";

//Setting Server
// char SocketServer[] = "192.168.43.47";
char SocketServer[] = "192.168.43.132";
int port = 4000;

void setup()
{
  //setup pins and sensor
  pinMode(relay1, OUTPUT);
  pinMode(relay1, HIGH);
  pinMode(relay2, OUTPUT);
  pinMode(relay2, HIGH);

  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(3.3);      //reference voltage on ADC, default 5.0V on Arduino UNO
  gravityTds.setAdcRange(1024); //1024 for 10bit ADC; 4096 for 12bit ADC
  gravityTds.begin();

  // pinMode(TdsSensorPin, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  dht.begin();

  //setup tombol realtime aplikasi
  SetupRelayAplikasi();

  Serial.begin(115200);

  //Setup WiFi
  WiFi.begin(ssid, pass);
  // while (WiFi.status() != WL_CONNECTED)
  // {
  delay(1000);
  // }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");

  socket.begin(SocketServer, port);
  //   RelayStatus1 = false;
  //   RelayStatus2 = false;
  //listener for socket io start

  socket.on("connect", konek);
  socket.on("rwl", RelayWl);
  socket.on("rtemp", RelayTemp);
  socket.on("UbahMode", GantiMode);
  socket.on("NilaiWl", GantiNilaiWl);
  socket.on("NilaiTemp", GantiNilaiTemp);
  socket.on("disconnect", diskonek);

  //listener for socket io end
  if (!RelayStatus1)
  {
    Serial.println("Relay 1 False");
  }
}

void loop()
{
  socket.loop();
  // sensor suhu dan humidity
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  String temp = TangkapNilaiSensor(t);
  String hum = TangkapNilaiSensor(h);

  WaterLv();
  String wlval = TangkapNilaiSensor(distance);
  TdsSensor();
  String tdsVal = TangkapNilaiSensor(tdsValue);

  automatisasi(kalibrasiWl(distance).toFloat(), t); //contoh pemanggilan function otomatisasi nilai float WL, nilai Float Temp

  if (IsConnect)
  {
    //kirim Data Sensor Disini

    KirimSocket("temp", temp);
    KirimSocket("hum", hum);
    KirimSocket("tds", tdsVal);
    KirimSocket("wl", kalibrasiWl(distance));
    //
    //        KirimSocket("temp", String(random(27,28)));
    //        KirimSocket("hum", String(random(66,70)));
    //        KirimSocket("tds", "1000");
    //        KirimSocket("wl", kalibrasiWl(float(random(1,100))));
    // KirimSocket("temp", "27");
    // KirimSocket("hum", String(random(66, 70)));
    // KirimSocket("tds", "1000");
    // KirimSocket("wl", "20");
    //
  }
  else
  {
    Serial.print("temperatur: ");
    Serial.println(temp);
    Serial.print("kelembaban: ");
    Serial.println(hum);
    Serial.print("Tds : ");
    Serial.println(tdsVal);
    Serial.print("water level: ");
    Serial.println(kalibrasiWl(distance));
  }

  delay(1500);
}

void WaterLv()
{
  // sensor ultrasound ketinggian air
  digitalWrite(trigPin, LOW); // Added this line
  delayMicroseconds(2);       // Added this line
  digitalWrite(trigPin, HIGH);

  delayMicroseconds(10); // Added this line
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = (duration / 2) / 29.1;
  distance = max_hight - distance;
  distance = (distance / max_hight) * 100;
}

void TdsSensor() // function tds sensor
{

  gravityTds.setTemperature(temperature); // set the temperature and execute temperature compensation
  gravityTds.update();                    //sample and calculate
  tdsValue = gravityTds.getTdsValue();    // then get the value
}

//function automatisasi
void automatisasi(float nilaiWl, float nilaiTemp)
{
  Serial.println(String(RelayStatus1));
  Serial.println(String(RelayStatus2));
  Serial.println(String(Mode_Prototype));

  if (Mode_Prototype)
  {
    modesekarang = 1;
    if (nilaiWl <= NilaiWl)
    {
      if (!RelayStatus1)
      {
        Serial.println("Relay WL true!!\n");
        JalankanRelay("true", "resWl", relay1);
        RelayStatus1 = true;
      }
    }
    else if (nilaiWl > NilaiWl)
    {
      if (RelayStatus1)
      {
        Serial.println("Relay WL false!!\n");
        JalankanRelay("false", "resWl", relay1);
        RelayStatus1 = false;
      }
    }
    if (nilaiTemp >= NilaiTemp)
    {
      if (!RelayStatus2)
      {
        JalankanRelay("true", "resTemp", relay2);
        RelayStatus2 = true;
      }
    }
    else if (nilaiTemp < NilaiTemp)
    {
      if (RelayStatus2)
      {
        JalankanRelay("false", "resTemp", relay2);
        RelayStatus2 = false;
      }
    }
  }
  else
  {
    if (modesekarang == 0)
    {
      if (RelayStatus1)
      {
        Serial.println("Relay False!!\n");
        JalankanRelay("false", "resWl", relay1);
        RelayStatus1 = false;
      }
      if (RelayStatus2)
      {
        Serial.println("Relay False!!\n");
        JalankanRelay("false", "resTemp", relay2);
        RelayStatus1 = false;
      }
    }
    modesekarang = 1;
  }
}

//Function Function Penting Di Bawah

void konek(const char *payload, size_t length)
{
  socket.emit("new user", "\"P1471984882\"");
  Serial.println("Made Socket Connection");
  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, HIGH);
  IsConnect = true;
}

void GantiNilaiWl(const char *payload, size_t length)
{
  String StringN = String(payload);
  NilaiWl = StringN.toInt();
}
void GantiNilaiTemp(const char *payload, size_t length)
{
  String StringN = String(payload);
  NilaiTemp = StringN.toInt();
}

void JalankanRelay(const char *payload, String NamaSocket, uint8_t pin)
{
  String value = String(payload);

  if (value == "true")
  {
    digitalWrite(pin, LOW);
    KirimSocket(NamaSocket, "true");
    Serial.println("its true");
    if (NamaSocket == "resWl")
    {
      RelayStatus1 = true;
    }
    else
    {
      RelayStatus2 = true;
    }
  }
  else
  {
    if (NamaSocket == "resWl")
    {
      RelayStatus1 = false;
    }
    else
    {
      RelayStatus2 = false;
    }
    digitalWrite(pin, HIGH);
    KirimSocket(NamaSocket, "false");
    Serial.println("its false");
  }
}

void RelayWl(const char *payload, size_t length)
{
  Serial.println(payload);
  if (payload == "true")
  {
    RelayStatus1 = true;
  }
  else
  {
    RelayStatus1 = false;
  }

  JalankanRelay(payload, "resWl", relay1);
}

void RelayTemp(const char *payload, size_t length)
{
  Serial.println(payload);
  if (payload == "true")
  {
    RelayStatus2 = true;
  }
  else
  {
    RelayStatus2 = false;
  }

  JalankanRelay(payload, "resTemp", relay2);
}

void RelayHum(const char *payload, size_t length)
{
  Serial.println(payload);
  JalankanRelay(payload, "resHum", relay1);
}

void GantiMode(const char *payload, size_t length)
{
  String value = String(payload);
  String stat;
  bool statz;
  if (value == "true")
  {
    stat = "true";
    statz = true;
  }
  else
  {
    stat = "false";
    statz = false;
    modesekarang = 0;
  }
  KirimSocket("ModeProto", stat);

  Mode_Prototype = statz;
  Serial.println("mode saat ini\n");
  Serial.println(stat);
}

void diskonek(const char *payload, size_t length)
{
  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, HIGH);
  IsConnect = false;
}

void RelayTds(const char *payload, size_t length)
{
  JalankanRelay(payload, "resTds", relay1);
}

void KirimSocket(String nama, String val)
{
  String Data = "{\"_id\":\"" + id + "\",\"_val\":\"" + val + "\"}";
  socket.emit(nama.c_str(), Data.c_str());
}

String TangkapNilaiSensor(float sensor)
{
  char Var[10];
  dtostrf(sensor, 1, 2, Var);
  String hasil = String(Var);
  return hasil;
}

void SetupRelayAplikasi()
{
  String status1;
  String status2;
  String modeP;
  if (!RelayStatus1)
  {
    status1 = "true";
  }
  else
  {
    status1 = "false";
  }

  if (!RelayStatus2)
  {
    status2 = "true";
  }
  else
  {
    status2 = "false";
  }
  if (Mode_Prototype)
  {
    modeP = "true";
  }
  else
  {
    modeP = "false";
  }
  //  KirimSocket("resTemp", status2);
  //  KirimSocket("resWl", status1);
  JalankanRelay(status2.c_str(), "resTemp", relay2);
  JalankanRelay(status1.c_str(), "resWl", relay1);
  KirimSocket("ModeProto", modeP);
}

//kalibrasi sensor Water Level
String kalibrasiWl(float wl)
{
  String hasil;
  if (currWl < 1)
  {
    if (wl < 40)
    {
      currWl = wl;
      hasil = String(wl);
    }
    else
    {
      currWl = 30;
      hasil = "30";
    }
  }
  else
  {
    if (abs(currWl - wl) > nilaiToleransiWl)
    {
      hasil = String(currWl);
    }
    else if (wl < 1)
    {
      hasil = String(currWl);
    }
    else if (wl > 99)
    {
      hasil = String(currWl);
    }
    else
    {
      hasil = String(wl);
      currWl = wl;
    }
  }
  return hasil;
}

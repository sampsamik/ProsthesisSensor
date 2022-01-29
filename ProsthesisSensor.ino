#include <Adafruit_NeoPixel.h>
#include <HX711.h>
#include <SPI.h>
#include <SD.h>

#define Led_ring 7
#define Buttonpin 6
#define Buttonpin2 5
#define pot A0
#define NUMPIXELS 12

const int record_light_intensity = 240;

int recordData(int pot_val_, int flush_it = 100);
void setLightColor(int r, int g, int b, int num_pixels = NUMPIXELS);
void setLightColor_wDelay(int r, int g, int b, int delay_light, int num_pixels = NUMPIXELS);
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

File dataFile; // initialize sd file
//String fileNumber = "000";
//String fileName;

const int LOADCELL_DOUT_PIN = 3;
const int LOADCELL_SCK_PIN = 2;
const int MAPPin = A2;
const int chipSelect = 9; // CS pin on sd card module
int prev_file_indx = 0;   // used for file naming
int mode = 1;
int prev_mode = 0;
int prev_pot_val = 0;
unsigned long millis_print = 1;

unsigned int print_it = 0;
unsigned long prev_millis = 0;
float frequency = 0.0;

unsigned long mode_button_millis = 0;
bool print_HX711 = 0;
bool pressure_indicator_on = 0;

Adafruit_NeoPixel pixels(NUMPIXELS, Led_ring, NEO_GRB + NEO_KHZ800);

HX711 scale;

#define DELAYVAL 10

void setup()
{
  Serial.begin(115200);
  Serial.println("Reset");

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  //scale.set_scale(-38000); //TODO: fix scale factor
  //scale.tare(); //TODO: how and when to tare?

  pinMode(MAPPin, INPUT);
  pinMode(Buttonpin, INPUT);
  pinMode(Buttonpin2, INPUT);
  pinMode(pot, INPUT);
  pinMode(Led_ring, OUTPUT);
  pixels.begin();
  delay(800);
  setLightColor(0, 0, 0);
  delay(500);

  // verify SD card is working
  if (!SD.begin(chipSelect))
  {
    Serial.println("SD begin fail");
    setLightColor(60, 0, 0);
    mode = -1;
  }
  else
  {
    setLightColor(0, 60, 0); //GREEN: ready to go
    mode = 6;
    Serial.println("Files in SD card:");
    File dir = SD.open("/");
    while (true)
    {
      File entry = dir.openNextFile();
      if (!entry)
      {
        break;
      }
      Serial.print(entry.name());
      Serial.print(" ");
      Serial.print((float)entry.size() / 1000.0);
      Serial.println("kB");
      entry.close();
    }
    dir.close();
    SD.end();
  }

  delay(1000);
}

void loop()
{
  bool mode_button = digitalRead(Buttonpin);
  bool start_button = digitalRead(Buttonpin2);
  int pot_val = map(analogRead(pot), 0, 1023, 0, 255);

  //Start recording when start_button is pressed and released
  if (!start_button)
  {
    delay(200);
    //Do nothing while button is pressed down
    while (!digitalRead(Buttonpin2))
    {
      delay(200);
    }

    mode = recordData(pot_val, 750);

    //Record and save data to SD
    //mode = recordData(pot_val);
    prev_mode = 0;
    pressure_indicator_on = 0;
  }
  else if (!(millis() % millis_print)) //Print every millis_print
  {
    //Print sensor vals to serial monitor for debugging etc.
    Serial.print(millis());
    Serial.print(" ");
    Serial.print(frequency);
    Serial.print("Hz ");
    Serial.print(pot_val);
    Serial.print(" ");
    Serial.print(mode);
    Serial.print(" ");
    Serial.print(MAPread(MAPPin));
    Serial.print("kPa ");
    if (print_HX711)
      Serial.println(scale.read());
    else
      Serial.println("<go to yellow light then hold mode button for min. 2s to show HX711 data>");

    ++print_it;
    if (print_it == 20)
    {
      frequency = 1000.0 * 20.0 / (millis() - prev_millis);
      print_it = 0;
      prev_millis = millis();
    }
  }

  if (pressure_indicator_on)
  {
    float kpa_limits[] = {20.0, 58.0, 60.0, 62.0, 105.0};
    float kPa = MAPread(MAPPin);
    if (kPa <= kpa_limits[1])
    {
      int light_intensity = mapfloat(kPa, kpa_limits[1], kpa_limits[0], 15.0, 120.0);
      setLightColor(light_intensity, 0, 0);
    }
    else if (kPa >= kpa_limits[3])
    {
      int light_intensity = mapfloat(kPa, kpa_limits[3], kpa_limits[4], 15.0, 120.0);
      setLightColor(0, 0, light_intensity);
    }
    else if (kPa <= kpa_limits[2])
    {
      int light_intensity = mapfloat(kPa, kpa_limits[1], kpa_limits[2], 15.0, 120.0);
      setLightColor(/*255 - light_intensity*/ 0, light_intensity, 0);
    }
    else
    {
      int light_intensity = mapfloat(kPa, kpa_limits[2], kpa_limits[3], 15.0, 120.0);
      setLightColor(/*light_intensity*/ 0, 255 - light_intensity, 0);
    }
  }

  if (/*mode != -1 &&*/ !mode_button)
  {
    mode_button_millis = millis();
    delay(200);
    //Do nothing while button is pressed down
    while (!digitalRead(Buttonpin))
    {
      delay(200);
    }
    mode = mode % 8 + 1; //Mode iterates between 1-7
  }

  //Do changes only once (potentiometer fluctuate quite a bit...)
  if (prev_mode != mode || abs(prev_pot_val - pot_val) > 7)
  {
    prev_mode = mode;
    prev_pot_val = pot_val;
    switch (mode)
    {
    case -1: //RED - error mode
      Serial.println("SD ERROR");
      setLightColor(pot_val, 0, 0);
      break;

    case 1:
      setLightColor(pot_val, pot_val, 0);
      break;

    case 2:
      if (millis() - mode_button_millis >= 2000)
      {
        print_HX711 = !print_HX711;
        mode = 1;
        prev_mode = 1;
      }
      else
        setLightColor(0, 0, pot_val);
      break;

    case 3:
      if (millis() - mode_button_millis >= 2000)
      {
        millis_print = (millis_print == 3000) ? 1 : 3000;
        mode = 2;
        prev_mode = 2;
      }
      else
      {
        setLightColor(pot_val, 0, pot_val);
        if (print_HX711)
          scale.tare();
      }
      break;

    case 4:
      setLightColor(pot_val, pot_val, pot_val);
      break;

    case 5:
      setLightColor(0, pot_val, pot_val);
      break;

    case 6:
      setLightColor(0, pot_val, 0);
      break;

    case 7:
      pressure_indicator_on = 1;
      setLightColor(0, 0, 0);
      break;

    case 8:
      pressure_indicator_on = 0;
      setLightColor(0, 0, 0);
      // verify SD card is working
      mode = 1;
      int it_sd = 0;
      int it_sd_lim = 5;
      while (!SD.begin(chipSelect))
      {
        SD.end();
        setLightColor_wDelay(pot_val * ((float)(it_sd + 1) / (float)(it_sd_lim + 1)), 0, 0, 400 / NUMPIXELS);
        ++it_sd;
        if (it_sd == it_sd_lim)
        {
          Serial.println("SD begin fail");
          setLightColor(60, 0, 0);
          mode = -1;
          break;
        }
      }
      if (it_sd < it_sd_lim)
      {
        for (int i = 0; i < 8; ++i)
        {
          setLightColor(0, pot_val * (i % 2), 0);
          delay(100);
        }
      }
      break;

    default:
      break;
    }
  }
}

String getFileNumber(File dirr)
{
  String fileNumber = "000";
  Serial.println("Files in SD card:");
  while (true)
  {
    // iterate all files to ensure no overwrites
    File entry = dirr.openNextFile();
    if (!entry)
    {
      break;
    }
    // naming routine
    String entry_name = entry.name();
    Serial.print(entry.name());
    Serial.print(" ");
    Serial.print((float)entry.size() / 1000.0);
    Serial.println("kB");
    if ((entry_name.substring(4, 7)).toInt() >= prev_file_indx)
    {
      prev_file_indx = (entry_name.substring(4, 7)).toInt() + 1;
      if (prev_file_indx >= 100)
      {
        fileNumber = String(prev_file_indx);
      }
      else if (prev_file_indx >= 10)
      {
        fileNumber = "0" + String(prev_file_indx);
      }
      else
      {
        fileNumber = "00" + String(prev_file_indx);
      }
    }
    entry.close();
  }
  Serial.println();
  return fileNumber;
}

float MAPread(int MAP_pin)
{
  return analogRead(MAP_pin) * 0.00488 / 0.022 + 20; //kPa
}

void setLightColor(int r, int g, int b, int num_pixels)
{
  //int light_ids[]={0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  //int light_ids[] = {0, 1, 2, 3, 4, 5, 6, 10, 11};

  /*for (int i = 0; i < sizeof(light_ids) / sizeof(light_ids[0]); ++i)
  {
    pixels.setPixelColor(light_ids[i], pixels.Color(r, g, b));
  }*/

  for (int i = 0; i < num_pixels; i++)
  {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }

  pixels.show();
}
void setLightColor_wDelay(int r, int g, int b, int delay_light, int num_pixels)
{
  for (int i = 0; i < num_pixels; i++)
  {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
    delay(delay_light);
    pixels.show();
  }
}

int recordData(int pot_val_, int flush_it)
{

  if (!SD.begin(chipSelect))
  {
    Serial.println("SD could not initiate");
    setLightColor(pot_val_, 0, 0);
    return -1;
  }

  String fileNumber = getFileNumber(SD.open("/"));
  String fileName = "DATA" + fileNumber + ".csv";
  Serial.print("Writing to ");
  Serial.println(fileName);
  File dataFile_ = SD.open(fileName, FILE_WRITE);
  if (!dataFile_)
  {
    Serial.print("Could not open ");
    Serial.println(fileName);
    return -1;
  }

  //Turn off light for 5s
  pixels.clear();
  pixels.show();
  delay(5000);
  scale.tare();

  //Turn on "blitz" for 1s (time starts just before the light for Gopro)
  unsigned long start_millis = millis();
  setLightColor(255, 255, 255);
  delay(1000);

  //Turn on light for gopro recording. TODO: find fixed value so it doesnt change!
  setLightColor(record_light_intensity, record_light_intensity, 0);

  //Record until start button is pressed again. TODO: add timeout?
  //This while loop must be as efficient as possible
  int write_it = 0;
  while (digitalRead(Buttonpin2))
  {
    //Record data
    String data_array = String(millis() - start_millis) + "," + String(MAPread(MAPPin)) + "," + String(scale.read());

    if (dataFile_)
    {
      dataFile_.println(data_array);
      ++write_it;
    }
    else
    {
      dataFile_.close();
      SD.end();
      delay(200);
      return -1; // TODO: Don't change light color if fail so gopro can continue?
    }
    // Make sure data is written to the card
    if (write_it == flush_it)
    {
      dataFile_.flush();
      write_it = 0;
    }
  }

  Serial.print((float)dataFile_.size() / 1000.0);
  Serial.println("kB");
  dataFile_.close();
  SD.end();
  delay(1000);
  return 6;
}

/*
ERROR checking

RED light: SD not responsive.
    - Check if SD card is in place, turn power off and on.
    - Make sure battery has enough power

No light after 5s from starting recording: HX711 not connected.
    - Make sure HX711 is connected
    - Check wires are connected/soldered properly

No response (stuck/slow) when clicking mode/start buttons: trying to read HX711 without it being connected.
    - Same as above

*/
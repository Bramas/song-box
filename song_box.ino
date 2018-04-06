/**************
 *     LCD
 **************/

#include <U8x8lib.h>

U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE); 

#define MAX_LCD_LINE_LENGTH 32 // should be at least 31 in order to read track info

char lcd_line[2][MAX_LCD_LINE_LENGTH] = {"Song BOX!",  "by Quentin Bramas"};
uint8_t lcd_line_length[2]={9, 17};
uint8_t lcd_line_cursor[2]={0};

#define LCD_WIDTH 16
#define LCD_HEIGHT 7



/******************
 *     SD and MP3
 ******************/

//Add the SdFat Libraries

#include <SPI.h>
#include <SdFat.h>
#include <FreeStack.h>

//and the MP3 Shield Library
#include <vs1053_SdFat.h>

#define USE_MP3_REFILL_MEANS USE_MP3_Polled

SdFat sd;
vs1053 MP3player;
char current_folder[11] = {};
uint16_t current_index_in_folder = 0;

enum mp3_state_t {
  Stopped,
  Playing
};

byte mp3_state = Stopped;



#define RIGHT_PIN 3
#define LEFT_PIN 4

uint8_t buttonStateRight = 0;
uint8_t buttonStateLeft = 0;


#define REFILL_DELAY 10
unsigned long last_refill = millis();

/**************
 *     RFID
 **************/

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 5
 
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
 /*MFRC522::MIFARE_Key key; 

*/


void setup() {
 uint8_t result; //result code from some function as to be tested at later time.

  Serial.begin(115200);

  Serial.print(F("F_CPU = "));
  Serial.println(F_CPU);
  Serial.print(F("Free RAM = ")); // available in Version 1.0 F() bases the string to into Flash, to use less SRAM.
  Serial.print(FreeStack(), DEC);  // FreeRam() is provided by SdFatUtil.h
  Serial.println(F(" Should be a base line of 1017, on ATmega328 when using INTx"));

  Serial.println("Selection Pin SD");
  Serial.println(SD_SEL);  
  Serial.println(MP3_XCS);  
  Serial.println(MP3_XDCS);  
  Serial.println(MP3_DREQ);  
  // init button
  
  pinMode(RIGHT_PIN, INPUT);
  pinMode(LEFT_PIN, INPUT);
  
  delay(50);
  //Initialize the SdCard.
  if(!sd.begin(SD_SEL, SPI_FULL_SPEED)) sd.initErrorHalt();
  
  // depending upon your SdCard environment, SPI_HAVE_SPEED may work better.
  if(!sd.chdir()) sd.errorHalt("sd.chdir");

  delay(100);
  
  //Initialize the MP3 Player Shield
  result = MP3player.begin();
  //check result, see readme for error codes.
  if(result != 0) {
    Serial.print(F("Error code: "));
    Serial.print(result);
    Serial.println(F(" when trying to start MP3 player"));
    if( result == 6 ) {
      Serial.println(F("OK"));
      //Serial.println(F("Warning: patch file not found, skipping.")); // can be removed for space, if needed.
      //Serial.println(F("Use the \"d\" command to verify SdCard can be read")); // can be removed for space, if needed.
    }
  }
  sd.ls();
/*
  // Typically not used by most shields, hence commented out.
  Serial.println(F("Applying ADMixer patch."));
  if(MP3player.ADMixerLoad("admxster.053") == 0) {
    Serial.println(F("Setting ADMixer Volume."));
    MP3player.ADMixerVol(-3);
  }
*/


   MP3player.setVolume(55<<1, 55<<1); // commit new volume
  
   
   initLCD();

  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 
}


void loop() {

  if(Serial.available())
  {
    playFolder(0);
    Serial.read();
  }
  
  printLCD();

  buttonEvents();

  if(RFIDEvents())
  {
    playFolder(0);
  }
  
  if(millis() - last_refill > REFILL_DELAY)
  {
    last_refill = millis();
    if(MP3player.isPlaying()) 
    {
      vs1053::available(); //Refill the MP3 Player;
    }
    else if(MP3player.getState() == ready && mp3_state == Playing)
    {
      playNext();
    }
  }
  
}

/* return 1 if the folder to play has changed
 *  
 */
byte RFIDEvents()
{
  // Look for new cards
  if ( ! rfid.PICC_IsNewCardPresent())
    return 0;
  
  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return 0;

  byte ret = 0;
  char uid[11];

  for(int i = 0; i < 5 && i < rfid.uid.size; i++)
  {
      uid[2*i] = (rfid.uid.uidByte[i] >> 4) + 0x30;
      if (uid[2*i] > 0x39) uid[2*i] +=7;
      
      uid[2*i+1] = (rfid.uid.uidByte[i] & 0x0f) + 0x30;
      if (uid[2*i+1] > 0x39) uid[2*i+1] +=7;
  }
  uid[min(10,rfid.uid.size*2)] = '\0';
  
  if(strncmp(uid, current_folder, 10))
  {
    Serial.println(F("A new card has been detected."));
   
    Serial.println(F("The NUID tag is:"));
    Serial.print(F("In hex: "));
    Serial.print(uid);
    Serial.println();
    sd.chdir(true);
    if(!sd.exists(uid)) {
      Serial.println(F("Folder does not exists"));
      newAffectation(uid);
    }
    else
    {
      strncpy(current_folder, uid, 10);
      current_folder[10] = '\0';
      //current_index_in_folder = 0;
      ret = 1;
    }
  }
  else 
  {
    Serial.println(F("Already playing."));
  }
  
  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
  
  return ret;
}

void buttonEvents()
{
  
  int prevState = digitalRead(RIGHT_PIN);
  if(buttonStateRight != prevState)
  {
     buttonStateRight = prevState;
     if(buttonStateRight == HIGH) {
        playNext();
     }
  }

  prevState = digitalRead(LEFT_PIN);
  
  if(buttonStateLeft != prevState)
  {
     buttonStateLeft = prevState;
     if(buttonStateLeft == HIGH) {
        playPrevious();
     }
  }
}

void playNext() {
  current_index_in_folder++;
  playCurrent();
}


void playPrevious() {
  current_index_in_folder--;
  playCurrent();
}

void playCurrent() {

  mp3_state = Stopped;
  MP3player.stopTrack();

  if(current_folder == 0 || current_index_in_folder < 0) {
    current_index_in_folder = 0;
    return;
  }
  delay(10);
  
  char filename[100];
  {
    SdFile file;
    sd.chdir(true);
    sd.chdir(current_folder, true);
    uint16_t count = 1;
    Serial.print(F("play "));
    Serial.print(current_index_in_folder);
    Serial.print(F(" in "));
    Serial.println(current_folder);
    delay(10);
    byte found = 0;
    while (file.openNext(sd.vwd(),O_READ))
    {
      file.getName(filename, sizeof(filename));
      if (!file.isHidden() && isFnMusic(filename) ) {
        
        if (count == current_index_in_folder) {
          found = 1;
          break;
        }
        count++;
      }
      file.close();
    }
    if(!found)
    {
      current_index_in_folder = count - 1;
      return;
    }
  }
  Serial.println(filename);
  int8_t result = MP3player.playMP3(filename);
  //check result, see readme for error codes.
  if(result != 0) {
    Serial.print(F("Error code: "));
    Serial.print(result);
  }
  else 
  {
    MP3player.trackTitle((char*)&lcd_line[0]);
    MP3player.trackArtist((char*)&lcd_line[1]); // Those function read at most 30 characters
    lcd_line_length[0] = strlen(lcd_line[0]);
    lcd_line_length[1] = strlen(lcd_line[1]);
  
    lcdResetLines();
    
    mp3_state = Playing;
  }
  
}
/*
int isMP3(char* filename) {
  ifstream sdin(filename);
  char buffer[3] = {};
  for(int i = 0; i < 3 && !sdin.eof(); i++) {
    sdin >> buffer[i];
    Serial.print(F("buffer["));
    Serial.print(i);
    Serial.print(F("] = "));
    Serial.print(buffer[i]);
    Serial.println();
  }
  
  if(sdin.eof()){
    return 0;
  }
  
  if(buffer[0] == 0x49 && buffer[1] == 0x44 && buffer[2] == 0x33){
    return 1;
  }
  if(buffer[0] == 0xff && buffer[1] == 0xfb){
    return 1;
  }
  return 0;
}
*/
void playFolder(char *key) {

  if(key != 0)
  {
    if(!sd.exists(key)) {
      Serial.print(key);
      Serial.println(F("does not exists"));
      return;
    }
    strncpy(current_folder, key, 10);
    current_folder[10] = '\0';
  }
  current_index_in_folder = 0;
  playNext();
}



void newAffectation(const char *key)
{
  SdFile file;
  char filename[17];
  
  sd.chdir(true);
  
  while (file.openNext(sd.vwd(),O_READ))
  {
    file.getName(filename, sizeof(filename));
    filename[16]='\0';
    if(!file.isDir() || filename[0] == '.')
    {
      file.close();
      continue;
    }
    u8x8.clearDisplay();
    u8x8.setCursor(0, 0);
    u8x8.print(F("Affectation"));
    u8x8.setCursor(0, 1);
    u8x8.print(F("nouvelle carte"));
    u8x8.setCursor(0, 3);
    u8x8.print(F("Utiliser ce"));
    u8x8.setCursor(0, 4);
    u8x8.print(F("dossier ?"));
    u8x8.setCursor(0, 5);
    u8x8.print(filename);
    while(1)
    {
      int prevState = digitalRead(RIGHT_PIN);
      if(buttonStateRight != prevState)
      {
         buttonStateRight = prevState;
         if(buttonStateRight == HIGH) {
            break;
         }
      }
      prevState = digitalRead(LEFT_PIN);
      if(buttonStateLeft != prevState)
      {
         buttonStateLeft = prevState;
         if(buttonStateLeft == HIGH) {
            if (!file.rename(sd.vwd(), key)) {
              
              u8x8.clearDisplay();
              u8x8.setCursor(0, 0);
              u8x8.print(F("Affectation"));
              u8x8.setCursor(0, 1);
              u8x8.print(F("nouvelle carte"));
              u8x8.draw2x2UTF8(0,3, (const char*)F("Erreur"));
              u8x8.setCursor(0, 6);
              u8x8.print(F("Operation"));
              u8x8.setCursor(0, 7);
              u8x8.print(F("Annulee"));
            }
            else 
            {
              u8x8.clearDisplay();
              u8x8.setCursor(0, 0);
              u8x8.print(F("Affectation"));
              u8x8.setCursor(0, 1);
              u8x8.print(F("nouvelle carte"));
              u8x8.draw2x2Glyph(0,3,'O');
              u8x8.draw2x2Glyph(2,3,'K');
            }
            delay(2000);
            u8x8.clearDisplay();
            return;
         }
      }
    }
    file.close();
  }

  u8x8.clearDisplay();
  u8x8.setCursor(0, 0);
  u8x8.print(F("Affectation"));
  u8x8.setCursor(0, 1);
  u8x8.print(F("nouvelle carte"));
  u8x8.setCursor(0, 3);
  u8x8.print(F("Il n'y a pas"));
  u8x8.setCursor(0, 4);
  u8x8.print(F("d'autre dossier"));
  u8x8.setCursor(0, 6);
  u8x8.print(F("Operation"));
  u8x8.setCursor(0, 7);
  u8x8.print(F("Annulee"));

  delay(2000);
  u8x8.clearDisplay();

}





#define LCD_PRINT_DELAY 300
unsigned long lastLcdPrint = millis() - LCD_PRINT_DELAY;
#define LCD_SCROLL_WAITING_TIME_AT_START 5
#define LCD_SCROLL_WAITING_TIME_AT_END 5


void lcdResetLines() 
{
  lcd_line_cursor[0] = 0;
  lcd_line_cursor[1] = 0;
}


void printLCD()
{  
  
   if (millis() - lastLcdPrint >= LCD_PRINT_DELAY)
   {
      u8x8.setFont(u8x8_font_victoriabold8_r);
      
      lastLcdPrint = millis();
      int y = 3;
      for(int l = 0; l < 2; l++)
      {
        int multiplier = 1;
        if(l==0)
        {
          multiplier = 2;
        }
        //u8x8.setCursor(0,y);
        int offset = 0;
        
        if(lcd_line_length[l]*multiplier > LCD_WIDTH)
        {
          lcd_line_cursor[l] = (lcd_line_cursor[l] + 1) % (lcd_line_length[l]*multiplier - LCD_WIDTH + LCD_SCROLL_WAITING_TIME_AT_END + LCD_SCROLL_WAITING_TIME_AT_START);
          
          offset = (lcd_line_cursor[l] - LCD_SCROLL_WAITING_TIME_AT_START)/multiplier;

          offset = max(offset, 0);
          offset = min(offset, (lcd_line_length[l]*multiplier - LCD_WIDTH)/multiplier);
          
        }
        
        int x_offset = (lcd_line_cursor[l] - LCD_SCROLL_WAITING_TIME_AT_START);
        x_offset = max(x_offset, 0);
        x_offset = min(x_offset, (lcd_line_length[l]*multiplier - LCD_WIDTH));
        x_offset = x_offset%2;

        for(int i = 0; i < LCD_WIDTH/multiplier; i++) 
        {
          if(offset + i < lcd_line_length[l])
          {
            if(l == 0)
            {
              
              u8x8.draw2x2Glyph(2*i - x_offset, y, lcd_line[l][offset + i]); //-((lcd_line_cursor[l] - LCD_SCROLL_WAITING_TIME_AT_START)%2+1)
            } 
            else 
            {
              u8x8.drawGlyph(i,y, lcd_line[l][offset + i]);
            }
          }
          else
          {
            
            if(l == 0)
            {
              u8x8.draw2x2Glyph(2*i-(offset%2),y,' ');
            } 
            else 
            {
              u8x8.drawGlyph(i,y, ' ');
            }
          }
          
          if(multiplier == 2 && i == 0 && x_offset == 1) //fix half of the first letter at the end of the screen
          {
            u8x8.drawGlyph(LCD_WIDTH-1, y,' ');
            u8x8.drawGlyph(LCD_WIDTH-1, y+1,' ');
          }
        }
        y+=multiplier;
      }
   }
}

void initLCD() 
{
  u8x8.begin();
  u8x8.setPowerSave(0);
}


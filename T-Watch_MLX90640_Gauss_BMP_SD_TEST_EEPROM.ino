#include <Wire.h>
#include <TTGO.h>
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include "SPI.h"
//#include "Adafruit_GFX.h"
//#include "Adafruit_ILI9341.h"
#include "Blur.h"
#include <EEPROM.h>            // read and write from flash memory

// define the number of bytes you want to access
#define EEPROM_SIZE 1

TTGOClass *ttgo;

const byte MLX90640_address = 0x33;           //Default 7-bit unshifted address of the MLX90640

#define TA_SHIFT 8                            //Default shift for MLX90640 in open air

static float mlx90640To[768];
static float mlx90640ToBlur[3072];
paramsMLX90640 mlx90640;

int R_colour, G_colour, B_colour;              // RGB-Farbwert
int i, j;                                      // Zählvariable
float T_max, T_min;                            // maximale bzw. minimale gemessene Temperatur
float T_center;                                // Temperatur in der Bildschirmmitte
uint16_t tft_width  = 240;                     // ST7789_TFTWIDTH;
uint16_t tft_height = 240;                     // ST7789_TFTHEIGHT;
String path;
int pictureNumber = 1;

//----------------------------------
const uint16_t disp_width_pix = 32, disp_height_pix = 24;
const uint16_t max_x = disp_width_pix - 1;
const uint16_t max_y = disp_height_pix - 1;
const uint16_t max_w_pix_buf = disp_width_pix * 2;
uint8_t bmp_data_buf[disp_height_pix][max_w_pix_buf] = {};
//----------------------------------
//boolean canStartStream = false;
//boolean canSendImage = false;
//boolean shouldClear = true;
//uint32_t frame_last_time = 0;                 //for display FPS
//uint32_t draw_time = 0;
//------Initialize bitmap data------
const uint16_t data_size = disp_width_pix * 2 * disp_height_pix;
const uint8_t data_size_lsb = (uint8_t)(0x00ff & data_size);
const uint8_t data_size_msb = (uint8_t)(data_size >> 8); 
const uint8_t bmp_head_bytes = 66;
const uint16_t file_size = bmp_head_bytes + data_size;
const uint8_t file_size_lsb = (uint8_t)(0x00ff & file_size);
const uint8_t file_size_msb = (uint8_t)(file_size >> 8);
const uint8_t info_header_size = 0x28;          //情報ヘッダサイズは常に40byte = 0x28byte
const uint8_t bits_per_pixel = 16;              //色ビット数=16bit(0x10)
const uint8_t compression = 3;                  //色ビット数が16bitの場合、マスクを設定するので3にする。
const uint8_t red_mask[2] =   {0b11111000, 0b00000000};
const uint8_t green_mask[2] = {0b00000111, 0b11100000};
const uint8_t blue_mask[2] =  {0b00000000, 0b00011111}; 
//※Bitmap file headerは全てリトルエンディアン
const uint8_t bmp_header[bmp_head_bytes]=
    {0x42, 0x4D,
     file_size_lsb, file_size_msb, 0, 0,
     0, 0, 0, 0,
     bmp_head_bytes, 0, 0, 0,
     info_header_size, 0, 0, 0,
     disp_width_pix, 0, 0, 0,
     disp_height_pix, 0, 0, 0,
     1, 0, bits_per_pixel, 0,
     compression, 0, 0, 0,
     data_size_lsb, data_size_msb, 0, 0,
     0,0,0,0,
     0,0,0,0,
     0,0,0,0,
     0,0,0,0,
     red_mask[1], red_mask[0], 0, 0,
     green_mask[1], green_mask[0], 0, 0,
     blue_mask[1], blue_mask[0], 0, 0};
//*********************************************

GBlur blur;                                       //Gauss interpolation class

// ***************************************
// **************** SETUP ****************
// ***************************************

void writeBMP(fs::FS &fs, const char * path){
    Serial.printf("Writing file: %s\n", path);
    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    file.write(bmp_header, bmp_head_bytes);
    for(int i = disp_height_pix - 1; i >= 0; i--){
      file.write(&bmp_data_buf[i][0], max_w_pix_buf);
      }
    file.close();
    }

void pressed(){
    EEPROM.begin(EEPROM_SIZE);
    pictureNumber = EEPROM.read(0) + 1;
    path = "/picture" + String(pictureNumber) +".bmp";
    writeBMP(SD, path.c_str());
    Serial.print(path);
    Serial.println(" saved");
    EEPROM.write(0, pictureNumber);
    EEPROM.commit();
    //pictureNumber++;
}    

void setup(){
    Serial.begin(115200);
    ttgo = TTGOClass::getWatch();
    ttgo->begin();
    ttgo->button->setPressedHandler(pressed);
    // initialize EEPROM with predefined size

    while (1) {
        if (ttgo->sdcard_begin()) {
            Serial.println("sd begin pass");
            break;
        }
        Serial.println("sd begin fail,wait 1 sec");
        delay(1000);
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return;
    }
    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    Wire.begin();
    Wire.setClock(400000); //Increase I2C clock speed to 400kHz
    while (!Serial); //Wait for user to open terminal
   
    Serial.println("MLX90640 IR Array Example");

    if (isConnected() == false)
       {
        Serial.println("MLX90640 not detected at default I2C address. Please check wiring. Freezing.");
        while (1);
       }
       
    Serial.println("MLX90640 online!");

    //Get device parameters - We only have to do this once
    int status;
    uint16_t eeMLX90640[832];
    status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
    if (status != 0)
       Serial.println("Failed to load system parameters");
    status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640); 
    if (status != 0)
       {
        Serial.println("Parameter extraction failed");
        Serial.print(" status = ");
        Serial.println(status);
       }

    //Once params are extracted, we can release eeMLX90640 array

    MLX90640_I2CWrite(0x33, 0x800D, 6401);    // writes the value 1901 (HEX) = 6401 (DEC) in the register at position 0x800D to enable reading out the temperatures!!!
    // ===============================================================================================================================================================

    //MLX90640_SetRefreshRate(MLX90640_address, 0x00); //Set rate to 0.25Hz effective - Works
    //MLX90640_SetRefreshRate(MLX90640_address, 0x01); //Set rate to 0.5Hz effective - Works
    //MLX90640_SetRefreshRate(MLX90640_address, 0x02); //Set rate to 1Hz effective - Works
    //MLX90640_SetRefreshRate(MLX90640_address, 0x03); //Set rate to 2Hz effective - Works
    MLX90640_SetRefreshRate(MLX90640_address, 0x04); //Set rate to 4Hz effective - Works
    //MLX90640_SetRefreshRate(MLX90640_address, 0x05); //Set rate to 8Hz effective - Works at 800kHz
    //MLX90640_SetRefreshRate(MLX90640_address, 0x06); //Set rate to 16Hz effective - Works at 800kHz
    //MLX90640_SetRefreshRate(MLX90640_address, 0x07); //Set rate to 32Hz effective - fails

    ttgo->openBL();
    ttgo->lvgl_begin();
    //ttgo->eTFT->begin();

    ttgo->eTFT->setRotation(0);
    ttgo->eTFT->fillScreen(TFT_BLACK);

    // draw scale
    for (int x = 30; x <= 210; x +=30){
      ttgo->eTFT->drawFastVLine(x, 210, 8, TFT_WHITE);
    }

    ttgo->eTFT->setCursor(80, 10);
    ttgo->eTFT->setTextColor(TFT_WHITE, ttgo->eTFT->color565(0, 0, 0));
    ttgo->eTFT->print("T+ = ");    

    // drawing the colour-scale
    // ========================
 
    for (i = 0; i < 181; i++)
       {
        //value = random(180);     
        getColour(i);
        ttgo->eTFT->drawLine(30 + i , 218, 30 + i, 228, ttgo->eTFT->color565(R_colour, G_colour, B_colour));
       } 
   } 

// **********************************
// ************** LOOP **************
// **********************************

void loop(){
    ttgo->button->loop();
    for (byte x = 0 ; x < 2 ; x++) //Read both subpages
       {
        uint16_t mlx90640Frame[834];
        int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
    
        if (status < 0){
            Serial.print("GetFrame Error: ");
            Serial.println(status);
           }
        float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
        float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);
        float tr = Ta - TA_SHIFT; //Reflected temperature based on the sensor ambient temperature
        float emissivity = 0.95;
        MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, mlx90640To);
       }

       
    // determine T_min and T_max and eliminate error pixels
    // ====================================================

    mlx90640To[1*32 + 21] = 0.5 * (mlx90640To[1*32 + 20] + mlx90640To[1*32 + 22]);    // eliminate the error-pixels
    mlx90640To[4*32 + 30] = 0.5 * (mlx90640To[4*32 + 29] + mlx90640To[4*32 + 31]);    // eliminate the error-pixels
    
    T_min = mlx90640To[0];
    T_max = mlx90640To[0];

    for (i = 1; i < 768; i++){
        if((mlx90640To[i] > -41) && (mlx90640To[i] < 301)){
            if(mlx90640To[i] < T_min){
                T_min = mlx90640To[i];
               }
            if(mlx90640To[i] > T_max){
                T_max = mlx90640To[i];
               }
           }
        else if(i > 0){   // temperature out of range         
            mlx90640To[i] = mlx90640To[i-1];
           }
        else{
            mlx90640To[i] = mlx90640To[i+1];
           }
       }

    // determine T_center
    // ==================
    T_center = mlx90640To[11* 32 + 15];    

    // drawing the picture
    // ===================

   //blur.calculate(mlx90640To, mlx90640ToBlur);

   for (i = 0 ; i < 24 ; i++){
        for (j = 0; j < 32; j++){
            mlx90640To[i*32 + j] = 180.0 * (mlx90640To[i*32 + j] - T_min) / (T_max - T_min);                       
            getColour(mlx90640To[i*32 + j]);            
            ttgo->eTFT->fillRect(225 - j * 7, 30 + i * 7, 7, 7, ttgo->eTFT->color565(R_colour, G_colour, B_colour));
            //drawRectangleFill(187 - j * 6, i * 6, (187 - j * 6) + 5, i * 6 + 5, R_colour, G_colour, B_colour);   //fill BMP array: width = 32*6, height = 24 * 6
            drawRectangleFill(31 - j, i, (31 - j) + 1, i + 1, R_colour, G_colour, B_colour);   //fill BMP array: width = 32, height = 24 
           }
       }
       
    /*for (i = 0 ; i < 48 ; i++){
        for (j = 0; j < 64; j++){
            mlx90640ToBlur[i*64 + j] = 180.0 * (mlx90640ToBlur[i*64 + j] - T_min) / (T_max - T_min);                       
            getColour(mlx90640ToBlur[i*64 + j]);            
            //ttgo->eTFT->fillRect(217 - j * 7, 35 + i * 7, 7, 7, ttgo->eTFT->color565(R_colour, G_colour, B_colour));
            ttgo->eTFT->fillRect(213 - j * 3, 30 + i * 3, 3, 3, ttgo->eTFT->color565(R_colour, G_colour, B_colour));
            drawRectangleFill(187 - j * 6, i * 6, (187 - j * 6) + 5, i * 6 + 5, R_colour, G_colour, B_colour);   //fill BMP array
           }
       }
     */     
    //draw cross
    ttgo->eTFT->drawFastHLine(tft_width/2 - 2, tft_height/2, 5, TFT_BLACK);
    ttgo->eTFT->drawFastVLine(tft_width/2, tft_height/2 - 2, 5, TFT_BLACK);
    
 
    ttgo->eTFT->fillRect(260, 25, 37, 10, ttgo->eTFT->color565(0, 0, 0));
    ttgo->eTFT->fillRect(260, 205, 37, 10, ttgo->eTFT->color565(0, 0, 0));    
    ttgo->eTFT->fillRect(115, 10, 37, 10, ttgo->eTFT->color565(0, 0, 0));    

    ttgo->eTFT->setTextColor(TFT_WHITE, ttgo->eTFT->color565(0, 0, 0));
    ttgo->eTFT->setCursor(200, 200);
    ttgo->eTFT->print(T_max, 1);
    ttgo->eTFT->setCursor(10, 200);
    ttgo->eTFT->print(T_min, 1);
    ttgo->eTFT->setCursor(120, 10);
    ttgo->eTFT->print(T_center, 1);

    ttgo->eTFT->setCursor(230, 200);
    ttgo->eTFT->print("C");
    ttgo->eTFT->setCursor(35, 200);
    ttgo->eTFT->print("C");
    ttgo->eTFT->setCursor(155, 10);
    ttgo->eTFT->print("C");
    
    delay(20);
   }

void clearAll(){
  memset(bmp_data_buf, 0, disp_height_pix * max_w_pix_buf);
}
//*********************************************
void drawRectangleFill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t red, uint8_t green, uint8_t blue){
  uint8_t rgb565_msb = 0, rgb565_lsb = 0;
  convertRGB888toRGB565(red, green, blue, rgb565_msb, rgb565_lsb);
  for(int i = x0; i <= x1; i++){
    drawVerticalLine565(i, y0, y1, rgb565_msb, rgb565_lsb);
  }
}
//*********************************************
void drawVerticalLine565(uint16_t x0, uint16_t y0, uint16_t y1, uint8_t rgb565_msb, uint8_t rgb565_lsb){
  judgeMaxPixel(x0, max_x);
  judgeMaxPixel(y0, max_y);
  judgeMaxPixel(y1, max_y);
  uint16_t x01 = x0 * 2;
  uint16_t x02 = x0 * 2 + 1;
  if(y0 > y1) std::swap(y0, y1);
  for(uint16_t i = y0; i <= y1; i++){
    bmp_data_buf[i][x01] = rgb565_lsb;
    bmp_data_buf[i][x02] = rgb565_msb;
  }
}
//*********************************************
void convertRGB888toRGB565(uint8_t red888, uint8_t green888, uint8_t blue888, uint8_t &rgb565_msb, uint8_t &rgb565_lsb){
  //RGB888をRGB565へ変換するには、下位ビットを削除するだけでＯＫ。
  uint8_t red565 = red888 & 0b11111000;
  uint8_t green565 = green888 & 0b11111100;
  uint8_t blue565 = blue888 & 0b11111000;
  rgb565_msb = red565 | (green565 >> 5);
  rgb565_lsb = (green565 << 3) | (blue565 >> 3);
}
//*********************************************
void judgeMaxPixel(uint16_t &pix, uint16_t max_pix){
  if(pix > max_pix){
    //Serial.printf("Over Max pix = %d\r\n", pix);
    pix = max_pix;
  }
}

// ===============================
// ===== determine the colour ====
// ===============================
void getColour(int j){
    if (j >= 0 && j < 30){
        R_colour = 0;
        G_colour = 0;
        B_colour = 20 + (120.0/30.0) * j;
       }    
    if (j >= 30 && j < 60){
        R_colour = (120.0 / 30) * (j - 30.0);
        G_colour = 0;
        B_colour = 140 - (60.0/30.0) * (j - 30.0);
       }
    if (j >= 60 && j < 90){
        R_colour = 120 + (135.0/30.0) * (j - 60.0);
        G_colour = 0;
        B_colour = 80 - (70.0/30.0) * (j - 60.0);
       }
    if (j >= 90 && j < 120){
        R_colour = 255;
        G_colour = 0 + (60.0/30.0) * (j - 90.0);
        B_colour = 10 - (10.0/30.0) * (j - 90.0);
       }
    if (j >= 120 && j < 150){
        R_colour = 255;
        G_colour = 60 + (175.0/30.0) * (j - 120.0);
        B_colour = 0;
       }
    if (j >= 150 && j <= 180){
        R_colour = 255;
        G_colour = 235 + (20.0/30.0) * (j - 150.0);
        B_colour = 0 + 255.0/30.0 * (j - 150.0);
       }
   }
      
//Returns true if the MLX90640 is detected on the I2C bus
boolean isConnected(){
    Wire.beginTransmission((uint8_t)MLX90640_address);
  
    if (Wire.endTransmission() != 0)
       return (false); //Sensor did not ACK
    
    return (true);
   }  


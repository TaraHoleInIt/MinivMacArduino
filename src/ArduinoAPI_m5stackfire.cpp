#include <string.h>

#include <Arduino.h>
#include <Wire.h>

#include <FS.h>
#include <SPIFFS.h>
#include <SD_MMC.h>

#include "M5Stack.h"

#include "ArduinoAPI.h"
#include "ArduinoDraw.h"
#include "ArduinoKeyboard.h"

#include "SYSDEPNS.h"
#include "CNFGGLOB.h"
#include "CNFGRAPI.h"
#include "MYOSGLUE.h"

#define DisplayWidth 320
#define DisplayHeight 240

const int Pin_CS = 1;
const int Pin_DC = 2;
const int Pin_RST = 0;
const int Pin_MISO = 12;
const int Pin_MOSI = 13;
const int Pin_CLK = 14;

volatile bool ShowKeyboard = true;

int MouseX = 0;
int MouseY = 0;

portMUX_TYPE Crit = portMUX_INITIALIZER_UNLOCKED;

#define DrawScreenEvent 0x01

EventGroupHandle_t RenderTaskEventHandle = NULL;
SemaphoreHandle_t RenderTaskLock = NULL;
SemaphoreHandle_t SPIBusLock = NULL;
TaskHandle_t RenderTaskHandle = NULL;

volatile const uint8_t* EmScreenPtr = NULL;

int JoyCenterX = 0;
int JoyCenterY = 0;

int VirtMouseX = 0;
int VirtMouseY = 0;

volatile bool JoyButtonState = false;
volatile bool LastJoyButtonState = false;

volatile float JoyDx = 0.0f;
volatile float JoyDy = 0.0f;

void DrawKeyboard( void );

void M5JoyGetDelta( float* Outdx, float* Outdy, bool* OutButton ) {
    const float AccelValue = 6.0f;
    bool Button = false;
    float dx = 0.0f;
    float dy = 0.0f;
    int X = 0;
    int Y = 0;

    Wire.requestFrom( 0x5E, 5 );

    LastJoyButtonState = JoyButtonState;

    if ( Wire.available( ) ) {
        Y = Wire.read( );
        Y |= ( Wire.read( ) << 8 );

        X = Wire.read( );
        X |= ( Wire.read( ) << 8 );

        Button = ( bool ) ! Wire.read( );

        dx = ( float ) ( X - JoyCenterX );
        dy = ( float ) ( Y - JoyCenterY );

        dx/= 384.0f;
        dy/= 384.0f;

        dx*= AccelValue;
        dy*= AccelValue;

        if ( Outdx ) *Outdx = dx;
        if ( Outdy ) *Outdy = dy;
        if ( OutButton ) *OutButton = Button;
    }    
}

void M5JoyRead( int* OutJoyX, int* OutJoyY, bool* OutButton ) {
    bool Button = false;
    int X = 0;
    int Y = 0;

    Wire.requestFrom( 0x5E, 5 );

    if ( Wire.available( ) ) {
        Y = Wire.read( );
        Y |= ( Wire.read( ) << 8 );

        X = Wire.read( );
        X |= ( Wire.read( ) << 8 );

        Button = ( bool ) ! Wire.read( );
    }

    if ( OutButton != NULL ) {
        *OutButton = Button;
    }

    if ( OutJoyX != NULL ) {
        *OutJoyX = X;
    }

    if ( OutJoyY != NULL ) {
        *OutJoyY = Y;
    }
}

void CenterJoystick( void ) {
    bool Button = false;
    int x = 0;
    int y = 0;

    M5.Lcd.clear( TFT_WHITE );

    M5.Lcd.setTextColor( TFT_BLACK, TFT_WHITE );

    M5.Lcd.setTextFont( 1 );
    M5.Lcd.setTextSize( 1 );

    M5.Lcd.drawCentreString( "Leave the joystick centered.", DisplayWidth/ 2, DisplayHeight / 2, 2 );
    M5.Lcd.drawCentreString( "Press any button to continue.", DisplayWidth / 2, ( DisplayHeight / 2 ) + 16, 2 );

    do {
        M5.update( );
        M5JoyRead( &x, &y, &Button );
        
        delay( 100 );
    }
    while ( ! ( M5.BtnA.pressedFor( 10 ) || M5.BtnA.pressedFor( 10 ) || M5.BtnC.pressedFor( 10 ) ) );
}

void PrintCore( const char* Str ) {
    Serial.print( Str );
    Serial.print( " is on core " );
    Serial.println( xPortGetCoreID( ) );
}

void RenderTask( void* Param ) {
    int x = 0;
    int y = 0;

    PrintCore( __func__ );

    while ( true ) {
        x = MouseX - ( DisplayWidth * 1 ) / 2;
        y = MouseY - ( DisplayHeight * 1 ) / 2;

        x = x < 0 ? 0 : x;
        y = y < 0 ? 0 : y;

        xEventGroupWaitBits( RenderTaskEventHandle, DrawScreenEvent, pdTRUE, pdTRUE, portMAX_DELAY );

        if ( xSemaphoreTake( SPIBusLock, pdMS_TO_TICKS( 5 ) ) == pdTRUE ) {
            xSemaphoreTake( RenderTaskLock, portMAX_DELAY );
                if ( EmScreenPtr ) {
                    DrawWindow( ( const uint8_t* ) EmScreenPtr, x, y );

                    if ( ShowKeyboard ) {
                        DrawKeyboard( );
                    }
                }
            xSemaphoreGive( RenderTaskLock );

            xSemaphoreGive( SPIBusLock );
        }
    }
}

void setup( void ) {
    M5.begin( true, true, true, true );
    M5.Speaker.mute( );

    CenterJoystick( );
    M5JoyRead( &JoyCenterX, &JoyCenterY, ( bool* ) &JoyButtonState );

    M5.Lcd.clear( TFT_BLACK );

    //SPIFFS.begin( );

    Setup1BPPTable( );
    SetupScalingTable( );

    RenderTaskEventHandle = xEventGroupCreate( );
    RenderTaskLock = xSemaphoreCreateMutex( );
    SPIBusLock = xSemaphoreCreateMutex( );

    xTaskCreatePinnedToCore( RenderTask, "RenderTask", 4096, NULL, 0, &RenderTaskHandle, 0 );

    Serial.println( ( int ) RenderTaskEventHandle, 16 );
    Serial.println( ( int ) RenderTaskLock, 16 );
    Serial.println( ( int ) RenderTaskHandle );

    PrintCore( __func__ );

    Serial.print( "Freq: " );
    Serial.println( ( int ) getCpuFrequencyMhz( ) ); 
}

void loop( void ) {
    PrintCore( __func__ );

    minivmac_main( 0, NULL );

    Serial.println( "If we got here, something bad happened." );

    while ( true ) {
        delay( 100 );
    }
}

void ArduinoAPI_GetDisplayDimensions( int* OutWidthPtr, int* OutHeightPtr ) {
    *OutWidthPtr = DisplayWidth;
    *OutHeightPtr = DisplayHeight;
}

void ArduinoAPI_SetAddressWindow( int x0, int y0, int x1, int y1 ) {
    M5.Lcd.setAddrWindow( x0, y0, ( x1 - x0 ), ( y1 - y0 ) );
}

void ArduinoAPI_WritePixels( const uint16_t* Pixels, size_t Count ) {
    M5.Lcd.startWrite( );
        M5.Lcd.writePixels( ( uint16_t* ) Pixels, Count );
    M5.Lcd.endWrite( );
}

void ArduinoAPI_GetMouseDelta( int* OutXDeltaPtr, int* OutYDeltaPtr ) {
    *OutXDeltaPtr = ( int ) JoyDx;
    *OutYDeltaPtr = -( int ) JoyDy;
}

void ArduinoAPI_GiveEmulatedMouseToArduino( int* EmMouseX, int* EmMouseY ) {
    MouseX = *EmMouseX;
    MouseY = *EmMouseY;
}

int ArduinoAPI_GetMouseButton( void ) {
    return JoyButtonState || M5.BtnA.isPressed( );
}

uint64_t ArduinoAPI_GetTimeMS( void ) {
    return ( uint64_t ) millis( );
}

void ArduinoAPI_Yield( void ) {
    yield( );
}

void ArduinoAPI_Delay( uint32_t MSToDelay ) {
    delay( MSToDelay );
}

ArduinoFile ArduinoAPI_open( const char* Path, const char* Mode ) {
    ArduinoFile Handle = NULL;
    char SPIFFSPath[ 256 ];

    snprintf( SPIFFSPath, sizeof( SPIFFSPath ), "/sd/%s", Path );
    
    xSemaphoreTake( SPIBusLock, portMAX_DELAY );
        Handle = ( ArduinoFile ) fopen( SPIFFSPath, Mode );
    xSemaphoreGive( SPIBusLock );

    return Handle;
}

void ArduinoAPI_close( ArduinoFile Handle ) {
    if ( Handle ) {
        xSemaphoreTake( SPIBusLock, portMAX_DELAY );
            fclose( ( FILE* ) Handle );
        xSemaphoreGive( SPIBusLock );
    }
}

size_t ArduinoAPI_read( void* Buffer, size_t Size, size_t Nmemb, ArduinoFile Handle ) {
    size_t BytesRead = 0;

    if ( Handle ) {
        xSemaphoreTake( SPIBusLock, portMAX_DELAY );
            BytesRead = fread( Buffer, Size, Nmemb, ( FILE* ) Handle );
        xSemaphoreGive( SPIBusLock );
    }

    return BytesRead;
}

size_t ArduinoAPI_write( const void* Buffer, size_t Size, size_t Nmemb, ArduinoFile Handle ) {
    size_t BytesWritten = 0;

    if ( Handle ) {
        xSemaphoreTake( SPIBusLock, portMAX_DELAY );
            BytesWritten = fwrite( Buffer, Size, Nmemb, ( FILE* ) Handle );
        xSemaphoreGive( SPIBusLock );
    }

    return BytesWritten;
}

long ArduinoAPI_tell( ArduinoFile Handle ) {
    long Offset = 0;

    if ( Handle ) {
        xSemaphoreTake( SPIBusLock, portMAX_DELAY );
            Offset = ftell( ( FILE* ) Handle );
        xSemaphoreGive( SPIBusLock );
    }

    return Offset;
}

long ArduinoAPI_seek( ArduinoFile Handle, long Offset, int Whence ) {
    long Result = -1;

    if ( Handle ) {
        xSemaphoreTake( SPIBusLock, portMAX_DELAY );
            Result = fseek( ( FILE* ) Handle, Offset, Whence );
        xSemaphoreGive( SPIBusLock );
    }

    return Result;
}

int ArduinoAPI_eof( ArduinoFile Handle ) {
    int IsEOF = 0;

    if ( Handle ) {
        xSemaphoreTake( SPIBusLock, portMAX_DELAY );
            IsEOF = feof( ( FILE* ) Handle );
        xSemaphoreGive( SPIBusLock );
    }

    return IsEOF;
}

void* ArduinoAPI_malloc( size_t Size ) {
    return heap_caps_malloc( Size, ( Size >= 262144 ) ? MALLOC_CAP_SPIRAM : MALLOC_CAP_DEFAULT );
}

void* ArduinoAPI_calloc( size_t Nmemb, size_t Size ) {
    return heap_caps_calloc( Nmemb, Size, ( ( Size * Nmemb ) >= 262144 ) ? MALLOC_CAP_SPIRAM : MALLOC_CAP_DEFAULT );
}

void ArduinoAPI_free( void* Memory ) {
    heap_caps_free( Memory );
}

bool Changed = false;

void ArduinoAPI_CheckForEvents( void ) {
    M5.update( );
    M5JoyGetDelta( ( float* ) &JoyDx, ( float* ) &JoyDy, ( bool* ) &JoyButtonState );

    if ( M5.BtnB.wasReleasefor( 10 ) ) {
        ShowKeyboard = ! ShowKeyboard;
        Changed = true;
    }

    if ( ShowKeyboard ) {
        UpdateKeyboard( );
    }
}

void ArduinoAPI_ScreenChanged( int Top, int Left, int Bottom, int Right ) {
    Changed = true;
}

extern volatile bool KeyChanged;

void ArduinoAPI_DrawScreen( const uint8_t* Screen ) {
    int x = 0;
    int y = 0;

    if ( Changed || ( ShowKeyboard && ( KeyChanged || ( JoyButtonState != LastJoyButtonState ) ) ) ) {
        if ( ShowKeyboard ) {
            KeyChanged = false;
        }

        EmScreenPtr = Screen;
        Changed = false;

        if ( xSemaphoreTake( RenderTaskLock, 0 ) == pdTRUE ) {
            xSemaphoreGive( RenderTaskLock );

            xEventGroupSetBits( RenderTaskEventHandle, DrawScreenEvent );
        }
    }
}

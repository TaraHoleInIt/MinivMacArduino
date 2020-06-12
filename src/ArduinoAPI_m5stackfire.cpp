#include <string.h>

#include <Arduino.h>
#include <Wire.h>

#include <FS.h>
#include <SPIFFS.h>
#include <SD_MMC.h>

#include "M5Stack.h"

#include "ArduinoAPI.h"
#include "ArduinoDraw.h"

#define DisplayWidth 320
#define DisplayHeight 240

const int Pin_CS = 1;
const int Pin_DC = 2;
const int Pin_RST = 0;
const int Pin_MISO = 12;
const int Pin_MOSI = 13;
const int Pin_CLK = 14;

bool SerMouseDown = false;
int SerMouseDX = 0;
int SerMouseDY = 0;

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

bool JoyButtonState = false;
int JoyX = 0;
int JoyY = 0;

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

    *OutButton = Button;
    *OutJoyX = X;
    *OutJoyY = Y;
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
                }
            xSemaphoreGive( RenderTaskLock );

            xSemaphoreGive( SPIBusLock );
        }
    }
}

void setup( void ) {
    //Wire.begin( );
    M5.begin( true, true, true, true );

    M5JoyRead( &JoyCenterX, &JoyCenterY, &JoyButtonState );

    M5.Lcd.clear( 0xAB50 );

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
    const float fAccel = 6.0f;
    float dx = 0.0f;
    float dy = 0.04;

    dx-= JoyX - JoyCenterX;
    dy-= JoyY - JoyCenterY;

    dx/= 384.0f;
    dy/= 384.0f;

    dx*= fAccel;
    dy*= fAccel;

    *OutXDeltaPtr = -( int ) dx;
    *OutYDeltaPtr = ( int ) dy;

    //printf( "Joy: %.2f,%.2f\n", dx, dy );

    SerMouseDX = 0;
    SerMouseDY = 0;
}

void ArduinoAPI_GiveEmulatedMouseToArduino( int* EmMouseX, int* EmMouseY ) {
    MouseX = *EmMouseX;
    MouseY = *EmMouseY;
}

int ArduinoAPI_GetMouseButton( void ) {
    return SerMouseDown || JoyButtonState || M5.BtnA.isPressed( ) || M5.BtnB.isPressed( ) || M5.BtnC.isPressed( );
}

uint64_t ArduinoAPI_GetTimeMS( void ) {
    return ( uint64_t ) 1591551981844L + ( uint64_t ) millis( );
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

    printf( "File: %s = ", SPIFFSPath );

    xSemaphoreTake( SPIBusLock, portMAX_DELAY );
        Handle = ( ArduinoFile ) fopen( SPIFFSPath, Mode );
    xSemaphoreGive( SPIBusLock );

    printf( "%p\n", ( void* ) Handle );

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

void ArduinoAPI_CheckForEvents( void ) {
    M5.update( );
    M5JoyRead( &JoyX, &JoyY, &JoyButtonState );

    while ( Serial.available( ) ) {
        switch ( Serial.read( ) ) {
            case 'w': {
                SerMouseDY-= 2;
                break;
            }
            case 's': {
                SerMouseDY+= 2;
                break;
            }
            case 'a': {
                SerMouseDX-= 2;
                break;
            }
            case 'd': {
                SerMouseDX+= 2;
                break;
            }
            case ' ': {
                SerMouseDown = ! SerMouseDown;
                break;
            }
            default: break;
        }
    }
}

bool Changed = false;

void ArduinoAPI_ScreenChanged( int Top, int Left, int Bottom, int Right ) {
    Changed = true;
}

void ArduinoAPI_DrawScreen( const uint8_t* Screen ) {
    int x = 0;
    int y = 0;

    if ( Changed ) {
        EmScreenPtr = Screen;
        Changed = false;

        x = MouseX - ( DisplayWidth * 3 ) / 4;
        y = MouseY - ( DisplayHeight * 3 ) / 4;

        x = x < 0 ? 0 : x;
        y = y < 0 ? 0 : y;

        //DrawWindowSubpixel( Screen, x, y );
        //DrawWindowScaled( Screen, 0, 0 );
#if 1
        if ( xSemaphoreTake( RenderTaskLock, 0 ) == pdTRUE ) {
            xSemaphoreGive( RenderTaskLock );

            xEventGroupSetBits( RenderTaskEventHandle, DrawScreenEvent );
        }
#endif
    }
}

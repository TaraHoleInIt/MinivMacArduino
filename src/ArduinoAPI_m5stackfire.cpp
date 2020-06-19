#include <string.h>

#include <Arduino.h>
#include <Wire.h>

#include <FS.h>
#include <SPIFFS.h>
#include <SD_MMC.h>

#include "M5Stack.h"

#include "ArduinoAPI.h"
#include "ArduinoDraw.h"

#include "m5stack/mouse_imu.h"

#include "SYSDEPNS.h"
#include "CNFGGLOB.h"
#include "CNFGRAPI.h"
#include "MYOSGLUE.h"

int vMacMouseX = 0;
int vMacMouseY = 0;

bool ScreenNeedsUpdate = false;

portMUX_TYPE Crit = portMUX_INITIALIZER_UNLOCKED;

#define DrawScreenEvent 0x01

EventGroupHandle_t RenderTaskEventHandle = NULL;
SemaphoreHandle_t RenderTaskLock = NULL;
SemaphoreHandle_t SPIBusLock = NULL;
TaskHandle_t RenderTaskHandle = NULL;

volatile const uint8_t* EmScreenPtr = NULL;

IMUMouse Mouse = IMUMouse( );

void RenderTask( void* Param ) {
    while ( true ) {
        //x = vMacMouseX - ( DisplayWidth * 1 ) / 2;
        //y = vMacMouseY - ( DisplayHeight * 1 ) / 2;

        xEventGroupWaitBits( RenderTaskEventHandle, DrawScreenEvent, pdTRUE, pdTRUE, portMAX_DELAY );

        if ( xSemaphoreTake( SPIBusLock, pdMS_TO_TICKS( 5 ) ) == pdTRUE ) {
            xSemaphoreTake( RenderTaskLock, portMAX_DELAY );
                if ( EmScreenPtr ) {
                    DrawWindowScaled( ( const uint8_t* ) EmScreenPtr, 0, 0 );
                }
            xSemaphoreGive( RenderTaskLock );
            xSemaphoreGive( SPIBusLock );
        }
    }
}

void setup( void ) {
    M5.begin( true, true, true, true );

    M5.Speaker.write( 0 );
    M5.Speaker.mute( );
    M5.Speaker.setVolume( 0 );

    M5.Lcd.clear( TFT_BLACK );

    Setup1BPPTable( );
    SetupScalingTable( );

    RenderTaskEventHandle = xEventGroupCreate( );
    RenderTaskLock = xSemaphoreCreateMutex( );
    SPIBusLock = xSemaphoreCreateMutex( );

    xTaskCreatePinnedToCore( RenderTask, "RenderTask", 4096, NULL, 0, &RenderTaskHandle, 0 );

    Mouse.Init( );
}

void loop( void ) {
    minivmac_main( 0, NULL );

    Serial.println( "If we got here, something bad happened." );

    while ( true ) {
        delay( 100 );
    }
}

void ArduinoAPI_GetDisplayDimensions( int* OutWidthPtr, int* OutHeightPtr ) {
    *OutWidthPtr = M5.Lcd.width( );
    *OutHeightPtr = M5.Lcd.height( );
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
    float x = 0.0f;
    float y = 0.0f;

    Mouse.Read( x, y );

    *OutXDeltaPtr = ( int ) x;
    *OutYDeltaPtr = ( int ) y;
}

void ArduinoAPI_GiveEmulatedMouseToArduino( int* EmMouseX, int* EmMouseY ) {
    vMacMouseX = *EmMouseX;
    vMacMouseY = *EmMouseY;
}

int ArduinoAPI_GetMouseButton( void ) {
    return ( int ) Mouse.ReadButton( );
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

void ArduinoAPI_CheckForEvents( void ) {
    M5.update( );
    Mouse.Update( );
}

void ArduinoAPI_ScreenChanged( int Top, int Left, int Bottom, int Right ) {
    ScreenNeedsUpdate = true;
}

void ArduinoAPI_DrawScreen( const uint8_t* Screen ) {
    if ( ScreenNeedsUpdate ) {
        ScreenNeedsUpdate = false;
        EmScreenPtr = Screen;

        if ( xSemaphoreTake( RenderTaskLock, 0 ) == pdTRUE ) {
            xSemaphoreGive( RenderTaskLock );

            xEventGroupSetBits( RenderTaskEventHandle, DrawScreenEvent );
        }
    }
}

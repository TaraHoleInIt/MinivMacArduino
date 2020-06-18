#include <Arduino.h>
#include <ArduinoAPI.h>
#include <ArduinoDraw.h>
#include <ArduinoKeyboard.h>

extern volatile bool JoyButtonState;
extern volatile float JoyDx;
extern volatile float JoyDy;

extern "C" const uint8_t KB_Lowercase_Image[ ] __asm( "_binary_data_kb_lc_bin_start" );
extern "C" const uint8_t Touchmap[ 10 ][ 29 ] __asm( "_binary_data_touchmap_bin_start" );

#define KeyboardWidth 232
#define KeyboardHeight 80

volatile int SelectedKey = 0xFF;
volatile int LastKey = 0xFF;

volatile bool KeyChanged = false;

int KBMouseX = 0;
int KBMouseY = 0;

void DrawKeyboard( void ) {
    const uint8_t* Src = KB_Lowercase_Image;
    static uint16_t Buffer[ KeyboardWidth ];
    uint16_t* Dest = NULL;
    int DisplayWidth = 0;
    int DisplayHeight = 0;
    uint8_t Data = 0;
    int Key = 0;
    int x = 0;
    int y = 0;

    ArduinoAPI_GetDisplayDimensions( &DisplayWidth, &DisplayHeight );

    x = ( DisplayWidth / 2 ) - ( KeyboardWidth / 2 );
    y = ( DisplayHeight - KeyboardHeight );

    ArduinoAPI_SetAddressWindow( x, y, x + KeyboardWidth, y + KeyboardHeight );
    
    for ( y = 0; y < KeyboardHeight; y++ ) {
        Dest = Buffer;

        for ( x = 0; x < KeyboardWidth; x+= 8 ) {
            Key = ( int ) Touchmap[ y / 8 ][ x / 8 ];
            Data = *Src++;

            if ( Key == SelectedKey ) {
                if ( JoyButtonState ) {
                    Data = ~Data;
                } else {
                    Data = ( y & 1 ) ? Data & ~0x55 : Data & ~0xAA;
                }
            }

            *Dest++ = Data & 0x80 ? 0xFFFF : 0x0000;
            *Dest++ = Data & 0x40 ? 0xFFFF : 0x0000;
            *Dest++ = Data & 0x20 ? 0xFFFF : 0x0000;
            *Dest++ = Data & 0x10 ? 0xFFFF : 0x0000;
            *Dest++ = Data & 0x08 ? 0xFFFF : 0x0000;
            *Dest++ = Data & 0x04 ? 0xFFFF : 0x0000;
            *Dest++ = Data & 0x02 ? 0xFFFF : 0x0000;
            *Dest++ = Data & 0x01 ? 0xFFFF : 0x0000;
        }

        ArduinoAPI_WritePixels( Buffer, KeyboardWidth );
    }
}

void UpdateKeyboard( void ) {
    bool Button = false;
    int dx = 0;
    int dy = 0;

    Button = ArduinoAPI_GetMouseButton( );

    ShowKeyboard = false;
        ArduinoAPI_GetMouseDelta( &dx, &dy );
    ShowKeyboard = true;

    KBMouseX+= dx;
    KBMouseY+= dy;

    KBMouseX = ( KBMouseX < 0 ) ? 0 : KBMouseX;
    KBMouseX = ( KBMouseX >= KeyboardWidth ) ? KeyboardWidth - 1 : KBMouseX;

    KBMouseY = ( KBMouseY < 0 ) ? 0 : KBMouseY;
    KBMouseY = ( KBMouseY >= KeyboardHeight ) ? KeyboardHeight - 1 : KBMouseY;

    SelectedKey = Touchmap[ KBMouseY / 8 ][ KBMouseX / 8 ];

    if ( SelectedKey != LastKey ) {
        LastKey = SelectedKey;
        KeyChanged = true;
    }
}

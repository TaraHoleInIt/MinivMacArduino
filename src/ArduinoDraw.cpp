#include <Arduino.h>
#include "ArduinoAPI.h"
#include "ArduinoDraw.h"

#include "CNFGGLOB.h"

#define RGB565( r, g, b ) ( ( ( b ) << 11 ) | ( ( g ) << 5 ) | ( r ) )

#define EmScreenPitch ( vMacScreenWidth / 8 )

DRAM_ATTR uint16_t ScreenBuffer[ vMacScreenWidth ]; // HACKHACKHACK

DRAM_ATTR uint16_t ConversionTable1BPP[ 256 ][ 8 ];
DRAM_ATTR uint16_t ScaleTable[ 16 ];

DRAM_ATTR uint16_t SubpxDecodeTable_Wide[ 8 ][ 2 ] = {
    { RGB565( 0, 0, 0 ), RGB565( 0, 0, 0 ) }, // 000
    { RGB565( 0, 0, 0 ), RGB565( 0, 63, 31 ) }, // 001
    { RGB565( 0, 0, 31 ), RGB565( 31, 0, 0 ) }, // 010
    { RGB565( 0, 0, 31 ), RGB565( 31, 63, 31 ) }, // 011
    { RGB565( 31, 63, 0 ), RGB565( 0, 0, 0 ) }, // 100
    { RGB565( 31, 63, 0 ), RGB565( 0, 63, 31 ) }, // 101
    { RGB565( 31, 63, 31 ), RGB565( 31, 0, 0 ) }, // 110
    { RGB565( 31, 63, 31 ), RGB565( 31, 63, 31 ) } // 111
};

void SetupScalingTable( void ) {
	float Color = 0.0f;
	int i = 0;
	int j = 0;

	for ( i = 0; i < 4; i++ ) {
		for ( j = 0; j < 4; j++ ) {
			Color = i & 0x02 ? 1.0f : 0.0f;
			Color+= j & 0x02 ? 1.0f : 0.0f;
			Color+= i & 0x01 ? 1.0f : 0.0f;
			Color+= j & 0x01 ? 1.0f : 0.0f;

			Color/= 4.0f;
			Color*= 31.0f;

			ScaleTable[ ( i << 2 ) | j ] = RGB565( ( int ) Color, ( ( int ) Color ) << 1, ( int ) Color );
		}
	}
}

void Setup1BPPTable( void ) {
    int i = 0;

    for ( i = 0; i < 256; i++ ) {
        ConversionTable1BPP[ i ][ 0 ] = i & 0x80 ? RGB565( 0, 0, 0 ) : RGB565( 31, 63, 31 );
        ConversionTable1BPP[ i ][ 1 ] = i & 0x40 ? RGB565( 0, 0, 0 ) : RGB565( 31, 63, 31 );
        ConversionTable1BPP[ i ][ 2 ] = i & 0x20 ? RGB565( 0, 0, 0 ) : RGB565( 31, 63, 31 );
        ConversionTable1BPP[ i ][ 3 ] = i & 0x10 ? RGB565( 0, 0, 0 ) : RGB565( 31, 63, 31 );
        ConversionTable1BPP[ i ][ 4 ] = i & 0x08 ? RGB565( 0, 0, 0 ) : RGB565( 31, 63, 31 );
        ConversionTable1BPP[ i ][ 5 ] = i & 0x04 ? RGB565( 0, 0, 0 ) : RGB565( 31, 63, 31 );
        ConversionTable1BPP[ i ][ 6 ] = i & 0x02 ? RGB565( 0, 0, 0 ) : RGB565( 31, 63, 31 );
        ConversionTable1BPP[ i ][ 7 ] = i & 0x01 ? RGB565( 0, 0, 0 ) : RGB565( 31, 63, 31 );
    }
}

IRAM_ATTR void DrawWindow( const uint8_t* Src, int SrcX, int SrcY ) {
    const uint8_t* Ptr = NULL;
    uint16_t* Dst = NULL;
    int DisplayWidth = 0;
    int DisplayHeight = 0;
    int Width = 0;
    int Height = 0;
    int x = 0;
    int y = 0;

    ArduinoAPI_GetDisplayDimensions( &DisplayWidth, &DisplayHeight );

    SrcX = ( SrcX >= ( vMacScreenWidth - DisplayWidth ) ) ? ( vMacScreenWidth - DisplayWidth - 0 ) : SrcX;
    SrcY = ( SrcY >= ( vMacScreenHeight - DisplayHeight ) ) ? ( vMacScreenHeight - DisplayHeight - 0 ) : SrcY;

    Width = ( vMacScreenWidth >= DisplayWidth ) ? DisplayWidth : vMacScreenWidth;
    Height = ( vMacScreenHeight >= DisplayHeight ) ? DisplayHeight : vMacScreenHeight;

    ArduinoAPI_SetAddressWindow( 0, 0, Width, Height );

    Src+= ( SrcY * EmScreenPitch );

    for ( y = 0; y < Height; y++ ) {
        Ptr = &Src[ ( y * ( vMacScreenWidth / 8 ) ) + ( SrcX / 8 ) ];
        Dst = ScreenBuffer;

        for ( x = 0; x < Width; x+= 8 ) {
            memcpy( Dst, ConversionTable1BPP[ *Ptr++ ], 8 * 2 );
            Dst+= 8;
        }

        ArduinoAPI_WritePixels( ScreenBuffer, Width );
    }
}

IRAM_ATTR void DrawWindowScaled( const uint8_t* Src, int SrcX, int SrcY ) {
	const uint8_t* SrcLinePtrA = NULL;
	const uint8_t* SrcLinePtrB = NULL;
    int DisplayWidth = 0;
    int DisplayHeight = 0;
    int Width = 0;
    int Height = 0;
	uint16_t* Dst = NULL;
	uint8_t a = 0;
	uint8_t b = 0;
	int x = 0;
	int h = 0;

    ArduinoAPI_GetDisplayDimensions( &DisplayWidth, &DisplayHeight );

    Width = ( vMacScreenWidth / 2 ) >= DisplayWidth ? DisplayWidth : ( vMacScreenWidth / 2 );
    Height = ( vMacScreenHeight / 2 ) >= DisplayHeight ? DisplayHeight : ( vMacScreenHeight / 2 );

    ArduinoAPI_SetAddressWindow( 0, 0, Width, Height );

	for ( h = 0; h < 342; h+= 2 ) {
		SrcLinePtrA = &Src[ ( SrcY + h ) * ( vMacScreenWidth / 8 ) ];
		SrcLinePtrA+= ( SrcX / 8 );

		SrcLinePtrB = &Src[ ( SrcY + h + 1 ) * ( vMacScreenWidth / 8 ) ];
		SrcLinePtrB+= ( SrcX / 8 );

		Dst = ScreenBuffer;

		for ( x = 0; x < Width; x+= 4 ) {
			a = ~*SrcLinePtrA++;
			b = ~*SrcLinePtrB++;

			*Dst++ = ScaleTable[ ( ( ( a >> 6 ) & 0x03 ) << 2 ) | ( ( b >> 6 ) & 0x03 ) ];
			*Dst++ = ScaleTable[ ( ( ( a >> 4 ) & 0x03 ) << 2 ) | ( ( b >> 4 ) & 0x03 ) ];
			*Dst++ = ScaleTable[ ( ( ( a >> 2 ) & 0x03 ) << 2 ) | ( ( b >> 2 ) & 0x03 ) ];
			*Dst++ = ScaleTable[ ( ( ( a ) & 0x03 ) << 2 ) | ( ( b ) & 0x03 ) ];
		}

		ArduinoAPI_WritePixels( ScreenBuffer, Width );
	}
}

IRAM_ATTR void DrawWindowSubpixel( const uint8_t* Src, int SrcX, int SrcY ) {
    const uint8_t* SrcLinePtr = NULL;
    uint16_t* DestLinePtr = NULL;
    int DisplayWidth = 0;
    int DisplayHeight = 0;
    uint32_t Data = 0;
    int Temp = 0;
    int x = 0;
    int y = 0;
    int i = 0;

    ArduinoAPI_GetDisplayDimensions( &DisplayWidth, &DisplayHeight );

    Temp = ( DisplayWidth * 3 ) / 2;

    SrcX = ( SrcX >= ( vMacScreenWidth - Temp ) ) ? ( vMacScreenWidth - Temp ) : SrcX;
    SrcY = ( SrcY >= ( vMacScreenHeight - DisplayHeight ) ) ? ( vMacScreenHeight - DisplayHeight ) : SrcY;
    DisplayWidth = Temp >= DisplayWidth ? DisplayWidth : Temp;

    DisplayWidth = vMacScreenWidth > DisplayWidth ? DisplayWidth : vMacScreenWidth;
    DisplayHeight = vMacScreenWidth > DisplayHeight ? DisplayHeight : vMacScreenHeight;

    ArduinoAPI_SetAddressWindow( 0, 0, DisplayWidth, DisplayHeight );

    Src+= ( SrcY * EmScreenPitch );

    for ( y = 0; y < DisplayHeight; y++ ) {
        SrcLinePtr = &Src[ y * EmScreenPitch ];
        SrcLinePtr+= ( SrcX / 8 );

        DestLinePtr = ScreenBuffer;

        for ( x = 0; x < DisplayWidth; x+= 16 ) {
			Data = ( *SrcLinePtr++ ) << 24;
			Data |= ( *SrcLinePtr++ ) << 16;
			Data |= ( *SrcLinePtr++ ) << 8;

			Data = ~Data;

			for ( i = 0; i < 8; i++ ) {
				*DestLinePtr++ = SubpxDecodeTable_Wide[ ( Data >> 29 ) & 0x07 ][ 0 ];
				*DestLinePtr++ = SubpxDecodeTable_Wide[ ( Data >> 29 ) & 0x07 ][ 1 ];

				Data<<= 3;
			}
        }

        ArduinoAPI_WritePixels( ScreenBuffer, DisplayWidth );
    }
}

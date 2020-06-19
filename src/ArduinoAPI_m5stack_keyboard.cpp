#include <Arduino.h>

#define KeyboardWidth 232
#define KeyboardHeight 80

extern "C" void MinivMacAPI_UpdateKey( uint8_t Key, uint8_t Down );

extern "C" const uint8_t KB_Lowercase_Image[ ] __asm( "_binary_data_kb_lc_bin_start" );
extern "C" const uint8_t Touchmap[ 10 ][ 29 ] __asm( "_binary_data_touchmap_bin_start" );



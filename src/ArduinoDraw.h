#ifndef _ARDUINODRAW_H_
#define _ARDUINODRAW_H_

void SetupScalingTable( void );
void Setup1BPPTable( void );

void DrawWindow( const uint8_t* Src, int SrcX, int SrcY );
void DrawWindowScaled( const uint8_t* Src, int SrcX, int SrcY );

#endif

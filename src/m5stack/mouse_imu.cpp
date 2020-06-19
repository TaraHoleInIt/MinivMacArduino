#include <Arduino.h>
#include "M5Stack.h"

#include "arduino/mouse.h"
#include "m5stack/mouse_imu.h"

#define IMU_SAMPLE_COUNT 10

bool IMUMouse::Init( void ) {
    M5.IMU.Init( );
    return true;
}

void IMUMouse::Update( void ) {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    int i = 0;

    _dx = 0.0f;
    _dy = 0.0f;

    for ( i = 0; i < IMU_SAMPLE_COUNT; i++ ) {
        M5.IMU.getAccelData( &x, &y, &z );

        _dx+= x;
        _dy+= y;
    }

    _dx/= ( float ) IMU_SAMPLE_COUNT;
    _dy/= ( float ) IMU_SAMPLE_COUNT;

    _dx-= _CenterX;
    _dy-= _CenterY;

    _dx = fabs( _dx ) >= 0.15f ? -_dx * 6.0f : 0.0f;
    _dy = fabs( _dy ) >= 0.15f ? -_dy * 6.0f : 0.0f;

    _Button = M5.BtnA.isPressed( );

    if ( M5.BtnC.wasReleased( ) ) {
        _CenterX = x;
        _CenterY = y;
    }
}

void IMUMouse::Read( float& dx, float& dy ) {
    dx = _dx;
    dy = _dy;
}

bool IMUMouse::ReadButton( void ) {
    return _Button;
}

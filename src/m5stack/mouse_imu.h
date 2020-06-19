#ifndef _MOUSE_IMU_H_
#define _MOUSE_IMU_H_

#include "arduino/mouse.h"

class IMUMouse : public ArduinoMouse {
private:
    float _CenterX = 0.0f;
    float _CenterY = 0.0f;

public:
    IMUMouse( void ) {
        _Button = false;

        _CenterX = 0.0f;
        _CenterY = 0.0f;
        
        _dx = 0.0f;
        _dy = 0.0f;
    }

    bool Init( void );
    void Update( void );

    void Read( float& dx, float& dy );
    bool ReadButton( void );
};

#endif

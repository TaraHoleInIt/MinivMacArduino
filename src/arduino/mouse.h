#ifndef _MOUSE_H_
#define _MOUSE_H_

class ArduinoMouse {
protected:
    bool _Button;
    float _dx;
    float _dy;

public:
    virtual bool Init( void ) = 0;
    virtual void Update( void ) = 0;

    virtual void Read( float& dx, float& dy ) = 0;
    virtual bool ReadButton( void ) = 0;
};

#endif

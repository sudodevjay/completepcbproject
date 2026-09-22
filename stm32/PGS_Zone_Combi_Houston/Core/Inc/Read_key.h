/*
 * Read_key.h
 *
 *  Created on: Jan 24, 2026
 *      Author: Houston
 */

#ifndef INC_READ_KEY_H_
#define INC_READ_KEY_H_


#include "HWprofile.h"

unsigned char ReadKeyStt(void)
{
    unsigned char RKey = K_RLS;

    if(KEY_UP_RAW())      RKey = K_INC;
    else if(KEY_DN_RAW()) RKey = K_DEC;
    else if(KEY_SFT_RAW())RKey = K_ESC;
    else if(KEY_ENT_RAW())RKey = K_ENT;

    return RKey;
}
unsigned char ReadKey(unsigned char LastState)
{
    unsigned char Fkey, Skey;
    unsigned char SttKey = LastState;

    Fkey = ReadKeyStt();
    HAL_Delay(1);              // debounce delay (same spirit as old code)
    Skey = ReadKeyStt();

    if(Fkey == Skey)
    {
        SttKey = Fkey;
    }

    return SttKey;
}
void keyprocess(void)
{
    keystt = ReadKey(keystt);

    if(!KEY.Flag.Wait4KRLS)
    {
        switch(keystt)
        {
            case K_ESC:
                KEY.Flag.Wait4KRLS = 1;   // wait for release
                break;
            case K_INC:
                KEY.Flag.Wait4KRLS = 1;   // wait for release
                break;
            case K_DEC:
                KEY.Flag.Wait4KRLS = 1;   // wait for release
                break;
            case K_ENT:
                KEY.Flag.Wait4KRLS = 1;   // wait for release
                if(DisMenu==MIAN_PAGE) DisMenu=ENTER_PASSWORD;
                do { keystt = ReadKey(keystt); }
                       while(keystt != K_RLS);
                break;

            default:
                break;
        }
    }
    else
    {
        if(keystt == K_RLS)
        {
            KEY.Flag.Wait4KRLS = 0;       // released
        }
    }
}
#endif /* INC_READ_KEY_H_ */

/*
 * HWprofile.h
 *
 *  Created on: Jan 24, 2026
 *      Author: Houston
 */

#ifndef INC_HWPROFILE_H_
#define INC_HWPROFILE_H_


//#define MAX_SENSORS 99
//extern SensorData sensors[MAX_SENSORS];

#define KEY_UP_RAW()   (HAL_GPIO_ReadPin(UP_KEY_GPIO_Port,  UP_KEY_Pin)  == GPIO_PIN_RESET)
#define KEY_DN_RAW()   (HAL_GPIO_ReadPin(DN_KEY_GPIO_Port,  DN_KEY_Pin)  == GPIO_PIN_RESET)
#define KEY_SFT_RAW()  (HAL_GPIO_ReadPin(SFT_KEY_GPIO_Port, SFT_KEY_Pin) == GPIO_PIN_RESET)
#define KEY_ENT_RAW()  (HAL_GPIO_ReadPin(ENT_KEY_GPIO_Port, ENT_KEY_Pin) == GPIO_PIN_RESET)

#define K_RLS   0x00
#define K_INC   0x02    // UP
#define K_DEC   0x04    // DOWN
#define K_ESC   0x06    // SHIFT
#define K_ENT   0x08    // ENTER
union{
    struct{
    	unsigned Settings:1;
       unsigned Wait4KRLS:1;
//       unsigned Flg_RESET4:1;

    }Flag;
    unsigned char Flags;
}KEY;
union{
    struct{
    	unsigned DATA_REQUEST:1;
    	unsigned Zone_request:1;
    	unsigned Zone_responce:1;
    	unsigned Master_request:1;
    	unsigned Display_request:1;


    }Flag;
    unsigned char Flags;
}FL;
char keystt=0;

typedef struct
{
    uint8_t id;             // Display ID
    uint8_t mode;           // Display mode
    uint8_t color;          // Display color
//    uint8_t sensor_start;   // Sensor start index
//    uint8_t sensor_stop;    // Sensor stop index
} DISPLAY_t;

char Master_ID=0,Number_of_Display=0,Total_Floor=0;

#define MAX_DISPLAY 9
DISPLAY_t Displays[MAX_DISPLAY];

char DisMenu=10;

#define PASSWORD  				6669
#define ENTER_PASSWORD 			9
#define MIAN_PAGE   			10
#define SET_Master_ID     		11
#define SET_NUMBER_OF_DISPLAY   12
#define SET_DISPLAY_SETTING     13
#define SET_DISPLAY_CONFIG		14
#define SET_Total_Floor			15


const char* mode_to_str(uint8_t mode)
{
    switch (mode)
    {
        case 1: return "#u#";
        case 2: return "#d#";
        case 3: return "#r#";
        case 4: return "#l#";
        default: return "#u#";
    }
}
char color_to_char(uint8_t color)
{
    switch (color)
    {
        case 1: return 'R';
        case 2: return 'G';
        case 3: return 'B';
        default: return 'R';
    }
}

#endif /* INC_HWPROFILE_H_ */

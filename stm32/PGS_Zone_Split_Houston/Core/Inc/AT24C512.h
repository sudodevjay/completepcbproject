/*
 * AT24C512.h
 *
 *  Created on: Jan 24, 2026
 *      Author: Houston
 */

#ifndef INC_AT24C512_H_
#define INC_AT24C512_H_

extern I2C_HandleTypeDef hi2c3;

uint8_t AT24C512_ReadByte(uint16_t MemAddr)
{
    uint8_t data = 0;
    uint8_t DevAddr = 0x50 << 1;  // 7-bit address shifted for HAL
    HAL_I2C_Mem_Read(&hi2c3,DevAddr,MemAddr,I2C_MEMADD_SIZE_16BIT,&data,1,HAL_MAX_DELAY);
    return data;
}
void AT24C512_WriteByte(uint16_t MemAddr, uint8_t data)
{
    uint8_t DevAddr = 0x50 << 1;
    HAL_I2C_Mem_Write(&hi2c3,DevAddr,MemAddr,I2C_MEMADD_SIZE_16BIT,&data,1,HAL_MAX_DELAY);
    // EEPROM write cycle time (VERY IMPORTANT)
    HAL_Delay(5);  // AT24C512 needs ~5ms
}

#endif /* INC_AT24C512_H_ */

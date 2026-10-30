#ifndef nRF24L01_NRF
#define nRF24L01_NRF 100
#include "stm32f1xx_hal.h"




#define nRF24L01_TX_ADR_WIDTH    5      
#define nRF24L01_RX_ADR_WIDTH    5      
#define nRF24L01_TX_PLOAD_WIDTH  8    
#define nRF24L01_RX_PLOAD_WIDTH  8    


#define nRF24L01_READ_RE        0x00     
#define nRF24L01_WRITE_RE        0x20    
#define nRF24L01_RD_RX_PLOAD     0x61     
#define nRF24L01_WR_TX_PLOAD     0xA0     
#define nRF24L01_FLUSH_TX        0xE1    
#define nRF24L01_FLUSH_RX        0xE2     
#define nRF24L01_REUSE_TX_PL     0xE3    
#define nRF24L01_NOP             0xFF     

#define nRF24L01_CONFIG          0x00  
#define nRF24L01_EN_AA           0x01  
#define nRF24L01_EN_RXADDR       0x02  
#define nRF24L01_SETUP_AW        0x03 
#define nRF24L01_SETUP_RETR      0x04  
#define nRF24L01_RF_CH           0x05  
#define nRF24L01_RF_SETUP        0x06  
#define nRF24L01_STATUS          0x07  
#define nRF24L01_OBSERVE_TX			0x08
#define nRF24L01_CD              0x09  
#define nRF24L01_RX_ADDR_P0      0x0A  
#define nRF24L01_RX_ADDR_P1      0x0B  
#define nRF24L01_RX_ADDR_P2      0x0C  
#define nRF24L01_RX_ADDR_P3      0x0D  
#define nRF24L01_RX_ADDR_P4      0x0E  
#define nRF24L01_RX_ADDR_P5      0x0F  
#define nRF24L01_TX_ADDR         0x10  
#define nRF24L01_RX_PW_P0        0x11  
#define nRF24L01_RX_PW_P1        0x12  
#define nRF24L01_RX_PW_P2        0x13  
#define nRF24L01_RX_PW_P3        0x14  
#define nRF24L01_RX_PW_P4        0x15  
#define nRF24L01_RX_PW_P5        0x16  
#define nRF24L01_FIFO_STATUS     0x17  

//----- pins used for software (bit-banged) SPI to nRF24l01 -----
// SCK/MISO/MOSI هنگام راه‌اندازی به صورت نرم‌افزاری زده می‌شوند (SPI سخت‌افزاری استفاده نمی‌شود)
#define NRF_CE_Pin GPIO_PIN_2
#define NRF_CE_GPIO_Port GPIOA
#define NRF_CSN_Pin GPIO_PIN_3
#define NRF_CSN_GPIO_Port GPIOA
#define NRF_IRQ_Pin GPIO_PIN_4
#define NRF_IRQ_GPIO_Port GPIOA
#define NRF_IRQ_EXTI_IRQn EXTI4_IRQn
#define NRF_SCK_Pin GPIO_PIN_5
#define NRF_SCK_GPIO_Port GPIOA
#define NRF_MISO_Pin GPIO_PIN_6
#define NRF_MISO_GPIO_Port GPIOA
#define NRF_MOSI_Pin GPIO_PIN_7
#define NRF_MOSI_GPIO_Port GPIOA

/* Pins configuration */
#define NRF24L01_CE_LOW				HAL_GPIO_WritePin(NRF_CE_GPIO_Port,NRF_CE_Pin,GPIO_PIN_RESET);
#define NRF24L01_CE_HIGH			HAL_GPIO_WritePin(NRF_CE_GPIO_Port,NRF_CE_Pin,GPIO_PIN_SET);
#define NRF24L01_CSN_LOW			HAL_GPIO_WritePin(NRF_CSN_GPIO_Port,NRF_CSN_Pin,GPIO_PIN_RESET);
#define NRF24L01_CSN_HIGH			HAL_GPIO_WritePin(NRF_CSN_GPIO_Port,NRF_CSN_Pin,GPIO_PIN_SET);
#define NRF24L01_SCK_LOW			HAL_GPIO_WritePin(NRF_SCK_GPIO_Port,NRF_SCK_Pin,GPIO_PIN_RESET);
#define NRF24L01_SCK_HIGH			HAL_GPIO_WritePin(NRF_SCK_GPIO_Port,NRF_SCK_Pin,GPIO_PIN_SET);
/**
 * @brief  Data rate enumeration
 */
 /* Configuration register*/
#define NRF24L01_MASK_RX_DR		6
#define NRF24L01_MASK_TX_DS		5
#define NRF24L01_MASK_MAX_RT	4
#define NRF24L01_EN_CRC			3
#define NRF24L01_CRCO			2
#define NRF24L01_PWR_UP			1
#define NRF24L01_PRIM_RX		0

/* Enable auto acknowledgment*/
#define NRF24L01_ENAA_P5		5
#define NRF24L01_ENAA_P4		4
#define NRF24L01_ENAA_P3		3
#define NRF24L01_ENAA_P2		2
#define NRF24L01_ENAA_P1		1
#define NRF24L01_ENAA_P0		0

/* Enable rx addresses */
#define NRF24L01_ERX_P5			5
#define NRF24L01_ERX_P4			4
#define NRF24L01_ERX_P3			3
#define NRF24L01_ERX_P2			2
#define NRF24L01_ERX_P1			1
#define NRF24L01_ERX_P0			0

/* Setup of address width */
#define NRF24L01_AW				0 //2 bits

/* Setup of auto re-transmission*/
#define NRF24L01_ARD			4 //4 bits
#define NRF24L01_ARC			0 //4 bits

/* RF setup register*/
#define NRF24L01_PLL_LOCK		4
#define NRF24L01_RF_DR_LOW		5
#define NRF24L01_RF_DR_HIGH		3
#define NRF24L01_RF_DR			3
#define NRF24L01_RF_PWR			1 //2 bits   

/* General status register */
#define NRF24L01_RX_DR			6
#define NRF24L01_TX_DS			5
#define NRF24L01_MAX_RT			4
#define NRF24L01_RX_P_NO		1 //3 bits
#define NRF24L01_TX_FULL		0

/* Transmit observe register */
#define NRF24L01_PLOS_CNT		4 //4 bits
#define NRF24L01_ARC_CNT		0 //4 bits

/* FIFO status*/
#define NRF24L01_TX_REUSE		6
#define NRF24L01_FIFO_FULL		5
#define NRF24L01_TX_EMPTY		4
#define NRF24L01_RX_FULL		1
#define NRF24L01_RX_EMPTY		0

//Dynamic length
#define NRF24L01_DPL_P0			0
#define NRF24L01_DPL_P1			1
#define NRF24L01_DPL_P2			2
#define NRF24L01_DPL_P3			3
#define NRF24L01_DPL_P4			4
#define NRF24L01_DPL_P5			5

/* Transmitter power*/
#define NRF24L01_M18DBM			0 //-18 dBm
#define NRF24L01_M12DBM			1 //-12 dBm
#define NRF24L01_M6DBM			2 //-6 dBm
#define NRF24L01_0DBM			3 //0 dBm

/* Data rates */
#define NRF24L01_2MBPS			0
#define NRF24L01_1MBPS			1
#define NRF24L01_250KBPS		2

/* Configuration */
#define NRF24L01_CONFIG			((1 << NRF24L01_EN_CRC) | (0 << NRF24L01_CRCO))

/* Instruction Mnemonics */
#define NRF24L01_REGISTER_MASK				0x1F

#define NRF24L01_READ_REGISTER_MASK(reg)	(0x00 | (NRF24L01_REGISTER_MASK & reg)) //Last 5 bits will indicate reg. address
#define NRF24L01_WRITE_REGISTER_MASK(reg)	(0x20 | (NRF24L01_REGISTER_MASK & reg)) //Last 5 bits will indicate reg. address
#define NRF24L01_R_RX_PAYLOAD_MASK			0x61
#define NRF24L01_W_TX_PAYLOAD_MASK			0xA0
#define NRF24L01_FLUSH_TX_MASK				0xE1
#define NRF24L01_FLUSH_RX_MASK				0xE2
#define NRF24L01_REUSE_TX_PL_MASK			0xE3
#define NRF24L01_ACTIVATE_MASK				0x50 
#define NRF24L01_R_RX_PL_WID_MASK			0x60
#define NRF24L01_NOP_MASK					0xFF
typedef enum _nRF24L01_DataRate_t {
	nRF24L01_DataRate_2M = 0x00, /*!< Data rate set to 2Mbps */
	nRF24L01_DataRate_1M,        /*!< Data rate set to 1Mbps */
	nRF24L01_DataRate_250k       /*!< Data rate set to 250kbps */
} nRF24L01_DataRate_t;

/**
 * @brief  Output power enumeration
 */
typedef enum _nRF24L01_OutputPower_t {
	nRF24L01_OutputPower_M18dBm = 0x00,/*!< Output power set to -18dBm */
	nRF24L01_OutputPower_M12dBm,       /*!< Output power set to -12dBm */
	nRF24L01_OutputPower_M6dBm,        /*!< Output power set to -6dBm */
	nRF24L01_OutputPower_0dBm          /*!< Output power set to 0dBm */
} nRF24L01_OutputPower_t;

/**
 * @brief  Transmission status enumeration
 */
typedef enum _nRF24L01_Transmit_Status_t {
	nRF24L01_Transmit_Status_Lost = 0x00,   /*!< Message is lost, reached maximum number of retransmissions */
	nRF24L01_Transmit_Status_Ok = 0x01,     /*!< Message sent successfully */
	nRF24L01_Transmit_Status_Sending = 0xFF /*!< Message is still sending */
} nRF24L01_Transmit_Status_t;



void nRF_Init_Pin(void);
void nRF_Init(void);
unsigned char nRF_spi_RW(unsigned char Buff);
unsigned char nRF_spi_Read(unsigned char reg);
unsigned char nRF_spi_RWreg(unsigned char reg, unsigned char value);
unsigned char nRF_spi_ReadBuff(unsigned char reg, unsigned char *buff, unsigned char uchars);
unsigned char nRF_spi_WriteBuff(unsigned char reg, unsigned char *buff, unsigned char uchars);
void nRF_SetRX(void);
void nRF_SetTX(void);
void delay_us(uint32_t value); 
unsigned char nRF_GetPacket(unsigned char* rx_buf);
void nRF_SendPacket(unsigned char * tx_buf);
void nRF_SetTxAddress(uint8_t *adr);
void nRF_SetMyAddress(uint8_t *adr);
void nRF_WriteBit(uint8_t reg, uint8_t bit, uint8_t value);
void nRF_PowerUpTx(void);
void nRF_PowerUpRx(void);
void nRF_PowerDown(void);
uint8_t nRF_DataReady(void);
uint8_t nRF_RxFifoEmpty(void);
uint8_t nRF_GetStatus(void);
uint8_t nRF_ReadBit(uint8_t reg, uint8_t bit);
nRF24L01_Transmit_Status_t nRF_GetTransmissionStatus(void);
uint8_t nRF_GetRetransmissionsCount(void);
void nRF_SetChannel(uint8_t channel);
void nRF_SetRF(nRF24L01_DataRate_t DataRate, nRF24L01_OutputPower_t OutPwr);
void nRF_Clear_Interrupts(void);
void nRF_Recover(void);
void nRF_FlushRx(void);
#endif

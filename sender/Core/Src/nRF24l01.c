#include "nRF24l01.h"
unsigned char  nRF24L01_TX_ADDRESS[nRF24L01_TX_ADR_WIDTH]= {0x68,0x31,0x08,0x10,0x01};
unsigned char  nRF24L01_RX_ADDRESS[nRF24L01_RX_ADR_WIDTH]= {0x68,0x31,0x08,0x10,0x01};



/* Flush FIFOs */
#define NRF24L01_FLUSH_TX					do { NRF24L01_CSN_LOW; nRF_spi_RW( NRF24L01_FLUSH_TX_MASK); NRF24L01_CSN_HIGH; } while (0)
#define NRF24L01_FLUSH_RX					do { NRF24L01_CSN_LOW; nRF_spi_RW( NRF24L01_FLUSH_RX_MASK); NRF24L01_CSN_HIGH; } while (0)

#define NRF24L01_TRANSMISSON_OK 			0
#define NRF24L01_MESSAGE_LOST   			1

#define NRF24L01_CHECK_BIT(reg, bit)       (reg & (1 << bit))

typedef struct {
	uint8_t PayloadSize;				//Payload size
	uint8_t Channel;					//Channel selected
	nRF24L01_OutputPower_t OutPwr;	//Output power
	nRF24L01_DataRate_t DataRate;	//Data rate
} nRF24L01_t;


typedef union _nRF24L01_IRQ_t {
	struct {
		uint8_t reserved0:4;
		uint8_t MaxRT:1;     /*!< Set to 1 if maximum number of retransmissions flag is set */
		uint8_t DataSent:1;  /*!< Set to 1 if last transmission is OK */
		uint8_t DataReady:1; /*!< Set to 1 if data are ready to be read */
		uint8_t reserved1:1;
	} F;
	uint8_t Status;          /*!< NRF status register value */
} nRF24L01_IRQ_t;




void nRF_Init_Pin(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  __HAL_RCC_GPIOA_CLK_ENABLE();
 /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, NRF_CE_Pin|NRF_CSN_Pin|NRF_SCK_Pin|NRF_MOSI_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : NRF_CE_Pin NRF_CSN_Pin NRF_SCK_Pin NRF_MOSI_Pin */
  GPIO_InitStruct.Pin = NRF_CE_Pin|NRF_CSN_Pin|NRF_SCK_Pin|NRF_MOSI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : NRF_IRQ_Pin */
  GPIO_InitStruct.Pin = NRF_IRQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(NRF_IRQ_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : NRF_MISO_Pin */
  GPIO_InitStruct.Pin = NRF_MISO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(NRF_MISO_GPIO_Port, &GPIO_InitStruct);

}


unsigned char nRF_spi_RW(unsigned char Buff)
{
	uint8_t c=0;
	for(c=0;c<8;c++)
	{
		if( (Buff&0x80) == 0x80)
			HAL_GPIO_WritePin(NRF_MOSI_GPIO_Port,NRF_MOSI_Pin,GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(NRF_MOSI_GPIO_Port,NRF_MOSI_Pin,GPIO_PIN_RESET);
		delay_us(5);
		Buff =  Buff<<1;
		NRF24L01_SCK_HIGH;
		delay_us(5);
		Buff |= HAL_GPIO_ReadPin(NRF_MISO_GPIO_Port,NRF_MISO_Pin);
		NRF24L01_SCK_LOW;
	}
	return (Buff);
}

unsigned char nRF_spi_Read(unsigned char reg)
{
	uint8_t reg_value=0;
	NRF24L01_CSN_LOW;
	nRF_spi_RW(reg);
	reg_value = nRF_spi_RW(0x00);
	NRF24L01_CSN_HIGH;
	return reg_value;
}

unsigned char nRF_spi_RWreg(unsigned char reg, unsigned char value)
{
	uint8_t status;
	NRF24L01_CSN_LOW;
	status = nRF_spi_RW(reg);
	nRF_spi_RW(value);
	NRF24L01_CSN_HIGH;
	return status;
}

unsigned char nRF_spi_ReadBuff(unsigned char reg, unsigned char *buff, unsigned char uchars)
{
	uint8_t status, c;
	NRF24L01_CSN_LOW;
	status = nRF_spi_RW(reg);
	for(c=0;c<uchars;c++)
	{
		buff[c] = nRF_spi_RW(0x00);
	}
	NRF24L01_CSN_HIGH;
	return status;
}

unsigned char nRF_spi_WriteBuff(unsigned char reg, unsigned char *buff, unsigned char uchars)
{
	uint8_t status,c;
	NRF24L01_CSN_LOW;
	status = nRF_spi_RW(reg);
	for(c=0;c<uchars;c++)
	{
		nRF_spi_RW(*buff++);
	}
	NRF24L01_CSN_HIGH;
	return status;
}

void nRF_Init(void)
{
  HAL_Delay(100);
  NRF24L01_CE_LOW;
  NRF24L01_CSN_HIGH;
  NRF24L01_SCK_LOW;

  /* Auto-Ack روی این کانال */
  nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_EN_AA, 0x01);
  /* فقط کانال 0 فعال */
  nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_EN_RXADDR, 0x01);
  /* عرض آدرس: AW=01 → 3 بایت (آدرس 5 بایتی نوشته می‌شود ولی چیپ فقط
     3 بایت پایینی را به کار می‌برد — در هر دو طرف یکسان است) */
  nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_SETUP_AW, 0x01);
  /* Auto-Retry: ARD=0100 → تأخیر 1250µs، ARC=15 → حداکثر 15 تلاش */
  nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_SETUP_RETR, 0x4F);
  /* کانال 86 → 2.400GHz + 86MHz = 2486MHz
     (بیرون از باند وای‌فای/بلوتوثِ تا 2483MHz و در عین حال 39MHz پایین‌تر از
      کانال قبلیِ 125، یعنی نزدیک‌تر به باند طراحی آنتن ماژول‌ها) */
  nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_RF_CH, 86);
  /* اندازه پلود ثابت 8 بایت */
  nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_RX_PW_P0, nRF24L01_RX_PLOAD_WIDTH);
  /* توان 0dBm (RF_PWR=11) و سرعت 250kbps (RF_DR_LOW=1، RF_DR_HIGH=0) */
  nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_RF_SETUP, 0x26);
  /* حالت TX: PWR_UP=1, PRIM_RX=0، CRC دو بایتی (EN_CRC=1، CRCO=1) */
  nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_CONFIG, 0x0e);
  /* پاک‌سازی شمارنده observe */
  nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_OBSERVE_TX, 0x00);
  /* آدرس‌ها */
  nRF_spi_WriteBuff(nRF24L01_WRITE_RE + nRF24L01_TX_ADDR, nRF24L01_TX_ADDRESS, nRF24L01_TX_ADR_WIDTH);
  nRF_spi_WriteBuff(nRF24L01_WRITE_RE + nRF24L01_RX_ADDR_P0, nRF24L01_RX_ADDRESS, nRF24L01_RX_ADR_WIDTH);

  /* پاک‌سازی وقفه‌ها */
  nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_STATUS, 0x70);

  delay_us(1000);
}

void nRF_SetRX(void)
{
	NRF24L01_CE_LOW;
	nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_CONFIG, 0x0f);
	NRF24L01_CE_HIGH;
	delay_us(250);
}

void nRF_SetTX(void)
{
	NRF24L01_CE_LOW;
	nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_CONFIG, 0x0e);
	NRF24L01_CE_HIGH;
	delay_us(250);
}

unsigned char nRF_GetPacket(unsigned char* rx_buf)
{
	uint8_t sta = 0;

	if (nRF_RxFifoEmpty())
	{
		return 0;
	}

	NRF24L01_CE_LOW;
	nRF_spi_ReadBuff(nRF24L01_RD_RX_PLOAD, rx_buf, nRF24L01_RX_PLOAD_WIDTH);
	sta = nRF_spi_Read(nRF24L01_STATUS);
	nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_STATUS, sta);

	NRF24L01_CE_HIGH;

	return 1;
}

void nRF_SendPacket(unsigned char * tx_buf)
{
	/* اگر MAX_RT فعال بود، اول پاکش کن */
	if (nRF_ReadBit(nRF24L01_STATUS, NRF24L01_MAX_RT))
	{
		nRF_WriteBit(nRF24L01_STATUS, NRF24L01_MAX_RT, 1);
	}
	/* اگر FIFO پر بود، پاکش کن */
	if (nRF_ReadBit(nRF24L01_FIFO_STATUS, NRF24L01_TX_FULL))
	{
		NRF24L01_CSN_LOW; nRF_spi_RW( NRF24L01_FLUSH_TX_MASK); NRF24L01_CSN_HIGH;
	}

	NRF24L01_CE_LOW;
	/* آدرس RX را برای Auto-Ack ست کن */
	nRF_spi_WriteBuff(nRF24L01_WRITE_RE + nRF24L01_RX_ADDR_P0, nRF24L01_TX_ADDRESS, nRF24L01_TX_ADR_WIDTH);
	/* پلود را بنویس */
	nRF_spi_WriteBuff(nRF24L01_WR_TX_PLOAD, tx_buf, nRF24L01_TX_PLOAD_WIDTH);
	/* حالت TX */
	nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_CONFIG, 0x0e);
	/* CE بالا: شروع ارسال */
	NRF24L01_CE_HIGH;
	/* حداقل 10μs بالا بماند */
	delay_us(15);
	NRF24L01_CE_LOW;

}

void nRF_WriteBit(uint8_t reg, uint8_t bit, uint8_t value) {
	uint8_t tmp;
	/* Read register */
	tmp = nRF_spi_Read(reg);
	/* Make operation */
	if (value) {
		tmp |= 1 << bit;
	} else {
		tmp &= ~(1 << bit);
	}
	/* Write back */
	nRF_spi_RWreg(nRF24L01_WRITE_RE + reg, tmp);
}


uint8_t nRF_ReadBit(uint8_t reg, uint8_t bit) {
	uint8_t tmp;
	tmp = nRF_spi_Read(reg);
	if (!NRF24L01_CHECK_BIT(tmp, bit)) {
		return 0;
	}
	return 1;
}

void nRF_PowerUpTx(void) {
	nRF_Clear_Interrupts();
	nRF_spi_RWreg(nRF24L01_WRITE_RE +nRF24L01_CONFIG, NRF24L01_CONFIG | (0 << NRF24L01_PRIM_RX) | (1 << NRF24L01_PWR_UP));
}

void nRF_PowerUpRx(void) {
	NRF24L01_CE_LOW;
	NRF24L01_FLUSH_RX;
	nRF_Clear_Interrupts();
	nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_CONFIG, NRF24L01_CONFIG | 1 << NRF24L01_PWR_UP | 1 << NRF24L01_PRIM_RX);
	NRF24L01_CE_HIGH;
}

void nRF_PowerDown(void) {
	NRF24L01_CE_LOW;
	nRF_WriteBit(nRF24L01_CONFIG, NRF24L01_PWR_UP, 0);
}

uint8_t nRF_DataReady(void) {
	uint8_t status = nRF_spi_Read(nRF24L01_STATUS);

	if (NRF24L01_CHECK_BIT(status, NRF24L01_RX_DR)) {
		return 1;
	}
	return 0;
}

uint8_t nRF_RxFifoEmpty(void) {
	uint8_t reg = nRF_spi_Read(nRF24L01_FIFO_STATUS);
	return NRF24L01_CHECK_BIT(reg, NRF24L01_RX_EMPTY);
}

uint8_t nRF_GetStatus(void) {
	uint8_t status;

	NRF24L01_CSN_LOW;
	status = nRF_spi_RW( NRF24L01_NOP_MASK);
	NRF24L01_CSN_HIGH;

	return status;
}


nRF24L01_Transmit_Status_t nRF_GetTransmissionStatus(void) {
	uint8_t status = nRF_GetStatus();
	if (NRF24L01_CHECK_BIT(status, NRF24L01_TX_DS)) {
		return nRF24L01_Transmit_Status_Ok;
	} else if (NRF24L01_CHECK_BIT(status, NRF24L01_MAX_RT)) {
		return nRF24L01_Transmit_Status_Lost;
	}

	return nRF24L01_Transmit_Status_Sending;
}

uint8_t nRF_GetRetransmissionsCount(void) {
	return nRF_spi_Read(nRF24L01_OBSERVE_TX) & 0x0F;
}

void nRF_SetChannel(uint8_t channel) {
	if (channel <= 125 && channel ) {
		nRF_spi_RWreg(nRF24L01_WRITE_RE +nRF24L01_RF_CH, channel);
	}
}

void nRF_SetRF(nRF24L01_DataRate_t DataRate, nRF24L01_OutputPower_t OutPwr) {
	uint8_t tmp = 0;

	if (DataRate == nRF24L01_DataRate_2M) {
		tmp |= 1 << NRF24L01_RF_DR_HIGH;
	} else if (DataRate == nRF24L01_DataRate_250k) {
		tmp |= 1 << NRF24L01_RF_DR_LOW;
	}

	if (OutPwr == nRF24L01_OutputPower_0dBm) {
		tmp |= 3 << NRF24L01_RF_PWR;
	} else if (OutPwr == nRF24L01_OutputPower_M6dBm) {
		tmp |= 2 << NRF24L01_RF_PWR;
	} else if (OutPwr == nRF24L01_OutputPower_M12dBm) {
		tmp |= 1 << NRF24L01_RF_PWR;
	}

	nRF_spi_RWreg(nRF24L01_WRITE_RE +nRF24L01_RF_SETUP, tmp);
}

uint8_t nRF24L01_Read_Interrupts(nRF24L01_IRQ_t* IRQ) {
	IRQ->Status = nRF_GetStatus();
	return IRQ->Status;
}

/**
 * @brief  پاک‌سازی پرچم‌های وقفه (درست: به STATUS می‌نویسد، نه CONFIG!)
 */
void nRF_Clear_Interrupts(void) {
	nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_STATUS, 0x70);
}

/**
 * @brief  بازیابی ماژول پس از گیر کردن: Power Down → Flush → Power Up
 */
void nRF_Recover(void)
{
	NRF24L01_CE_LOW;
	/* Power Down */
	nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_CONFIG, 0x08);
	delay_us(2000);   /* دیتاشیت: power-down → power-up حدود 1.5ms طول می‌کشد */
	/* Flush هر دو FIFO */
	NRF24L01_FLUSH_TX;
	NRF24L01_FLUSH_RX;
	/* پاک‌سازی وقفه‌ها */
	nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_STATUS, 0x70);
	/* Power Up در حالت TX (فرستنده) */
	nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_CONFIG, 0x0e);
	delay_us(2000);   /* دیتاشیت: از power-down تا TX/RX حدود 1.5ms */
	/* CE عمداً پایین می‌ماند (Standby-I برای ارسال)؛ nRF_SendPacket هر بار
	   خودش CE را پالس می‌زند، پس اینجا مشکلی پیش نمی‌آید */
}

/**
 * @brief  Flush کردن FIFO دریافت
 */
void nRF_FlushRx(void)
{
	NRF24L01_FLUSH_RX;
}

/**
 * @brief  Flush کردن FIFO ارسال
 */
void nRF_FlushTx(void)
{
	NRF24L01_FLUSH_TX;
}

void delay_us(uint32_t value)
{
  volatile uint32_t count = value * 3;
  while (count--)
  {
    __NOP();
  }
}

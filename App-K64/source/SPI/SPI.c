/***************************************************************************//**
  @file		SPI.c
  @brief	+Descripcion del archivo+
  @author	KevinWahle
  @date		10 sep. 2022
 ******************************************************************************/

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include "gpio.h"
#include "MK64F12.h"
#include "hardware.h"
#include "SPI.h"
#include <stdint.h>
// +Incluir el header propio (ej: #include "template.h")+

/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define PCS0_PORT(spi_n) (((spi_n)==SPI_0)? PC: ((spi_n)==SPI_1)? PE: PB)
#define PCS1_PORT(spi_n) (((spi_n)==SPI_0)? PC: PE)
#define PCS2_PORT(spi_n) (((spi_n)==SPI_0)? PC: PE)
#define PCS3_PORT(spi_n) (((spi_n)==SPI_0)? PC: PE)
#define PCS4_PORT(spi_n) (PC)
#define PCS5_PORT(spi_n) (PB)
																			//SPI_0 SPI_1 SPI_2
#define PSCK_PORT(spi_n) (((spi_n)==SPI_0)? PD: ((spi_n)==SPI_1)? PE: PB)   //PTD1, PTE2, PTB21
#define PSOUT_PORT(spi_n) (((spi_n)==SPI_0)? PD: ((spi_n)==SPI_1)? PE: PB)  //PTD2, PTE1, PTB22
#define PSIN_PORT(spi_n) (((spi_n)==SPI_0)? PD: ((spi_n)==SPI_1)? PE: PB)   //PTD3, PTE3, PTB23

#define PCS0_PIN(spi_n) (((spi_n)==SPI_0)? 4: ((spi_n)==SPI_1)? 4: 20) 
#define PCS1_PIN(spi_n) (((spi_n)==SPI_0)? 3: 0)
#define PCS2_PIN(spi_n) (((spi_n)==SPI_0)? 2: 5)
#define PCS3_PIN(spi_n) (((spi_n)==SPI_0)? 1: 6)
#define PCS4_PIN(spi_n) (0)
#define PCS5_PIN(spi_n) (23)
#define PSCK_PIN(spi_n) (((spi_n)==SPI_0)? 1: ((spi_n)==SPI_1)? 2: 21)
#define PSOUT_PIN(spi_n) (((spi_n)==SPI_0)? 2: ((spi_n)==SPI_1)? 1: 22)
#define PSIN_PIN(spi_n) (((spi_n)==SPI_0)? 3: ((spi_n)==SPI_1)? 3: 23)

#define PCS0_ALT (ALTERNATIVE_2)
#define PCS1_ALT (ALTERNATIVE_2)
#define PCS2_ALT (ALTERNATIVE_2)
#define PCS3_ALT (ALTERNATIVE_2)
#define PCS4_ALT (ALTERNATIVE_2)
#define PCS5_ALT (ALTERNATIVE_3)
#define PSCK_ALT (ALTERNATIVE_2)  
#define PSOUT_ALT (ALTERNATIVE_2)
#define PSIN_ALT (ALTERNATIVE_2)


typedef enum {PIN_DISABLE, ALTERNATIVE_1, ALTERNATIVE_2, ALTERNATIVE_3, ALTERNATIVE_4, 
									ALTERNATIVE_5, ALTERNATIVE_6, ALTERNATIVE_7} mux_alt;
typedef enum {OPEN_DRAIN, PUSH_PULL} pin_mode;

/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/


/*******************************************************************************
 * VARIABLES WITH GLOBAL SCOPE
 ******************************************************************************/

// +ej: unsigned int anio_actual;+


/*******************************************************************************
 * FUNCTION PROTOTYPES FOR PRIVATE FUNCTIONS WITH FILE LEVEL SCOPE
 ******************************************************************************/



static void ClockGatingAndInterruptEnable(uint8_t SPI_n);

static void PinsConfigMaster (uint8_t SPI_n);

static void PinConfig (uint8_t pin, uint8_t mux_alt, uint8_t interrupt_alt, uint8_t mode);

// Configura los PCRs de todos los PCS del puerto pedido
static void PCSInit(uint8_t SPI_n);


/*******************************************************************************
 * ROM CONST VARIABLES WITH FILE LEVEL SCOPE
 ******************************************************************************/

// +ej: static const int temperaturas_medias[4] = {23, 26, 24, 29};+


/*******************************************************************************
 * STATIC VARIABLES AND CONST VARIABLES WITH FILE LEVEL SCOPE
 ******************************************************************************/

// +ej: static int temperaturas_actuales[4];+


/*******************************************************************************
 *******************************************************************************
                        GLOBAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/

static PORT_Type* portPtrs[] = PORT_BASE_PTRS;
static SPI_Type* SPIPtrs[] = SPI_BASE_PTRS;
static SIM_Type* sim_ptr = SIM;				// For clock enable


bool SPI_config (uint8_t SPI_n, SPI_config_t * config){

	if ( (SPI_n!=SPI_0 && SPI_n!=SPI_1 && SPI_n!=SPI_2) || config->frame_size<4){
		return false;
	}
	
	ClockGatingAndInterruptEnable(SPI_n);

	if(config->type){	//Si es master
		PCSInit(SPI_n);
		PinsConfigMaster(SPI_n);
	}
	else{
		//Mostro, no va a pasar nada xd
	}

	
	// MCR Setup
	SPIPtrs[SPI_n]->MCR = 0x00 | SPI_MCR_HALT(1);	// Paramos toda comunicacion
	SPIPtrs[SPI_n]->MCR |= (SPI_MCR_MSTR(config->type) | SPI_MCR_PCSIS(config->PCS_inactive_state) ); 	//TODO: PCSIS pa todos y todas

	// TCR Setup
	SPIPtrs[SPI_n]->TCR |= SPI_TCR_SPI_TCNT(0);
	
	// CTAR Setup
	SPIPtrs[SPI_n]->CTAR[0] = 0x0; 	//Reset CTAR
	
	if(config->type){		// Master Mode
		SPIPtrs[SPI_n]->CTAR[0] = (SPI_CTAR_DBR(0) | SPI_CTAR_FMSZ(config->frame_size-1) | 
				SPI_CTAR_CPOL(config->clk_pol) | SPI_CTAR_CPHA(config->clk_phase) | 
				SPI_CTAR_LSBFE(config->LSB_fist) | SPI_CTAR_BR(config->Baud_rate_scaler));
	} 
	
	else {					// Slave Mode
		SPIPtrs[SPI_n]->CTAR_SLAVE[0] = (SPI_CTAR_FMSZ(config->frame_size-1) | SPI_CTAR_CPOL(config->clk_pol) | 
										SPI_CTAR_CPHA(config->clk_phase));
	}

	// SR Setup
	SPIPtrs[SPI_n]->SR |= SPI_SR_EOQF(1) | SPI_SR_RXCTR(1) | SPI_SR_TXCTR(1) | SPI_SR_TCF(1);

	// RSER Setup
	// TODO: Por el momento es sin interrupciones, pero se puede mejorar

	// PUSHR Setup
	SPIPtrs[SPI_n]->PUSHR = SPI_PUSHR_CTAS(0);

	// Enable SPI
	SPIPtrs[SPI_n]->MCR &= ~SPI_MCR_HALT(1);	// Reanudamos toda comunicacion

	return true;
}

/*******************************************************************************
 *******************************************************************************
                        LOCAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/
void ClockGatingAndInterruptEnable(uint8_t SPI_n){		//Pag 319
	switch(SPI_n){
		case SPI_0:
			sim_ptr->SCGC6 |= SIM_SCGC6_SPI0(1); 
			NVIC_EnableIRQ(SPI0_IRQn);
			break;
		case SPI_1:
			sim_ptr->SCGC6 |= SIM_SCGC6_SPI1(1);
			NVIC_EnableIRQ(SPI1_IRQn);
			break;
		case SPI_2:
			sim_ptr->SCGC3 |= SIM_SCGC3_SPI2(1); 
			NVIC_EnableIRQ(SPI2_IRQn);
			break;
	}
}

void PinsConfigMaster (uint8_t SPI_n){
	PinConfig(PORTNUM2PIN(PSCK_PORT(SPI_n),PSCK_PIN(SPI_n)), PSCK_ALT, GPIO_IRQ_MODE_DISABLE, PUSH_PULL);
	PinConfig(PORTNUM2PIN(PSIN_PORT(SPI_n),PSIN_PIN(SPI_n)), PSIN_ALT, GPIO_IRQ_MODE_DISABLE, PUSH_PULL);			
	PinConfig(PORTNUM2PIN(PSOUT_PORT(SPI_n),PSOUT_PIN(SPI_n)), PSOUT_ALT, GPIO_IRQ_MODE_DISABLE, PUSH_PULL);
}

void PinConfig (uint8_t pin, uint8_t mux_alt, uint8_t interrupt_alt, uint8_t mode){
	uint32_t portn = PIN2PORT(pin); 				// Port number
	uint32_t num = PIN2NUM(pin); 					// Pin number

	PORT_Type *port = portPtrs[portn];

	port->PCR[num]=0x00; 							//Clear pin

	port->PCR[num] &= ~PORT_PCR_MUX_MASK;			//Clear
	port->PCR[num] &= ~PORT_PCR_IRQC_MASK;

	port->PCR[num] |= PORT_PCR_MUX(mux_alt); 
	port->PCR[num] |= PORT_PCR_IRQC(interrupt_alt);

	switch(mode){
		case OPEN_DRAIN:
			port->PCR[num] |= PORT_PCR_ODE(1);
			break;
		case PUSH_PULL:
			port->PCR[num] &= ~PORT_PCR_ODE(1);
			break;
	}
}

void PCSInit(uint8_t SPI_n){
	switch (SPI_n){
	case SPI_0:
		portPtrs[PCS4_PORT(SPI_n)]->PCR[PCS4_PIN(SPI_n)]=0x00;
		portPtrs[PCS4_PORT(SPI_n)]->PCR[PCS4_PIN(SPI_n)] |= PORT_PCR_MUX(PCS4_ALT);

		portPtrs[PCS5_PORT(SPI_n)]->PCR[PCS5_PIN(SPI_n)]=0x00;	
		portPtrs[PCS5_PORT(SPI_n)]->PCR[PCS5_PIN(SPI_n)] |= PORT_PCR_MUX(PCS5_ALT);
	
	case SPI_1:
		portPtrs[PCS1_PORT(SPI_n)]->PCR[PCS1_PIN(SPI_n)]=0x00;
		portPtrs[PCS1_PORT(SPI_n)]->PCR[PCS1_PIN(SPI_n)] |= PORT_PCR_MUX(PCS1_ALT);

		portPtrs[PCS2_PORT(SPI_n)]->PCR[PCS2_PIN(SPI_n)]=0x00;
		portPtrs[PCS2_PORT(SPI_n)]->PCR[PCS2_PIN(SPI_n)] |= PORT_PCR_MUX(PCS2_ALT);

		portPtrs[PCS3_PORT(SPI_n)]->PCR[PCS3_PIN(SPI_n)]=0x00;
		portPtrs[PCS3_PORT(SPI_n)]->PCR[PCS3_PIN(SPI_n)] |= PORT_PCR_MUX(PCS3_ALT);

	case SPI_2:
		portPtrs[PCS0_PORT(SPI_n)]->PCR[PCS0_PIN(SPI_n)]=0x00;
		portPtrs[PCS0_PORT(SPI_n)]->PCR[PCS0_PIN(SPI_n)] |= PORT_PCR_MUX(PCS0_ALT);
		break;
	
	default:
		break;
	}
}


bool SPITransferCompleteFlag(uint8_t SPI_n){
	return (SPIPtrs[SPI_n]->SR & SPI_SR_TCF(1));
}


uint32_t SPIRead(uint8_t SPI_n){
	SPIPtrs[SPI_n]->SR |= SPI_SR_TCF(1);
	return SPIPtrs[SPI_n]->POPR;
}

void SPIWrite(uint8_t SPI_n, uint16_t msg, uint8_t PCS){
	SPIPtrs[SPI_n]->PUSHR &= ~SPI_PUSHR_TXDATA_MASK & ~SPI_PUSHR_PCS_MASK;
	SPIPtrs[SPI_n]->PUSHR |= SPI_PUSHR_TXDATA(msg) | SPI_PUSHR_PCS(1)<<PCS; 
}
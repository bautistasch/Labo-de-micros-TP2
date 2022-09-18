/***************************************************************************//**
  @file		I2Cm.c
  @brief	
  @author	Grupo 5
  @date		13 sep. 2022
 ******************************************************************************/

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include "I2Cm.h"
#include "MCAL/gpio.h"
#include "MK64F12.h"
#include "hardware.h"
#include "stdbool.h"
/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

// TODO: En modo MASTER RX, el valor a responder en el ACKbit se debe configurar antes de comenzar la lectura delbyte

#define I2C_COUNT	3

#define BUS_CLK	50000000UL

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


/*******************************************************************************
 * ROM CONST VARIABLES WITH FILE LEVEL SCOPE
 ******************************************************************************/

static I2C_Type* const I2CPtrs[] = I2C_BASE_PTRS;
static IRQn_Type const I2CIRQs[] = I2C_IRQS;

static PORT_Type* const portPtr[] = PORT_BASE_PTRS;

static const uint8_t I2CPinPorts[] =    { PB, PC, PA, PE };
static const uint8_t I2CPinPinsSCL[] =  { 2,  10, 12, 24 };
static const uint8_t I2CPinPinsSDA[] =  { 3,  11, 13, 25 };
static const uint8_t I2CPinAlts[] =     { 2,  2,  5,  5  };

static __IO uint32_t* const I2CClkSimPtr[] = {&(SIM->SCGC4), &(SIM->SCGC4), &(SIM->SCGC1), &(SIM->SCGC4)};
static const uint32_t I2CClkSimMask[] = {SIM_SCGC4_I2C0_MASK, SIM_SCGC4_I2C1_MASK, SIM_SCGC1_I2C2_MASK, SIM_SCGC4_I2C0_MASK};


/*******************************************************************************
 * STATIC VARIABLES AND CONST VARIABLES WITH FILE LEVEL SCOPE
 ******************************************************************************/


/*******************************************************************************
 *******************************************************************************
                        GLOBAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/

/**
 * @brief Inicializa el modulo I2C
 * @param id: Instancia del I2C [0 - 1]
*/
void I2CmInit(I2CPort_t id) {

// Clock Gating

	//TODO: Enable CLK for PORTx
	*(I2CClkSimPtr[id]) |= I2CClkSimMask[id];

// Config pins (ALT, Open Drain, NO Pullup)

//	portPtr[I2CPinPorts[id]]->PCR[I2CPinPinsSCL[id]] = PORT_PCR_MUX(I2CPinAlts[id]) | PORT_PCR_ODE_MASK;
//	portPtr[I2CPinPorts[id]]->PCR[I2CPinPinsSDA[id]] = PORT_PCR_MUX(I2CPinAlts[id]) | PORT_PCR_ODE_MASK;

	// TEST:
	portPtr[I2CPinPorts[id]]->PCR[I2CPinPinsSCL[id]] = PORT_PCR_MUX(I2CPinAlts[id]) | PORT_PCR_ODE_MASK | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
	portPtr[I2CPinPorts[id]]->PCR[I2CPinPinsSDA[id]] = PORT_PCR_MUX(I2CPinAlts[id]) | PORT_PCR_ODE_MASK | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

// I2C Master config

	// Clock Divider
	//I2CPtrs[id%I2C_COUNT]->F = I2C_F_ICR(0x2B);		// Divider to 512 with no multiplier	(SCL: 100k)
	I2CPtrs[id%I2C_COUNT]->F = I2C_F_ICR(0x3F) | I2C_F_MULT(2);        // Divider to 3840 with 4 multiplier    (SCL: 3.26k)

	// Enable I2C. No Enable Master Mode and interrupts yet
	//	I2CPtrs[id%I2C_COUNT]->C1 = I2C_C1_IICEN_MASK;

	// Enable IRQ in NVIC

	NVIC_EnableIRQ(I2CIRQs[id%I2C_COUNT]);

}

typedef struct{
	uint8_t writeSize;
	uint8_t writtenBytesCounter;
	uint8_t* writeBuffer;
	uint8_t address;
	bool repeatedStartRealeased;
}writeState_t;

typedef struct{
	uint8_t readSize;
	uint8_t readBytesCounter;
	uint8_t* readBuffer;
}readState_t;

typedef enum {MASTER_RX, MASTER_TX, I2C_FAIL} I2C_STATES;

I2C_STATES i2cStates [] = {I2C_FAIL, I2C_FAIL};

static writeState_t writeState [I2C_COUNT];
static readState_t readState [I2C_COUNT];

/**
 * @brief realiza una transmision y recepcion por I2C
 * @param address address del slave
 * @param writeBuffer buffer de escritura
 * @param writeSize Tamano del buffer de escritura
 * @param readBuffer buffer para guardar la lectura
 * @param readSize Tamano del buffer de lectura
*/
void I2CmStartTransaction(I2CPort_t id, uint8_t address, uint8_t* writeBuffer, uint8_t writeSize, uint8_t* readBuffer, uint8_t readSize) {

	I2C_Type* pI2C = I2CPtrs[id%I2C_COUNT];

  // Initialize RAM variables

	writeState[id%I2C_COUNT].writeSize = writeSize;
	writeState[id%I2C_COUNT].writeBuffer = writeBuffer;
	writeState[id%I2C_COUNT].address = address;
	writeState[id%I2C_COUNT].writtenBytesCounter = 0;
	writeState[id%I2C_COUNT].repeatedStartRealeased = false;
	readState[id%I2C_COUNT].readSize = readSize;
	readState[id%I2C_COUNT].readBuffer = readBuffer;
	readState[id%I2C_COUNT].readBytesCounter = 0;

	uint8_t RWbit = writeSize > 0 ? 0 : 1; // bit de R/W luego del address. 0 si hay que escribir, 1 para leer
	i2cStates[id%I2C_COUNT] = RWbit == 0 ? MASTER_TX : MASTER_RX; // Se setea el primer estado de la fsm


	// Enable I2C in Master Mode, transmit mode and interrupts
	pI2C->C1 |= I2C_C1_IICEN_MASK;
	pI2C->C1 |= I2C_C1_IICIE_MASK;
	pI2C->C1 |= I2C_C1_TX_MASK;
	pI2C->C1 |= I2C_C1_MST_MASK;

	pI2C->D = address << 1 | RWbit;		// Slave Address + RW bit

}



/*******************************************************************************
 *******************************************************************************
                        LOCAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/

__ISR__ I2C0_IRQHandler() {
	//I2C_IRQ();
	I2C_Type* pI2C = I2CPtrs[0];
 	pI2C->S |= I2C_S_IICIF_MASK;     // borro el flag de la interrupcion
	if(pI2C->S & I2C_S_TCF_MASK){     // Me fijo si la interrupcion fue porque se termino una transaccion y LLEGO un ACK/NACK
		switch(i2cStates[0]){
		case MASTER_TX:
			if(writeState[0].writtenBytesCounter >= writeState[0].writeSize && readState[0].readSize == 0){  // si ya se escribio \todo lo que me pasaron y no hay nada para leer
				pI2C->C1 &= ~I2C_C1_MST_MASK;     // genero el Stop Signal
				pI2C->D = 0x00;
				return;                           // ESCRIBIR D PARA CLEANIAR TCF
			}
			else if(pI2C->S & I2C_S_RXAK_MASK){  // si entra, no me reconocio el ACK, => corto \todo
				pI2C->C1 &= ~I2C_C1_MST_MASK;     // genero el Stop Signal
				//pI2C->D = 0x00;
				return;								// ESCRIBIR D PARA CLEANIAR TCF
			}
			else if(!(pI2C->S & I2C_S_RXAK_MASK)){ // ASUMO QUE RXAK NO CAMBIA
				if(!writeState[0].repeatedStartRealeased){
					if(writeState[0].writtenBytesCounter < writeState[0].writeSize){ // si aun no escribi \todo
						pI2C->D = writeState[0].writeBuffer[writeState[0].writtenBytesCounter];
						writeState[0].writtenBytesCounter++;
						return;
					}
					else if(writeState[0].writtenBytesCounter == writeState[0].writeSize){	 // si escribi \todo, lanzo un Repeated start
						pI2C->C1 |= I2C_C1_RSTA_MASK;              // hago un repeated start
						pI2C->D = writeState[0].address << 1 | 1;  // meto el address y ya FIJO lectura, si bien se puede escribir, por la simpleza de nuestra funcion se asume lectura ya
						writeState[0].repeatedStartRealeased = true;
					}
				}
				else{     // Debo cambiar el modo !! a RX, pues ya envie el Repeated start
					i2cStates[0] = MASTER_RX;
					pI2C->C1 &= ~I2C_C1_TX_MASK;  // Cambio la direccion
					uint8_t dummyData = pI2C->D;  // Leo dummy y disparo lectura
					dummyData++; // USING DUMMY
					return;
				}
			}
			break;
		case MASTER_RX:
			if(readState[0].readBytesCounter < ( readState[0].readSize - 1 ) ){  // si leeiste \todos menos el ultimo => ahora vas a leer el ultimo
				pI2C->C1 &= ~I2C_C1_TXAK_MASK;  // envio el AK
			 	readState[0].readBuffer[readState[0].readBytesCounter] = pI2C->D;
				readState[0].readBytesCounter++;
				return;
			}
			else{
				pI2C->C1 &= ~I2C_C1_MST_MASK;
			 	readState[0].readBuffer[readState[0].readBytesCounter] = pI2C->D;  //
				readState[0].readBytesCounter++;
				return;
			}
			break;
		default:
			break;
		}
	}
}
/*
__ISR__ I2C1_IRQHandler() {
	I2C_IRQ();
}

__ISR__ I2C2_IRQHandler() {
	I2C_IRQ();
}

static void I2C_IRQ() {

}
*/


 

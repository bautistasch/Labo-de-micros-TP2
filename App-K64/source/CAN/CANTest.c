/***************************************************************************//**
  @file     App.c
  @brief    Application functions
  @author   Nicolás Magliola
 ******************************************************************************/

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include "../MCAL/board.h"
#include "CAN.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

#define N 4
/*******************************************************************************
 * FUNCTION PROTOTYPES FOR PRIVATE FUNCTIONS WITH FILE LEVEL SCOPE
 ******************************************************************************/


/*******************************************************************************
 *******************************************************************************
                        GLOBAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/

 CANMsg_t mymsgReceive;
CANMsg_t* msgReceive;

uint8_t data[N]={'h','o','l','a'};

/* Función que se llama 1 vez, al comienzo del programa */
void App_Init (void)
{
    gpioMode(PIN_SW3, INPUT);					//Ya es pullup electricamente
    gpioMode(PIN_SW2, INPUT_PULLUP);

    msgReceive=&mymsgReceive;


}

/* Función que se llama constantemente en un ciclo infinito */
void App_Run (void)
{
	
    CANInit(0x105, msgReceive);

    while(1) {

		if (!gpioRead(PIN_SW3)){
			while (!gpioRead(PIN_SW3));
			CANSend(data, N);
		}

		if(newMsg()){
			printf("ID: %d\n Msg: ", mymsgReceive.ID);
			for(int i=0; i<mymsgReceive.length; i++){
				printf("%c", mymsgReceive.data[i]);
			}
			printf("\n");
		}
    }

}

/*******************************************************************************
 *******************************************************************************
                        LOCAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/


/*******************************************************************************
 ******************************************************************************/

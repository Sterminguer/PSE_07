/***************************************************************************//**
 * @file
 * @brief FreeRTOS Blink Demo for Energy Micro EFM32GG_STK3700 Starter Kit
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>


#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "croutine.h"
#include "time.h"

#include "em_gpio.h"
#include "em_chip.h"
#include "bsp.h"
#include "bsp_trace.h"

#include "sleep.h"

#define STACK_SIZE_FOR_TASK    (configMINIMAL_STACK_SIZE + 10)
#define TASK_PRIORITY          (tskIDLE_PRIORITY + 1)
#define MIDA_COLA 100 //tamano cola

#define RED_LED_PIN gpioPortF, 4

//definición colas
QueueHandle_t cola1, cola2;



/* Structure with parameters for LedBlink */
typedef struct {
  /* Delay between blink of led */
  portTickType delay;
  /* Number of led */
  int          ledNo;
} TaskParams_t;

/***************************************************************************//**
 * @brief Simple task which is blinking led
 * @param *pParameters pointer to parameters passed to the function
 ******************************************************************************/

static void LedBlink(void *pParameters)
{
  TaskParams_t     * pData = (TaskParams_t*) pParameters;
  const portTickType delay = pData->delay;

  for (;; ) {
    BSP_LedToggle(pData->ledNo);
    vTaskDelay(delay);
  }
}


//tarea para testear la conexion i2c con semaforo, no se utiliza por problemas con el sensor
/*
static void prueba81(void *pParameters)
{
	bool aux;
	for(;;)
	{
		aux=I2C_Test();
		if(aux==false)
			printf("error\n");
		else
			printf("correcto\n");
	}
}
*/

//Crear datos de CO2 aleatorios en un rango entre 0 y 3333 simulando la entrada del sensor CCS811 \
y enviando a la cola 1, incluyendo también un cierto delay para tratar el dato.
static void EscribirDatosCola(void *pParameters)
{
	int valor;
	for(;;){
		valor = (rand()%3333)+1;
		xQueueSend(cola1, &valor, portMAX_DELAY);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

//Función que lee datos de la cola 1, y los envia a la cola 2
static void LeerDatosCola(void *pParameters)
{
	int lecturaValor;
	for(;;){
		if ((xQueueReceive(cola1, &lecturaValor, portMAX_DELAY)) == pdPASS){
			//printf("valor: %d\n", lecturaValor);

			xQueueSend(cola2, &lecturaValor, portMAX_DELAY);
		}

		else
			printf("Error\n");
	}
}

//Función que lee dato de la cola 2, y en función de dicho dato, realiza una acción.
//Si el valor está entre 1500 y 2750, se enciende un único led (Riesgo moderado).
//Si el valor es mayor o igual a 2750, se encienden dos leds (Riesgo elevado).
//Si el valor es menor a 1750, no se enciende ningun led (Riesgo bajo).

static void Accion(void *pParameters)
{
	int lecturaValor2;
		for(;;)
		{
			if ((xQueueReceive(cola2, &lecturaValor2, portMAX_DELAY)) == pdPASS){
				printf("valor mesuretat: %d\n", lecturaValor2);
				if((lecturaValor2 > 1500) && (lecturaValor2 < 2750)) { //nivel de contaminacion medio (Únicamente se enciende led 0)
					if (BSP_LedGet(0) == 0)
							BSP_LedToggle(0);
					if (BSP_LedGet(1) == 1)
							BSP_LedToggle(1);
				}
				else if (lecturaValor2 >= 2750)//nivel de contaminacion alto (Se encienden ambos Leds)
				{
						if((BSP_LedGet(0) == 0) && (BSP_LedGet(1) == 0))
						{
							BSP_LedToggle(0);
							BSP_LedToggle(1);
						}
						if ((BSP_LedGet(0) == 1) && (BSP_LedGet(1) == 0))
							BSP_LedToggle(1);
				}
				else // No contamina, nivel 0
				{
					if((BSP_LedGet(0) == 1) && (BSP_LedGet(1) == 1))
					{
						BSP_LedToggle(0);
						BSP_LedToggle(1);
					}
					if ((BSP_LedGet(0) == 1) && (BSP_LedGet(1) == 0))
						BSP_LedToggle(0);

				}

			}
		}

}

/***************************************************************************//**
 * @brief  Main function
 ******************************************************************************/
int main(void)
 {
  /* Chip errata */
  CHIP_Init();

  /* If first word of user data page is non-zero, enable Energy Profiler trace */
  BSP_TraceProfilerSetup();

  //Inicializa el sensor CCS811
  BSP_I2C_Init(0xB6);

  /* Initialize LED driver */
  BSP_LedsInit();
  BSP_LedSet(0);
  BSP_LedSet(1);
  BSP_LedToggle(0);
  BSP_LedToggle(1);
  /* Setting state of leds*/

  //Creación colas
  cola1 = xQueueCreate(MIDA_COLA, sizeof(int));
  cola2 = xQueueCreate(MIDA_COLA, sizeof(int));


  /* Initialize SLEEP driver, no calbacks are used */
  SLEEP_Init(NULL, NULL);
#if (configSLEEP_MODE < 3)
  /* do not let to sleep deeper than define */
  SLEEP_SleepBlockBegin((SLEEP_EnergyMode_t)(configSLEEP_MODE + 1));
#endif
  /* Parameters value for taks*/
  static TaskParams_t parametersToTask1 = { pdMS_TO_TICKS(1000), 0 };
  static TaskParams_t parametersToTask2 = { pdMS_TO_TICKS(500), 1 };

  /*Create two task for blinking leds*/
  //xTaskCreate(LedBlink, (const char *) "LedBlink1", STACK_SIZE_FOR_TASK, &parametersToTask1, TASK_PRIORITY, NULL);
  //xTaskCreate(LedBlink, (const char *) "LedBlink2", STACK_SIZE_FOR_TASK, &parametersToTask2, TASK_PRIORITY, NULL);
  //xTaskCreate(prueba81, (const char *) "prueba81", STACK_SIZE_FOR_TASK, NULL, TASK_PRIORITY, NULL);
  xTaskCreate(EscribirDatosCola, (const char *) "EscribirDatosCola", STACK_SIZE_FOR_TASK, NULL, TASK_PRIORITY, NULL);
  xTaskCreate(LeerDatosCola, (const char *) "LeerDatosCola", STACK_SIZE_FOR_TASK, NULL, TASK_PRIORITY, NULL);
  xTaskCreate(Accion, (const char *) "Accion", STACK_SIZE_FOR_TASK, NULL, TASK_PRIORITY, NULL);

  /*Start FreeRTOS Scheduler*/
  vTaskStartScheduler();

  return 0;
}

//Función que nos permite hacer printf().
int _write(int file, const char *ptr, int len) {
    int x;
    for (x = 0; x < len; x++) {
       ITM_SendChar (*ptr++);
    }
    return (len);
}

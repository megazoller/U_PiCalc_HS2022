/*
 * U_PiCalc_HS2022.c
 *
 * Created: 20.03.2018 18:32:07
 * Author : -
 */ 

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "avr_compiler.h"
#include "pmic_driver.h"
#include "TC_driver.h"
#include "clksys_driver.h"
#include "sleepConfig.h"
#include "port_driver.h"
#include "math.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "stack_macros.h"

#include "mem_check.h"

#include "init.h"
#include "utils.h"
#include "errorHandler.h"
#include "NHD0420Driver.h"

#include "ButtonHandler.h"


void controllerTask(void* pvParameters);
void leibnizTask(void* pvParameters);
void asinTask(void *pvParameters);
void breakTask(void* pvParameters);
void UITask(void* pvParameters);

//EventgroupButtons
EventGroupHandle_t egButtonEvents = NULL;
#define BUTTON1_SHORT	0x01 // Start Calculate
#define BUTTON2_SHORT	0x02 // Stop Calculate
#define BUTTON3_SHORT	0x04 // Reset 
#define BUTTON4_SHORT	0x08 // Switch Algorithm
#define TASK_DONE		0x10 // 5 Digits after Dot reached

//Modes for Finite State Machine
#define MODE_IDLE 0
#define MODE_LEIBNIZ_OFF 1
#define MODE_ASIN_OFF 2
#define MODE_LEIBNIZ_ON 3
#define MODE_ASIN_ON 4
#define RESET_INACTIVE 0
#define RESET_ACTIVE 1
//#define MODE_ALARMALARM 3

TaskHandle_t LEIBNIZHANDLE = NULL;
TaskHandle_t ASINHANDLE = NULL;

uint8_t mode = 0;
uint32_t n = 3;
double pi_calc = 0, piquarter = 1.0;
uint32_t asinstart = 0, leibnizstart = 0, asintime = 0, leibniztime = 0;
char countstring[25];

int main(void)
{
	vInitClock();
	
	xTaskCreate( controllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 3, NULL);
	xTaskCreate( leibnizTask, (const char *) "leibniz_tsk", configMINIMAL_STACK_SIZE+150, NULL, 1, &LEIBNIZHANDLE);
	xTaskCreate( asinTask, (const char*) "asin_tsk", configMINIMAL_STACK_SIZE, NULL, 1, &ASINHANDLE); //Init ButtonTask. Medium Priority. Somehow important to time Button debouncing and timing.
	xTaskCreate( UITask, (const char *) "ui_task", configMINIMAL_STACK_SIZE, NULL, 2, NULL); //Init UITask. Lowest Priority. Least time critical.
	
	vInitDisplay();
	vDisplayClear();
	vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
	
	vTaskStartScheduler();
	return 0;
}

void controllerTask(void* pvParameters)
{
	vTaskSuspend(LEIBNIZHANDLE);
	vTaskSuspend(ASINHANDLE);
	egButtonEvents = xEventGroupCreate();
	while(1)
	{
		updateButtons();
		if(getButtonPress(BUTTON1) == SHORT_PRESSED)
		{
			xEventGroupSetBits(egButtonEvents, BUTTON1_SHORT);
		}
		if(getButtonPress(BUTTON2) == SHORT_PRESSED)
		{
			xEventGroupSetBits(egButtonEvents, BUTTON2_SHORT);
		}
		if(getButtonPress(BUTTON3) == SHORT_PRESSED)
		{
			xEventGroupSetBits(egButtonEvents, BUTTON3_SHORT);
		}
		if(getButtonPress(BUTTON4) == SHORT_PRESSED)
		{
			xEventGroupSetBits(egButtonEvents, BUTTON4_SHORT);
		}
		vTaskDelay(10/portTICK_RATE_MS);
	}
}
		
void UITask(void* pvParameters)
{
	while(1)
	{
		switch(mode)
		{
			case MODE_IDLE:
			if(xEventGroupGetBits(egButtonEvents) & BUTTON4_SHORT)
			{
				xEventGroupClearBits(egButtonEvents, BUTTON4_SHORT);
				vDisplayClear();
				vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
				vDisplayWriteStringAtPos(1,0, "Leibniz");
				countstring[0] = '\0';
				mode = MODE_LEIBNIZ_OFF;
			}
			if(xEventGroupGetBits(egButtonEvents) & BUTTON1_SHORT)
			{
				xEventGroupClearBits(egButtonEvents, BUTTON1_SHORT);
				vDisplayClear();
				vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
				vDisplayWriteStringAtPos(1,0, "Leibniz");
				vTaskResume(LEIBNIZHANDLE);
				mode = MODE_LEIBNIZ_ON;
			}
			break;
			case MODE_LEIBNIZ_OFF:
			if(xEventGroupGetBits(egButtonEvents) & BUTTON3_SHORT)
			{
				pi_calc = 0;
			}
			if(xEventGroupGetBits(egButtonEvents) & BUTTON1_SHORT)
			{
				xEventGroupClearBits(egButtonEvents, BUTTON1_SHORT);
				vDisplayClear();
				vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
				vDisplayWriteStringAtPos(1,0, "Leibniz");
				vTaskResume(LEIBNIZHANDLE);
				mode = MODE_LEIBNIZ_ON;
			}
			if(xEventGroupGetBits(egButtonEvents) & BUTTON4_SHORT)
			{
				xEventGroupClearBits(egButtonEvents, BUTTON4_SHORT);
				vDisplayClear();
				vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
				vDisplayWriteStringAtPos(1,0, "Asin");
				countstring[0] = '\0';
				mode = MODE_ASIN_OFF;
			}
			break;
			case MODE_ASIN_OFF:
			if(xEventGroupGetBits(egButtonEvents) & BUTTON3_SHORT)
			{
				pi_calc = 0;
			}
			if(xEventGroupGetBits(egButtonEvents) & BUTTON1_SHORT)
			{
				xEventGroupClearBits(egButtonEvents, BUTTON1_SHORT);
				vDisplayClear();
				vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
				vDisplayWriteStringAtPos(1,0, "Asin");
				vTaskResume(ASINHANDLE);
				mode = MODE_ASIN_ON;
			}
			if(xEventGroupGetBits(egButtonEvents) & BUTTON4_SHORT)
			{
				xEventGroupClearBits(egButtonEvents, BUTTON4_SHORT);
				vDisplayClear();
				vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
				vDisplayWriteStringAtPos(1,0, "Leibniz");
				countstring[0] = '\0';
				mode = MODE_LEIBNIZ_OFF;
			}
			break;
			case MODE_LEIBNIZ_ON:
			if(xEventGroupGetBits(egButtonEvents) & BUTTON4_SHORT)
			{
				xEventGroupClearBits(egButtonEvents, BUTTON4_SHORT);
				vDisplayClear();
				vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
				vDisplayWriteStringAtPos(1,0, "Asin");
				countstring[0] = '\0';
				vTaskSuspend(LEIBNIZHANDLE);
				vTaskResume(ASINHANDLE);
				mode = MODE_ASIN_ON;
			}
			if(xEventGroupGetBits(egButtonEvents) & BUTTON2_SHORT)
			{
				xEventGroupClearBits(egButtonEvents, BUTTON2_SHORT);
				vDisplayClear();
				vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
				vDisplayWriteStringAtPos(1,0, "Leibniz");
				vTaskSuspend(LEIBNIZHANDLE);
				mode = MODE_LEIBNIZ_OFF;
			}
			if(pi_calc < 3.1416)
			{
				sprintf(&countstring[0], "Total Time: %lums", leibniztime);
			}
			else
			{
				leibniztime = xTaskGetTickCount() - leibnizstart;
				sprintf(&countstring[0], "Timer: %lu", leibniztime);
			}
			break;
			case MODE_ASIN_ON:
			if(xEventGroupGetBits(egButtonEvents) & BUTTON4_SHORT)
			{
				xEventGroupClearBits(egButtonEvents, BUTTON4_SHORT);
				vDisplayClear();
				vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
				vDisplayWriteStringAtPos(1,0, "Leibniz");
				countstring[0] = '\0';
				vTaskSuspend(ASINHANDLE);
				vTaskResume(LEIBNIZHANDLE);
				mode = MODE_LEIBNIZ_ON;
			}
			if(xEventGroupGetBits(egButtonEvents) & BUTTON2_SHORT)
			{
				xEventGroupClearBits(egButtonEvents, BUTTON2_SHORT);
				vDisplayClear();
				vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
				vDisplayWriteStringAtPos(1,0, "Asin");
				vTaskSuspend(ASINHANDLE);
				mode = MODE_ASIN_OFF;
			}
			break;
			default:
			break;
		}
		char pistring[12];
		sprintf(&pistring[0], "PI: %.8f", pi_calc);
		vDisplayWriteStringAtPos(3,0, "%s", pistring);
		vDisplayWriteStringAtPos(2,0, "%s", countstring);
		vTaskDelay(500);
	}
}

void leibnizTask(void* pvParameters)
{
	leibnizstart = xTaskGetTickCount();
	vDisplayClear();
	vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
	vDisplayWriteStringAtPos(1,0, "Leibniz");
	countstring[0] = '\0';
	piquarter = 1.0;
	pi_calc = 0;
	n = 3;
	leibniztime = 0;
	while (1)
	{
		if(xEventGroupGetBits(egButtonEvents) & BUTTON3_SHORT)
		{
			leibnizstart = xTaskGetTickCount();
			xEventGroupClearBits(egButtonEvents, BUTTON3_SHORT);
			vDisplayClear();
			vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
			vDisplayWriteStringAtPos(1,0, "Leibniz");
			countstring[0] = '\0';
			piquarter = 1.0;
			pi_calc = 0;
			n = 3;
			leibniztime = 0;
		}
		piquarter = piquarter -(1.0/n) + (1.0/(n+2));
		n = n+4;
		pi_calc = piquarter * 4;
	}
}

void asinTask(void* pvParameters)
{
	asinstart = xTaskGetTickCount();
	vDisplayClear();
	vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
	vDisplayWriteStringAtPos(1,0, "Asin");
	countstring[0] = '\0';
	pi_calc = 0;
	asintime = 0;
	while (1)
	{
		if(xEventGroupGetBits(egButtonEvents) & BUTTON3_SHORT)
		{
			asinstart = xTaskGetTickCount();
			xEventGroupClearBits(egButtonEvents, BUTTON3_SHORT);
			vDisplayClear();
			vDisplayWriteStringAtPos(0,0,"Pi-Calc E.Z. 2022");
			vDisplayWriteStringAtPos(1,0, "Asin");
			countstring[0] = '\0';
			pi_calc = 0;
			asintime = 0;
		}
		pi_calc = 2 * asin(1.0);
		if(asintime<1)
		{
			asintime = xTaskGetTickCount() - asinstart;
		}
		sprintf(&countstring[0], "Total Time: %lums", asintime);
		vTaskDelay(500);
	}
}
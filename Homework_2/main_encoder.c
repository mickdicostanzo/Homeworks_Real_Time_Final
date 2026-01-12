/*
MEMBRI DEL GRUPPO:
- Annunziata Giovanni              DE6000015
- Di Costanzo Michele Pio          DE6000001
- Di Palma Lorenzo                 N39001908 
- Zaccone Amedeo                   DE6000014 

*/

#include <stdio.h>
#include <pthread.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include <stdlib.h>
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Local includes. */
#include "console.h"

#define mainENCODER_TASK_PRIORITY        ( tskIDLE_PRIORITY + 3 )
#define mainRT_TASK_PRIORITY             ( tskIDLE_PRIORITY + 4 )
#define mainDIAGNOSTIC_TASK_PRIORITY     ( tskIDLE_PRIORITY + 1 )
#define mainSCOPE_TASK_PRIORITY          ( tskIDLE_PRIORITY)


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

void rt_task1(void*pvParameters);
void rt_task2(void*pvParameters);
void enc_task(void* pvParameters);
void diagnostic(void* pvParameters);
void scope_task(void* pvParameters);



#define TICK_PERIOD 5 

struct enc_str
{
	unsigned int slit;		//valori oscillanti tra 0 e 1
	unsigned int home_slit;	//1 se in home, 0 altrimenti
    xSemaphoreHandle mutex;
};
static struct enc_str enc_data;

struct _cound_time_str{
	unsigned int count;
	unsigned long int time_diff;
	xSemaphoreHandle mutex;
};
static struct _cound_time_str count_time_data;

struct _slack_str{
	unsigned long int slack_time_task1;
	unsigned long int slack_time_task2;
	xSemaphoreHandle mutex;
};
static struct _slack_str slack_data;

uint64_t fromTicktoUs(TickType_t ticks){
	return ( ( (uint64_t)ticks * 1000000UL ) / configTICK_RATE_HZ );
}


void rt_task1(void* PvParameters){
	TickType_t xNextWakeTime;
	const TickType_t xPeriod = pdMS_TO_TICKS( TICK_PERIOD/2 );
	xSemaphoreTake(count_time_data.mutex, portMAX_DELAY );
	count_time_data.count = 0;
	xSemaphoreGive( count_time_data.mutex );

	int last_value =0;
	TickType_t finish_time;

	xNextWakeTime = xTaskGetTickCount();
	for(;;){
		vTaskDelayUntil( &xNextWakeTime, xPeriod );
		xSemaphoreTake( enc_data.mutex, portMAX_DELAY );
		if( last_value == 0 && enc_data.slit == 1){
			last_value = 1;

			xSemaphoreTake( count_time_data.mutex, portMAX_DELAY );
			count_time_data.count++;
			xSemaphoreGive( count_time_data.mutex );

		}
		else if(last_value == 1 && enc_data.slit == 0){
			last_value = 0;
		}
		xSemaphoreGive( enc_data.mutex );

		/* Slack Time */

		//DA VERIFICARE

		finish_time = xTaskGetTickCount();
		TickType_t actual_deadline = xNextWakeTime + xPeriod;

		if(finish_time <= actual_deadline){
			xSemaphoreTake( slack_data.mutex, portMAX_DELAY );
			slack_data.slack_time_task1= (unsigned long)( (((uint64_t)actual_deadline- (uint64_t)finish_time) * 1000000000UL) / configTICK_RATE_HZ );  //slack in nanosecondi
			xSemaphoreGive( slack_data.mutex );
		}
		else{
			printf("DEADLINE MISS TASK1 finish time: %ld us\n",fromTicktoUs(finish_time));
			slack_data.slack_time_task1=(xNextWakeTime -finish_time);
		}
	}
}

void rt_task2(void* PvParameters){
	TickType_t xNextWakeTime;
	const TickType_t xPeriod = pdMS_TO_TICKS( TICK_PERIOD/2 );

	TickType_t time_home;
	TickType_t last_time_home;

	int first_measure = 1;
	int last_home_slit = 0;

	TickType_t finish_time;
	xNextWakeTime = xTaskGetTickCount();

	for(;;){
		vTaskDelayUntil( &xNextWakeTime, xPeriod );

		xSemaphoreTake( enc_data.mutex, portMAX_DELAY );


		if(enc_data.home_slit == 1 && last_home_slit == 0){
			last_home_slit = 1;
			if(first_measure){
				last_time_home = xTaskGetTickCount();
				first_measure = 0;
			}
			else{
				time_home = xTaskGetTickCount();

				xSemaphoreTake( count_time_data.mutex, portMAX_DELAY );
				count_time_data.time_diff =(unsigned long)( (((uint64_t)time_home- (uint64_t)last_time_home) * 1000000UL) / configTICK_RATE_HZ );  
				xSemaphoreGive( count_time_data.mutex );

				last_time_home = time_home;
			}
		}
		else if(enc_data.home_slit == 0){
			last_home_slit = 0;
		}

		xSemaphoreGive( enc_data.mutex );

		/* Slack Time */
		
		finish_time = xTaskGetTickCount();
		TickType_t actual_deadline = xNextWakeTime + xPeriod;
		if(finish_time <= actual_deadline){
			xSemaphoreTake( slack_data.mutex, portMAX_DELAY );
			slack_data.slack_time_task2= (unsigned long)( ((uint64_t)(actual_deadline- finish_time) * 1000000000UL) / configTICK_RATE_HZ );   //slack in nanosecondi
			xSemaphoreGive( slack_data.mutex );
		}
		else{
			
			printf("DEADLINE MISS TASK2 finish time: %ld us \n",fromTicktoUs(finish_time));
			slack_data.slack_time_task2= (xNextWakeTime - finish_time);

		}

	}
}

void scope_task(void* PvParameters){
	TickType_t xNextWakeTime;
	const TickType_t xPeriod = pdMS_TO_TICKS( TICK_PERIOD*100 );

	unsigned int rpm = 0;
	TickType_t diff_ticks;
	//TickType_t diff_us = 0;
	float diff_us =0;
	unsigned int count = 0;

	xNextWakeTime = xTaskGetTickCount();

	for(;;){
		vTaskDelayUntil( &xNextWakeTime, xPeriod );

		xSemaphoreTake( count_time_data.mutex, portMAX_DELAY );
		count = count_time_data.count ;
		diff_us = count_time_data.time_diff;
		xSemaphoreGive( count_time_data.mutex );

		printf("Rising Edge Counter : %d\t",count);
		rpm = (unsigned int)(60*1000000/diff_us);
		printf("RPM : %u\n",rpm);
	}

}


void enc_task( void * pvParameters ){

	printf("Encoder Start\n");
	TickType_t xNextWakeTime;

	xSemaphoreTake( enc_data.mutex, portMAX_DELAY );	
	enc_data.slit = 0;
	enc_data.home_slit = 0;
	xSemaphoreGive( enc_data.mutex );

	unsigned int count = 0;
	unsigned int slit_count = 0;
	unsigned int prev_slit = 0;

	srand(time(NULL));
	unsigned int semi_per = (rand() % 10) + 1;
	const TickType_t xPeriod = pdMS_TO_TICKS( TICK_PERIOD );

	xNextWakeTime = xTaskGetTickCount();

	for(;;){
		vTaskDelayUntil( &xNextWakeTime, xPeriod );
		xSemaphoreTake( enc_data.mutex, portMAX_DELAY );
		prev_slit = enc_data.slit;
		if (count%semi_per == 0) {
			enc_data.slit++;
			enc_data.slit%=2;			
	}
		if (prev_slit==0&&enc_data.slit==1) 					//fronte di salita
			slit_count=(++slit_count)%8;

		if (slit_count==0) enc_data.home_slit=enc_data.slit;
		else enc_data.home_slit=0;

		count++;
		xSemaphoreGive( enc_data.mutex );
}
}

void diagnostic(void* PvParameters){
	TickType_t xNextWakeTime;
	const TickType_t xPeriod = pdMS_TO_TICKS( TICK_PERIOD*100 );
	xNextWakeTime = xTaskGetTickCount();

	//TickType_t avg_slack=0;
	unsigned long int avg_slack = 0;
	int i = 0;
	int rounds = 10;

	for(;;){
		vTaskDelayUntil( &xNextWakeTime, xPeriod );

		xSemaphoreTake( slack_data.mutex, portMAX_DELAY );

		avg_slack += (slack_data.slack_time_task1 + slack_data.slack_time_task2)/2000;  //media degli slack in microsecondi

		xSemaphoreGive( slack_data.mutex );
		i++;
		if(i == rounds){
			avg_slack = avg_slack/rounds;
			printf("**********SLACK TIME: %ld us**********\n",avg_slack);
			i = 0;
		}
	}
}

void main( void )
{

	enc_data.mutex = xSemaphoreCreateMutex();
	count_time_data.mutex = xSemaphoreCreateMutex();
	slack_data.mutex = xSemaphoreCreateMutex();


	xTaskCreate(    enc_task,                   
					"Encoder",                                   
					configMINIMAL_STACK_SIZE,             
					NULL,                               
					mainENCODER_TASK_PRIORITY,       
					NULL );                               

	xTaskCreate(    rt_task1,                   
					"RT_Task1",                                   
					configMINIMAL_STACK_SIZE,              
					NULL,                                
					mainRT_TASK_PRIORITY,      
					NULL );   
					
	xTaskCreate(	rt_task2,                   
					"RT_Task2",                            
					configMINIMAL_STACK_SIZE,             
					NULL,                                 
					mainRT_TASK_PRIORITY,       
					NULL );

	xTaskCreate(    diagnostic,                   
					"Diagnostic",                                  
					configMINIMAL_STACK_SIZE,             
					NULL,                                  
					mainDIAGNOSTIC_TASK_PRIORITY ,      
					NULL );	

	xTaskCreate(    scope_task,                    
					"Buddy",                                   
					configMINIMAL_STACK_SIZE,             
					NULL,                                 
					mainSCOPE_TASK_PRIORITY,      
					NULL );

	vTaskStartScheduler();

	
	for( ;; ){
		if (getchar() == 'q') break;
	};
}
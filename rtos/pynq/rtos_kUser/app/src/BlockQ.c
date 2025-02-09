/*
    FreeRTOS V6.0.4 - Copyright (C) 2010 Real Time Engineers Ltd.

    ***************************************************************************
    *                                                                         *
    * If you are:                                                             *
    *                                                                         *
    *    + New to FreeRTOS,                                                   *
    *    + Wanting to learn FreeRTOS or multitasking in general quickly       *
    *    + Looking for basic training,                                        *
    *    + Wanting to improve your FreeRTOS skills and productivity           *
    *                                                                         *
    * then take a look at the FreeRTOS eBook                                  *
    *                                                                         *
    *        "Using the FreeRTOS Real Time Kernel - a Practical Guide"        *
    *                  http://www.FreeRTOS.org/Documentation                  *
    *                                                                         *
    * A pdf reference manual is also available.  Both are usually delivered   *
    * to your inbox within 20 minutes to two hours when purchased between 8am *
    * and 8pm GMT (although please allow up to 24 hours in case of            *
    * exceptional circumstances).  Thank you for your support!                *
    *                                                                         *
    ***************************************************************************

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    ***NOTE*** The exception to the GPL is included to allow you to distribute
    a combined work that includes FreeRTOS without being obliged to provide the
    source code for proprietary components outside of the FreeRTOS kernel.
    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public
    License and the FreeRTOS license exception along with FreeRTOS; if not it
    can be viewed here: http://www.freertos.org/a00114.html and also obtained
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!

    http://www.FreeRTOS.org - Documentation, latest information, license and
    contact details.

    http://www.SafeRTOS.com - A version that is certified for use in safety
    critical systems.

    http://www.OpenRTOS.com - Commercial support, development, porting,
    licensing and training services.
*/

/**
 * Creates six tasks that operate on three queues as follows:
 *
 * The first two tasks send and receive an incrementing number to/from a queue.
 * One task acts as a producer and the other as the consumer.  The consumer is a
 * higher priority than the producer and is set to block on queue reads.  The queue
 * only has space for one item - as soon as the producer posts a message on the
 * queue the consumer will unblock, pre-empt the producer, and remove the item.
 *
 * The second two tasks work the other way around.  Again the queue used only has
 * enough space for one item.  This time the consumer has a lower priority than the
 * producer.  The producer will try to post on the queue blocking when the queue is
 * full.  When the consumer wakes it will remove the item from the queue, causing
 * the producer to unblock, pre-empt the consumer, and immediately re-fill the
 * queue.
 *
 * The last two tasks use the same queue producer and consumer functions.  This time the queue has
 * enough space for lots of items and the tasks operate at the same priority.  The
 * producer will execute, placing items into the queue.  The consumer will start
 * executing when either the queue becomes full (causing the producer to block) or
 * a context switch occurs (tasks of the same priority will time slice).
 *
 * \page BlockQC blockQ.c
 * \ingroup DemoFiles
 * <HR>
 */

/*
Changes from V1.00:

	+ Reversed the priority and block times of the second two demo tasks so
	  they operate as per the description above.

Changes from V2.0.0

	+ Delay periods are now specified using variables and constants of
	  portTickType rather than unsigned long.

Changes from V4.0.2

	+ The second set of tasks were created the wrong way around.  This has been
	  corrected.
*/


#include <stdlib.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Demo program include files. */
#include "BlockQ.h"
#include "Print.h"

#include "checkErrors.h"


/******************************** Definitions *********************************/
#define blckqSTACK_SIZE		( ( unsigned short ) configMINIMAL_STACK_SIZE )
#define blckqNUM_TASK_SETS	( 3 )
#define GOAL_VAL	(3000)

/* Structure used to pass parameters to the blocking queue tasks. */
typedef struct BLOCKING_QUEUE_PARAMETERS
{
	xQueueHandle xQueue;					/*< The queue to be used by the task. */
	portTickType xBlockTime;			/*< The block time to use on queue reads/writes. */
	volatile short *psCheckVariable;	/*< Incremented on each successful cycle to check the task is still running. */
} xBlockingQueueParameters;

#define MAGIC_QUEUE_VALUE	(303U)


/********************************* Prototypes *********************************/
/* Task function that creates an incrementing number and posts it on a queue. */
static void vBlockingQueueProducer( void *pvParameters );

/* Task function that removes the incrementing number from a queue and checks that
it is the expected number. */
static void vBlockingQueueConsumer( void *pvParameters );


/*********************************** Values ***********************************/
/* Variables which are incremented each time an item is removed from a queue, and
found to be the expected value.
These are used to check that the tasks are still running. */
static volatile short sBlockingConsumerCount[ blckqNUM_TASK_SETS ] = { ( short ) 0, ( short ) 0, ( short ) 0 };

/* Variable which are incremented each time an item is posted on a queue.   These
are used to check that the tasks are still running. */
static volatile short sBlockingProducerCount[ blckqNUM_TASK_SETS ] = { ( short ) 0, ( short ) 0, ( short ) 0 };

#define BLOCK_Q_TASK_NUM	(6)
static TaskHandle_t blockQTasks[BLOCK_Q_TASK_NUM];

// all the parameters for the tasks
xBlockingQueueParameters allBQparams[BLOCK_Q_TASK_NUM];


/********************************* Functions **********************************/

void vStartBlockingQueueTasks( unsigned portBASE_TYPE uxPriority ) {

	xBlockingQueueParameters *pxQueueParameters1, *pxQueueParameters2;
	xBlockingQueueParameters *pxQueueParameters3, *pxQueueParameters4;
	xBlockingQueueParameters *pxQueueParameters5, *pxQueueParameters6;
	const unsigned portBASE_TYPE uxQueueSize1 = 1, uxQueueSize5 = 5;
	const portTickType xBlockTime = ( portTickType ) 1000 / portTICK_RATE_MS;
	const portTickType xDontBlock = ( portTickType ) 0;

	/* Create the first two tasks as described at the top of the file. */

	/* First create the structure used to pass parameters to the consumer tasks. */
	pxQueueParameters1 = &allBQparams[0];

	/* Create the queue used by the first two tasks to pass the incrementing number.
	Pass a pointer to the queue in the parameter structure. */
	pxQueueParameters1->xQueue = xQueueCreate(
			uxQueueSize1, ( unsigned portBASE_TYPE ) sizeof( unsigned short ) );

	/* The consumer is created first so gets a block time as described above. */
	pxQueueParameters1->xBlockTime = xBlockTime;

	/* Pass in the variable that this task is going to increment so we can check it
	is still running. */
	pxQueueParameters1->psCheckVariable = &( sBlockingConsumerCount[ 0 ] );

	/* Create the structure used to pass parameters to the producer task. */
	pxQueueParameters2 = &allBQparams[1];

	/* Pass the queue to this task also, using the parameter structure. */
	pxQueueParameters2->xQueue = pxQueueParameters1->xQueue;

	/* The producer is not going to block - as soon as it posts the consumer will
	wake and remove the item so the producer should always have room to post. */
	pxQueueParameters2->xBlockTime = xDontBlock;

	/* Pass in the variable that this task is going to increment so we can check
	it is still running. */
	pxQueueParameters2->psCheckVariable = &( sBlockingProducerCount[ 0 ] );


	/* Note the producer has a lower priority than the consumer when the tasks are
	spawned. */
	xTaskCreate(
		vBlockingQueueConsumer,
		"QConsB1",
		blckqSTACK_SIZE,
		( void * ) 0,
		uxPriority,
		&blockQTasks[0]
	);
	xTaskCreate(
		vBlockingQueueProducer,
		"QProdB2",
		blckqSTACK_SIZE,
		( void * ) 1,
		tskIDLE_PRIORITY,
		&blockQTasks[1]
	);


	/* Create the second two tasks as described at the top of the file.   This uses
	the same mechanism but reverses the task priorities. */

	pxQueueParameters3 = &allBQparams[2];
	pxQueueParameters3->xQueue = xQueueCreate(
			uxQueueSize1, ( unsigned portBASE_TYPE ) sizeof( unsigned short ) );
	pxQueueParameters3->xBlockTime = xDontBlock;
	pxQueueParameters3->psCheckVariable = &( sBlockingProducerCount[ 1 ] );

	pxQueueParameters4 = &allBQparams[3];
	pxQueueParameters4->xQueue = pxQueueParameters3->xQueue;
	pxQueueParameters4->xBlockTime = xBlockTime;
	pxQueueParameters4->psCheckVariable = &( sBlockingConsumerCount[ 1 ] );

	xTaskCreate(
		vBlockingQueueProducer,
		"QProdB3",
		blckqSTACK_SIZE,
		( void * ) 2,
		tskIDLE_PRIORITY,
		&blockQTasks[2]
	);
	xTaskCreate(
		vBlockingQueueConsumer,
		"QConsB4",
		blckqSTACK_SIZE,
		( void * ) 3,
		uxPriority,
		&blockQTasks[3]
	);


	/* Create the last two tasks as described above.  The mechanism is again just
	the same.  This time both parameter structures are given a block time. */
	pxQueueParameters5 = &allBQparams[4];
	pxQueueParameters5->xQueue = xQueueCreate(
			uxQueueSize5, ( unsigned portBASE_TYPE ) sizeof( unsigned short ) );
	pxQueueParameters5->xBlockTime = xBlockTime;
	pxQueueParameters5->psCheckVariable = &( sBlockingProducerCount[ 2 ] );

	pxQueueParameters6 = &allBQparams[5];
	pxQueueParameters6->xQueue = pxQueueParameters5->xQueue;
	pxQueueParameters6->xBlockTime = xBlockTime;
	pxQueueParameters6->psCheckVariable = &( sBlockingConsumerCount[ 2 ] );

	xTaskCreate(
		vBlockingQueueProducer,
		"QProdB5",
		blckqSTACK_SIZE,
		( void * ) 4,
		tskIDLE_PRIORITY,
		&blockQTasks[4]
	);
	xTaskCreate(
		vBlockingQueueConsumer,
		"QConsB6",
		blckqSTACK_SIZE,
		( void * ) 5,
		tskIDLE_PRIORITY,
		&blockQTasks[5]
	);
}
/*-----------------------------------------------------------*/

void vEndBlockedQueueTasks(void) {
	int i;
	for (i = 0; i < BLOCK_Q_TASK_NUM; i+=1) {
#ifdef VERBOSE_KILL_TASKS
		xil_printf("Deleting task %d (%s)\r\n", i, pcTaskGetName(blockQTasks[i]));
#endif
		vTaskDelete(blockQTasks[i]);
	}
}
/*-----------------------------------------------------------*/

static void vBlockingQueueProducer( void *pvParameters ) {

	unsigned short usValue = MAGIC_QUEUE_VALUE;
	xBlockingQueueParameters *pxQueueParameters;
	const char * const pcTaskStartMsg = "Blocking queue producer started.\r\n";
	const char * const pcTaskErrorMsg = "Could not post on blocking queue\r\n";
	short sErrorEverOccurred = pdFALSE;

	pxQueueParameters = &allBQparams[(uint32_t) pvParameters];

	/* Queue a message for printing to say the task has started. */
	vPrintDisplayMessage( &pcTaskStartMsg );

	for( ;; )
	{
		if( xQueueSendToBack( pxQueueParameters->xQueue, ( void * ) &usValue, pxQueueParameters->xBlockTime ) != pdPASS )
		{
			vPrintDisplayMessage( &pcTaskErrorMsg );
			sErrorEverOccurred = pdTRUE;
			reportError();
		}
		else
		{
			/* We have successfully posted a message, so increment the variable
			used to check we are still running. */
			if( sErrorEverOccurred == pdFALSE )
			{
				( *pxQueueParameters->psCheckVariable )++;
			}

			/* Increment the variable we are going to post next time round.  The
			consumer will expect the numbers to	follow in numerical order. */
			++usValue;
		}
	}
}
/*-----------------------------------------------------------*/

static void vBlockingQueueConsumer( void *pvParameters ) {

	unsigned short usData, usExpectedValue = MAGIC_QUEUE_VALUE;
	xBlockingQueueParameters *pxQueueParameters;
	const char * const pcTaskStartMsg = "Blocking queue consumer started.\r\n";
	const char * const pcTaskErrorMsg = "Incorrect value received on blocking queue.\r\n";
	short sErrorEverOccurred = pdFALSE;

	// Only one of the tasks with this function should check for done
	short doneCheckFlag = 0;

	/* Queue a message for printing to say the task has started. */
	vPrintDisplayMessage( &pcTaskStartMsg );

	pxQueueParameters = &allBQparams[(uint32_t) pvParameters];

	if (pxQueueParameters->psCheckVariable == &( sBlockingConsumerCount[ 0 ] ) ) {
		doneCheckFlag = 1;
	}

	for( ;; )
	{
		if( xQueueReceive( pxQueueParameters->xQueue, &usData, pxQueueParameters->xBlockTime ) == pdPASS )
		{
			if( usData != usExpectedValue )
			{
				vPrintDisplayMessage( &pcTaskErrorMsg );

				/* Catch-up. */
				usExpectedValue = usData;

				sErrorEverOccurred = pdTRUE;
				reportError();
			}
			else
			{
				/* We have successfully received a message, so increment the
				variable used to check we are still running. */
				if( sErrorEverOccurred == pdFALSE )
				{
					( *pxQueueParameters->psCheckVariable )++;

					// here is where we decide if it's done
					if (doneCheckFlag &&
							( (*pxQueueParameters->psCheckVariable) == GOAL_VAL ) ) {
						goalReached();
					}
				}

				/* Increment the value we expect to remove from the queue next time
				round. */
				++usExpectedValue;
			}
		}
	}
}
/*-----------------------------------------------------------*/


static short sLastBlockingConsumerCount[ blckqNUM_TASK_SETS ] =
		{ ( short ) 0, ( short ) 0, ( short ) 0 };
static short sLastBlockingProducerCount[ blckqNUM_TASK_SETS ] =
		{ ( short ) 0, ( short ) 0, ( short ) 0 };

/* This is called to check that all the created tasks are still running. */
portBASE_TYPE xAreBlockingQueuesStillRunning( void ) {

	portBASE_TYPE xReturn = pdPASS, xTasks;

	/* Not too worried about mutual exclusion on these variables as they are 16
	bits and we are only reading them. We also only care to see if they have
	changed or not.

	Loop through each check variable and return pdFALSE if any are found not
	to have changed since the last call. */

	for( xTasks = 0; xTasks < blckqNUM_TASK_SETS; xTasks++ )
	{
		if( sBlockingConsumerCount[ xTasks ] == sLastBlockingConsumerCount[ xTasks ]  )
		{
			xReturn = pdFALSE;
		}
		sLastBlockingConsumerCount[ xTasks ] = sBlockingConsumerCount[ xTasks ];


		if( sBlockingProducerCount[ xTasks ] == sLastBlockingProducerCount[ xTasks ]  )
		{
			xReturn = pdFALSE;
		}
		sLastBlockingProducerCount[ xTasks ] = sBlockingProducerCount[ xTasks ];
	}

	return xReturn;
}

void vBlockingQueueCountPrint( void ) {
	portBASE_TYPE xTasks;

	for (xTasks = 0; xTasks < blckqNUM_TASK_SETS; xTasks++ ) {
		xil_printf("QProdB%ld: %d\r\n", xTasks, sBlockingProducerCount[xTasks]);
		xil_printf("QConsB%ld: %d\r\n", xTasks+1, sBlockingConsumerCount[xTasks]);
	}

	return;
}

void vBlockingQueueCountClear( void ) {
	portBASE_TYPE xTasks;

	for (xTasks = 0; xTasks < blckqNUM_TASK_SETS; xTasks++ ) {
		sBlockingProducerCount[xTasks] = 0;
		sBlockingConsumerCount[xTasks] = 0;

		sLastBlockingProducerCount[xTasks] = 0;
		sLastBlockingConsumerCount[xTasks] = 0;
	}

	return;
}

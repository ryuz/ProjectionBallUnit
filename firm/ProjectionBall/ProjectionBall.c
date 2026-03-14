/*
 *  	ProjectionBall.c
 *
 *  Copyright (c) 2023
 *  K.Watanabe, Crescentt
 *  Released under the MIT license
 *  http://opensource.org/licenses/mit-license.php
 *
 */

#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include "ProjectionBall.h"
#include "rtc_rv8803.h"
#include "rtc_sd30XX.h"
#include "encoder_ma732.h"
#include "motor_ctrl.h"
#include "path_ctrl.h"
#include "flash_ctrl.h"
#include "console.h"


static semaphore_t 	sem;
static volatile bool 		CtrlEventFlg = false;
static volatile bool 		PathEventFlg = false;
struct repeating_timer control_timer;
struct repeating_timer path_timer;
alarm_pool_t* 		core0Alarm;
alarm_pool_t* 		core1Alarm;
volatile uint32_t	loop_max_us = 0;

static uint32_t ctrl_overrun_count = 0;

static volatile uint32_t path_timer_cnt = 0;
#define PATH_TIMER_PERIOD 4000  // 80us * 4000 = 320ms

bool control_timer_callback(struct repeating_timer *t)
{    
	if(CtrlEventFlg == false)
		CtrlEventFlg = true;
	else
	{
		ctrl_overrun_count++;
		// Debug: count only, don't stop — measure actual timing first
	}

	// Software path timer (driven from Core1 timer)
	path_timer_cnt++;
	if(path_timer_cnt >= PATH_TIMER_PERIOD)
	{
		path_timer_cnt = 0;
		if(PathEventFlg == false)
			PathEventFlg = true;
	}
    return true;
}

void core1_main()
{
	// Warm up: pre-populate encoder buffers and flash cache
	uint16_t dummy;
	for(int i = 0; i < 16; i++) {
		MA732ReadAngleBit(PIN_CS1, &dummy);
		MA732ReadAngleBit(PIN_CS2, &dummy);
	}
	MotorCtrlLoop(); // first call to warm up code cache (XIP)

	core1Alarm = alarm_pool_create(1, 4);
	//irq_set_priority(0,0x00);
	alarm_pool_add_repeating_timer_us(core1Alarm, 80, control_timer_callback, NULL, &control_timer);//80us
	watchdog_enable(100, 1);// 100ms timeout for debug
	//Control Loop
	while (true)
    {
		//Control Event
		if( CtrlEventFlg == true )
		{
			CtrlEventFlg = false;
			uint32_t t0 = time_us_32();
			MotorCtrlLoop();
			UpdateUserButton();
			uint32_t elapsed = time_us_32() - t0;
			if(elapsed > loop_max_us) loop_max_us = elapsed;
			if( IsResetEnable()==false)
				watchdog_update();							
		}
	}
}

void core0_main()
{	

	printf("[DBG] core0_main started, waiting for PathEventFlg...\r\n");
	uint32_t idle_count = 0;
	while (true)
	{
		idle_count++;
		if(idle_count % 1000000 == 0)
			printf("[DBG] core0 polling... PathEventFlg=%d idle=%d\r\n", PathEventFlg, idle_count/1000000);
		
		if( PathEventFlg == true)
		{
			PathEventFlg = false;
			PathCtrlLoop();
			printf("[ALIVE] err0=%d err1=%d stby=%d lsr=%d init=%d errcnt=%d maxus=%d overrun=%d\r\n",
				motorControl[0].x_err, motorControl[1].x_err,
				gpio_get(PIN_STBY), gpio_get(PIN_LSR),
				InitCount, ErrCount, loop_max_us, ctrl_overrun_count);
			ConsoleGetString();
#ifdef ENABLE_DEBUG_OUTPUT
			DebugMotorCtrl();
#endif
			
		}
	
	}	
}

void ioInit()
{
	//Motor Direction Pin
	gpio_init(PIN_STBY);
	gpio_init(PIN_LSR);
	gpio_init(PIN_ERR);
	gpio_init(PIN_A1);
	gpio_init(PIN_A2);
	gpio_init(PIN_B1);
	gpio_init(PIN_B2);
	gpio_init(PIN_MODE);
	gpio_init(PIN_PATTERN);	
    gpio_set_dir(PIN_STBY, GPIO_OUT);
	gpio_set_dir(PIN_LSR, GPIO_OUT);
	gpio_set_dir(PIN_ERR, GPIO_OUT);
	gpio_set_dir(PIN_A1, GPIO_OUT);
    gpio_set_dir(PIN_A2, GPIO_OUT);
    gpio_set_dir(PIN_B1, GPIO_OUT);
    gpio_set_dir(PIN_B2, GPIO_OUT);
	gpio_set_dir(PIN_MODE, GPIO_IN);
    gpio_set_dir(PIN_PATTERN, GPIO_IN);
	gpio_put(PIN_STBY, 0);
	gpio_put(PIN_LSR, 0);
	gpio_put(PIN_ERR, 1);//Check LED 
	gpio_put(PIN_A1, 0);
	gpio_put(PIN_A2, 0);
	gpio_put(PIN_B1, 0);
	gpio_put(PIN_B2, 0);
    gpio_pull_up(PIN_MODE);
    gpio_pull_up(PIN_PATTERN);
	
	// PWM initialisation
	gpio_set_function(PIN_PWM_A, GPIO_FUNC_PWM);
    gpio_set_function(PIN_PWM_B, GPIO_FUNC_PWM);


    // SPI initialisation
    spi_init(SPI_PORT, 8000*1000);//10MHz
#ifdef USE_MA732
	spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
#endif
#ifdef USE_AS5048A
	spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);
#endif
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS1,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    

    // Chip select initialise
    gpio_init(PIN_CS1);
	gpio_init(PIN_CS2);
	gpio_set_dir(PIN_CS1, GPIO_OUT);
	gpio_set_dir(PIN_CS2, GPIO_OUT);
    gpio_put(PIN_CS1, 1);
    gpio_put(PIN_CS2, 1);
	sleep_ms(5);
    gpio_put(PIN_ERR, 0);

    // I2C Initialisation at 100Khz
    i2c_init(i2c0, 200*1000);    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

	//UART
    uart_init(UART_ID, 2400); 
    gpio_set_function(PIN_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_RX, GPIO_FUNC_UART);
    uart_set_baudrate(UART_ID, BAUD_RATE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
    uart_set_fifo_enabled(UART_ID, false);    
    irq_set_exclusive_handler(UART0_IRQ, OnUartRx);
    irq_set_enabled(UART0_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);

	//RTC Init
	rtc_init();
  

}

int main()
{
    	
	stdio_init_all();

	// Debug: USB CDC接続待ち (ERR LEDが点滅する間にCOMポートを開く)
	gpio_init(PIN_ERR);
	gpio_set_dir(PIN_ERR, GPIO_OUT);
	while (!stdio_usb_connected()) {
		gpio_put(PIN_ERR, 1);
		sleep_ms(250);
		gpio_put(PIN_ERR, 0);
		sleep_ms(250);
	}
	sleep_ms(500);
	printf("=== USB Connected ===\r\n");

	sem_init(&sem, 1, 1);	
	ioInit();
	printf("[DBG] ioInit done\r\n");

#ifdef ENABLE_FLASH_TEST
	TestFlashReadWrite();
#endif

	if(watchdog_caused_reboot())
	{
		printf("[DBG] *** Watchdog caused reboot! ***\r\n");
		sleep_ms(1000);			
	}

	printf("[DBG] MotorCtrlInit start\r\n");
	MotorCtrlInit();
	printf("[DBG] MotorCtrlInit done\r\n");

#ifdef ENABLE_ENCODER_CHECK_MODE
	uint16_t encVal0, encVal1;
	while(1)
	{
		sleep_ms(300);
		MA732ReadAngleBit(PIN_CS1, &encVal0);
		MA732ReadAngleBit(PIN_CS2, &encVal1);
		printf("Val0: %d, Val1:%d\n",encVal0>>2,encVal1>>2);		
	}
#endif

	sleep_ms(500);
    printf("\r\n\r\n ProjectionBall Unit"VER_STR" \r\n");
	printf("[Build: "__DATE__"]\r\n");

#ifdef USE_RTC_RV8803
	if( IsRtcBOR() == true )
		printf("RTC was Cleared. Check Battery & Set Datetime!\r\n");
#endif
	GetDateTime();

	//Update HW RTC
	printf("[DBG] UpdateHwRtc start\r\n");
	UpdateHwRtc();
	printf("[DBG] RestoreUserData start\r\n");
	RestoreUserData();
	printf("[DBG] RestoreUserData done\r\n");
	ClearBuffer();
	sleep_ms(500);	

	printf("[DBG] Launching Core1...\r\n");
	multicore_launch_core1(core1_main);		
	printf("[DBG] Core1 launched, entering core0_main\r\n");
	core0_main();

    return 0;
}

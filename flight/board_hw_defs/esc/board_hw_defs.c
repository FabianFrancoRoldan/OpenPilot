#include <pios_config.h>

#if defined(PIOS_INCLUDE_LED)

#include <pios_led_priv.h>
static const struct pios_led pios_leds[] = {
	[PIOS_LED_ALARM] = {
		.pin = {
			.gpio = GPIOB,
			.init = {
				.GPIO_Pin   = GPIO_Pin_12,
				.GPIO_Mode  = GPIO_Mode_Out_PP,
				.GPIO_Speed = GPIO_Speed_50MHz,
			},
		},
	},
	[PIOS_LED_HEARTBEAT] = {
		.pin = {
			.gpio = GPIOB,
			.init = {
				.GPIO_Pin   = GPIO_Pin_13,
				.GPIO_Mode  = GPIO_Mode_Out_PP,
				.GPIO_Speed = GPIO_Speed_50MHz,
			},
		},
	},
};

const struct pios_led_cfg pios_led_cfg = {
	.leds     = pios_leds,
	.num_leds = NELEMENTS(pios_leds),
};

#endif	/* PIOS_INCLUDE_LED */

#include "pios_rcvr_priv.h"
uint32_t pios_rcvr_group_map[1];

#define ESC_DEFAULT_PWM_RATE 40000

#include "pios_esc_priv.h"
const struct pios_esc_cfg pios_esc_cfg = {
	.tim_base_init = {
		// Note not the same prescalar as servo
		// Fastest clock possible 
		.TIM_Prescaler = 0,
		.TIM_ClockDivision = TIM_CKD_DIV1,
		.TIM_CounterMode = TIM_CounterMode_CenterAligned1,
		.TIM_Period = ((PIOS_MASTER_CLOCK / ESC_DEFAULT_PWM_RATE) - 1),
		.TIM_RepetitionCounter = 0x0000,
	},
	.tim_oc_init = {
		.TIM_OCMode = TIM_OCMode_PWM1,
		.TIM_OutputState = TIM_OutputState_Enable,
		.TIM_OutputNState = TIM_OutputNState_Disable,
		.TIM_Pulse = 0,
		.TIM_OCPolarity = TIM_OCPolarity_High,
		.TIM_OCNPolarity = TIM_OCPolarity_High,
		.TIM_OCIdleState = TIM_OCIdleState_Reset,
		.TIM_OCNIdleState = TIM_OCNIdleState_Reset,
	},
	.gpio_init = {
		.GPIO_Mode = GPIO_Mode_AF_PP,
		.GPIO_Speed = GPIO_Speed_2MHz,
	},
	.phase_a_minus = { /* needs to remap to alternative function */
		.timer = TIM3,
		.port = GPIOB,
		.channel = TIM_Channel_3,
		.pin = GPIO_Pin_0,
	},
	.phase_a_plus = { /* needs to remap to alternative function */
		.timer = TIM3,
		.port = GPIOB,
		.channel = TIM_Channel_4,
		.pin = GPIO_Pin_1,
	},
	.phase_b_minus = {
		.timer = TIM2,
		.port = GPIOA,
		.channel = TIM_Channel_4,
		.pin = GPIO_Pin_3,
	},
	.phase_b_plus = {
		.timer = TIM2,
		.port = GPIOA,
		.channel = TIM_Channel_3,
		.pin = GPIO_Pin_2,
	},
	.phase_c_minus = { 
		.timer = TIM2,
		.port = GPIOA,
		.channel = TIM_Channel_1,
		.pin = GPIO_Pin_0
	},
	.phase_c_plus = { 
		.timer = TIM2,
		.port = GPIOA,
		.channel = TIM_Channel_2,
		.pin = GPIO_Pin_1,
	},
	.remap = 0,
};

/*
 * PWM Inputs
 */

//Using TIM4 CH3 for PWM input and TIM4 for delay.  Settings are
//compatiblefor both

static const TIM_TimeBaseInitTypeDef tim_4_time_base = {
	.TIM_Prescaler = (PIOS_MASTER_CLOCK / 1000000) - 1,
	.TIM_ClockDivision = TIM_CKD_DIV1,
	.TIM_CounterMode = TIM_CounterMode_Up,
	.TIM_Period = 0xFFFF,
	.TIM_RepetitionCounter = 0x0000,
};

static const struct pios_tim_clock_cfg tim_4_cfg = {
	.timer = TIM4,
	.time_base_init = &tim_4_time_base,
	.irq = {
		.init = {
			.NVIC_IRQChannel                   = TIM4_IRQn,
			.NVIC_IRQChannelPreemptionPriority = PIOS_IRQ_PRIO_HIGHEST,
			.NVIC_IRQChannelSubPriority        = 0,
			.NVIC_IRQChannelCmd                = ENABLE,
		},
	},
};
#if defined(PIOS_INCLUDE_PWM)
#include <pios_pwm_priv.h>
#include <pios_tim_priv.h>

static const struct pios_tim_channel pios_tim_rcvrport_all_channels[] = {
	{
		.timer = TIM4,
		.timer_chan = TIM_Channel_3,
		.pin = {
			.gpio = GPIOB,
			.init = {
				.GPIO_Pin   = GPIO_Pin_8,
				.GPIO_Mode  = GPIO_Mode_IPD,
				.GPIO_Speed = GPIO_Speed_2MHz,
			},
		},
	},
};

const struct pios_pwm_cfg pios_pwm_cfg = {
	.tim_ic_init = {
		.TIM_ICPolarity = TIM_ICPolarity_Rising,
		.TIM_ICSelection = TIM_ICSelection_DirectTI,
		.TIM_ICPrescaler = TIM_ICPSC_DIV1,
		.TIM_ICFilter = 0x0,
	},
	.channels = pios_tim_rcvrport_all_channels,
	.num_channels = NELEMENTS(pios_tim_rcvrport_all_channels),
};
#endif


#if defined(PIOS_INCLUDE_ADC)
/*
 * ADC system
 */
#include "pios_adc_priv.h"
// Remap the ADC DMA handler to this one
const struct pios_adc_cfg pios_adc_cfg = {
	.dma = {
		.ahb_clk  = RCC_AHBPeriph_DMA1,
		.irq = {
			.flags   = (DMA1_FLAG_TC1 | DMA1_FLAG_TE1 | DMA1_FLAG_HT1 | DMA1_FLAG_GL1),
			.init    = {
				.NVIC_IRQChannel                   = DMA1_Channel1_IRQn,
				.NVIC_IRQChannelPreemptionPriority = PIOS_IRQ_PRIO_HIGH,
				.NVIC_IRQChannelSubPriority        = 0,
				.NVIC_IRQChannelCmd                = ENABLE,
			},
		},
		.rx = {
			.channel = DMA1_Channel1,
			.init    = {
				.DMA_PeripheralBaseAddr = (uint32_t) & ADC1->DR,
				.DMA_DIR                = DMA_DIR_PeripheralSRC,
				.DMA_PeripheralInc      = DMA_PeripheralInc_Disable,
				.DMA_MemoryInc          = DMA_MemoryInc_Enable,
				.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word,
				.DMA_MemoryDataSize     = DMA_MemoryDataSize_Word,
				.DMA_Mode               = DMA_Mode_Circular,
				.DMA_Priority           = DMA_Priority_High,
				.DMA_M2M                = DMA_M2M_Disable,
			},
		}
	},
	.half_flag = DMA1_IT_HT1,
	.full_flag = DMA1_IT_TC1,
	.compute_downsample = false
};

struct pios_adc_dev pios_adc_devs[] = {
	{
		.cfg = &pios_adc_cfg,
		.callback_function = NULL,
	},
};

uint8_t pios_adc_num_devices = NELEMENTS(pios_adc_devs);
#endif

#if defined(PIOS_INCLUDE_USART)
#include <pios_usart_priv.h>

/*
 * DEBUG USART
 */
const struct pios_usart_cfg pios_usart_debug_cfg = {
	.regs = USART1,
	.init = {
		 .USART_BaudRate = 115200, //230400,
		 .USART_WordLength = USART_WordLength_8b,
		 .USART_Parity = USART_Parity_No,
		 .USART_StopBits = USART_StopBits_1,
		 .USART_HardwareFlowControl = USART_HardwareFlowControl_None,
		 .USART_Mode = USART_Mode_Rx | USART_Mode_Tx,
		 },
	.irq = {
		.init = {
			 .NVIC_IRQChannel = USART1_IRQn,
			 .NVIC_IRQChannelPreemptionPriority = PIOS_IRQ_PRIO_HIGHEST,
			 .NVIC_IRQChannelSubPriority = 0,
			 .NVIC_IRQChannelCmd = ENABLE,
			 },
		},
	.rx = {
	       .gpio = GPIOA,
	       .init = {
			.GPIO_Pin = GPIO_Pin_10,
			.GPIO_Speed = GPIO_Speed_2MHz,
			.GPIO_Mode = GPIO_Mode_IPU,
			},
	       },
	.tx = {
	       .gpio = GPIOA,
	       .init = {
			.GPIO_Pin = GPIO_Pin_9,
			.GPIO_Speed = GPIO_Speed_2MHz,
			.GPIO_Mode = GPIO_Mode_AF_PP,
			},
	       },
};

#define UART_DEBUG_TX_LEN 64
uint8_t uart_debug_tx_buffer[UART_DEBUG_TX_LEN];
#define UART_DEBUG_RX_LEN 64
uint8_t uart_debug_rx_buffer[UART_DEBUG_RX_LEN];
static uint32_t pios_usart_debug_id;

#endif /* PIOS_INCLUDE_USART */

#if defined(PIOS_INCLUDE_SOFTUSART)

#include "pios_softusart_priv.h"

static const struct pios_softusart_cfg pios_softusart_cfg = {
	.half_duplex = true,
	.tim_ic_init = {
		.TIM_ICPolarity = TIM_ICPolarity_Rising,
		.TIM_ICSelection = TIM_ICSelection_DirectTI,
		.TIM_ICPrescaler = 0,
		.TIM_ICFilter = 0x0,
	},
	.rx = {
		.timer = TIM4,
		.timer_chan = TIM_Channel_3,
		.pin = {
			.gpio = GPIOB,
			.init = {
				.GPIO_Pin   = GPIO_Pin_8,
				.GPIO_Mode  = GPIO_Mode_IPU,
				.GPIO_Speed = GPIO_Speed_2MHz,
			},
		},
	},
	.tx = {
		.timer = TIM4,
		.timer_chan = TIM_Channel_3,
		.pin = {
			.gpio = GPIOB,
			.init = {
				.GPIO_Pin   = GPIO_Pin_8,
				.GPIO_Mode  = GPIO_Mode_Out_OD,
				.GPIO_Speed = GPIO_Speed_2MHz,
			},
		},
	},
};

#define UART_SOFTUSART_TX_LEN 64
uint8_t uart_softusart_tx_buffer[UART_SOFTUSART_TX_LEN];
#define UART_SOFTUSART_RX_LEN 64
uint8_t uart_softusart_rx_buffer[UART_SOFTUSART_RX_LEN];

#endif /* PIOS_INCLUDE_SOFTUSART */

#if defined(PIOS_INCLUDE_COM)

#include <pios_com_priv.h>

#endif /* PIOS_INCLUDE_COM */

#if defined(PIOS_INCLUDE_I2C)

#include <pios_i2c_priv.h>

/*
 * I2C Adapters
 */

void PIOS_I2C_main_adapter_ev_irq_handler(void);
void PIOS_I2C_main_adapter_er_irq_handler(void);
void I2C1_EV_IRQHandler()
    __attribute__ ((alias("PIOS_I2C_main_adapter_ev_irq_handler")));
void I2C1_ER_IRQHandler()
    __attribute__ ((alias("PIOS_I2C_main_adapter_er_irq_handler")));

const struct pios_i2c_adapter_cfg pios_i2c_main_adapter_cfg = {
	.regs = I2C1,
	.init = {
		 .I2C_Mode = I2C_Mode_I2C,
		 .I2C_OwnAddress1 = 0,
		 .I2C_Ack = I2C_Ack_Enable,
		 .I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit,
		 .I2C_DutyCycle = I2C_DutyCycle_2,
		 .I2C_ClockSpeed = 200000,	/* bits/s */
		 },
	.transfer_timeout_ms = 50,
	.scl = {
		.gpio = GPIOB,
		.init = {
			 .GPIO_Pin = GPIO_Pin_6,
			 .GPIO_Speed = GPIO_Speed_10MHz,
			 .GPIO_Mode = GPIO_Mode_AF_OD,
			 },
		},
	.sda = {
		.gpio = GPIOB,
		.init = {
			 .GPIO_Pin = GPIO_Pin_7,
			 .GPIO_Speed = GPIO_Speed_10MHz,
			 .GPIO_Mode = GPIO_Mode_AF_OD,
			 },
		},
	.event = {
		  .flags = 0,	/* FIXME: check this */
		  .init = {
			   .NVIC_IRQChannel = I2C1_EV_IRQn,
			   .NVIC_IRQChannelPreemptionPriority = PIOS_IRQ_PRIO_HIGH,
			   .NVIC_IRQChannelSubPriority = 0,
			   .NVIC_IRQChannelCmd = ENABLE,
			   },
		  },
	.error = {
		  .flags = 0,	/* FIXME: check this */
		  .init = {
			   .NVIC_IRQChannel = I2C1_ER_IRQn,
			   .NVIC_IRQChannelPreemptionPriority = PIOS_IRQ_PRIO_HIGH,
			   .NVIC_IRQChannelSubPriority = 0,
			   .NVIC_IRQChannelCmd = ENABLE,
			   },
		  },
};

uint32_t pios_i2c_main_adapter_id;
void PIOS_I2C_main_adapter_ev_irq_handler(void)
{
	/* Call into the generic code to handle the IRQ for this specific device */
	PIOS_I2C_EV_IRQ_Handler(pios_i2c_main_adapter_id);
}

void PIOS_I2C_main_adapter_er_irq_handler(void)
{
	/* Call into the generic code to handle the IRQ for this specific device */
	PIOS_I2C_ER_IRQ_Handler(pios_i2c_main_adapter_id);
}

#endif /* PIOS_INCLUDE_I2C */


extern const struct pios_com_driver pios_usart_com_driver;

uint32_t pios_com_debug_id;
uint32_t pios_com_softusart_id;
uint8_t adc_fifo_buf[sizeof(float) * 6 * 4] __attribute__ ((aligned(4)));    // align to 32-bit to try and provide speed improvement

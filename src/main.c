/**
 * @file        main.c
 * 
 * @brief       Audio DK HW_CODEC test using I2S loop and tone/noise generators.
 */

#include <zephyr/kernel.h>
#include <nrf.h>
#include <nrfx_clock.h>

#include "cs47l63_comm.h"



////////////////////////////////////////////////////////////////////////////////
// NRFX_CLOCKS

#define HFCLKAUDIO_12_288_MHZ 0x9BA6


/**
 * @brief       Initialize the high-frequency clocks and wait for each to start.
 * 
 * @details     HFCLK =         128,000,000 Hz
 *              HFCLKAUDIO =     12,288,000 Hz
 */
static int nrfadk_hfclocks_init(void)
{
	nrfx_err_t err;


	// HFCLK
	err = nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);
	if (err != NRFX_SUCCESS) return (err - NRFX_ERROR_BASE_NUM);

	nrfx_clock_start(NRF_CLOCK_DOMAIN_HFCLK);
	while (!nrfx_clock_is_running(NRF_CLOCK_DOMAIN_HFCLK, NULL)) k_msleep(1);


	// HFCLKAUDIO
	nrfx_clock_hfclkaudio_config_set(HFCLKAUDIO_12_288_MHZ);

	nrfx_clock_start(NRF_CLOCK_DOMAIN_HFCLKAUDIO);
	while (!nrfx_clock_is_running(NRF_CLOCK_DOMAIN_HFCLKAUDIO, NULL)) k_msleep(1);


	return 0;
}



////////////////////////////////////////////////////////////////////////////////
// NRF_I2S

#define MCKFREQ_6_144_MHZ 0x66666000


/** I2S TX buffer. */
static int16_t sine750_48k_16b_1c[64] =
{
	  3211,   6392,   9511,  12539,  15446,  18204,  20787,  23169,
	 25329,  27244,  28897,  30272,  31356,  32137,  32609,  32767,
	 32609,  32137,  31356,  30272,  28897,  27244,  25329,  23169,
	 20787,  18204,  15446,  12539,   9511,   6392,   3211,      0,
	 -3212,  -6393,  -9512, -12540, -15447, -18205, -20788, -23170,
	-25330, -27245, -28898, -30273, -31357, -32138, -32610, -32767,
	-32610, -32138, -31357, -30273, -28898, -27245, -25330, -23170,
	-20788, -18205, -15447, -12540,  -9512,  -6393,  -3212,     -1,
};


/**
 * @brief       Initialize and start the I2S peripheral using NRF registers.
 * 
 * @details     I2S master, 48kHz 16bit, Left mono, TX only.
 */
static int nrfadk_i2s_reg_init(void)
{
	// Configure and enable
	NRF_I2S0->CONFIG.CLKCONFIG =    I2S_CONFIG_CLKCONFIG_CLKSRC_ACLK;
	NRF_I2S0->CONFIG.MCKFREQ =      MCKFREQ_6_144_MHZ;
	NRF_I2S0->CONFIG.RATIO =        I2S_CONFIG_RATIO_RATIO_128X;
	NRF_I2S0->CONFIG.CHANNELS =     I2S_CONFIG_CHANNELS_CHANNELS_Left;
	NRF_I2S0->CONFIG.TXEN =         I2S_CONFIG_TXEN_TXEN_Enabled;

	NRF_I2S0->ENABLE =              I2S_ENABLE_ENABLE_Enabled;


	// Start TX buffer
	NRF_I2S0->TXD.PTR =             (uint32_t)&sine750_48k_16b_1c[0];
	NRF_I2S0->RXTXD.MAXCNT =        (sizeof(sine750_48k_16b_1c)) / (sizeof(uint32_t));

	NRF_I2S0->TASKS_START =         I2S_TASKS_START_TASKS_START_Trigger;


	return 0;
}



////////////////////////////////////////////////////////////////////////////////
// HW_CODEC

/** CS47L63 driver state handle. */
static cs47l63_t cs47l63_driver;


/** CS47L63 subsystems configuration. */
static const uint32_t cs47l63_cfg[][2] =
{

	// Audio Serial Port 1 (I2S slave, 48kHz 16bit, Left mono, RX only)

	{ CS47L63_ASP1_CONTROL2,
		(0x10  << CS47L63_ASP1_RX_WIDTH_SHIFT) |        // 16bit
		(0x10  << CS47L63_ASP1_TX_WIDTH_SHIFT) |        // 16bit
		(0b010 << CS47L63_ASP1_FMT_SHIFT)               // I2S
	},
	{ CS47L63_ASP1_CONTROL3,
		(0b00 << CS47L63_ASP1_DOUT_HIZ_CTRL_SHIFT)      // Always 0
	},
	{ CS47L63_ASP1_ENABLES1,
		(0 << CS47L63_ASP1_RX2_EN_SHIFT) |              // Disabled
		(1 << CS47L63_ASP1_RX1_EN_SHIFT) |              // Enabled
		(0 << CS47L63_ASP1_TX2_EN_SHIFT) |              // Disabled
		(0 << CS47L63_ASP1_TX1_EN_SHIFT)                // Disabled
	},


	// Noise Generator (increased GAIN from -114dB to 0dB)

	{ CS47L63_COMFORT_NOISE_GENERATOR,
		(0    << CS47L63_NOISE_GEN_EN_SHIFT) |          // Disabled
		(0x13 << CS47L63_NOISE_GEN_GAIN_SHIFT)          // 0dB
	},


	// Output 1 Left (reduced MIX_VOLs to prevent clipping summed signals)

	{ CS47L63_OUT1L_INPUT1,
		(0x2E  << CS47L63_OUT1LMIX_VOL1_SHIFT) |        // -18dB
		(0x020 << CS47L63_OUT1L_SRC1_SHIFT)             // ASP1_RX1
	},
	{ CS47L63_OUT1L_INPUT2,
		(0x40  << CS47L63_OUT1LMIX_VOL2_SHIFT) |        // 0dB
		(0x000 << CS47L63_OUT1L_SRC2_SHIFT)             // NO_INPUT
	},
	{ CS47L63_OUT1L_INPUT3,
		(0x2B  << CS47L63_OUT1LMIX_VOL3_SHIFT) |        // -21dB
		(0x004 << CS47L63_OUT1L_SRC3_SHIFT)             // TONE1_GEN
	},
	{ CS47L63_OUT1L_INPUT4,
		(0x28  << CS47L63_OUT1LMIX_VOL4_SHIFT) |        // -24dB
		(0x00C << CS47L63_OUT1L_SRC4_SHIFT)             // NOISE_GEN
	},
	{ CS47L63_OUTPUT_ENABLE_1,
		(1 << CS47L63_OUT1L_EN_SHIFT)                   // Enabled
	},
};


/**
 * @brief       Write a configuration array to multiple CS47L63 registers.
 * 
 * @param[in]   config: Array of address/data pairs.
 * @param[in]   length: Number of registers to write.
 * 
 * @retval      `CS47L63_STATUS_OK`     The operation was successful.
 * @retval      `CS47L63_STATUS_FAIL`   Writing to the control port failed.
 */
static int nrfadk_hwcodec_config(const uint32_t config[][2], uint32_t length)
{
	int ret;
	uint32_t addr;
	uint32_t data;

	for (int i = 0; i < length; i++)
	{
		addr = config[i][0];
		data = config[i][1];

		ret = cs47l63_write_reg(&cs47l63_driver, addr, data);
		if (ret) return ret;
	}

	return CS47L63_STATUS_OK;
}


/**
 * @brief       Initialize the CS47L63, start clocks, and configure subsystems.
 * 
 * @details     MCLK1 =   6,144,000 Hz  (I2S MCK = CONFIG.MCKFREQ)
 *              FSYNC =      48,000 Hz  (I2S LRCK = MCK / CONFIG.RATIO)
 *              BCLK =    1,536,000 Hz  (I2S SCK = LRCK * CONFIG.SWIDTH * 2)
 *              FLL1 =   49,152,000 Hz  (MCLK1 * 8)
 *              SYSCLK = 98,304,000 Hz  (FLL1 * 2)
 * 
 * @retval      `CS47L63_STATUS_OK`     The operation was successful.
 * @retval      `CS47L63_STATUS_FAIL`   Initializing the CS47L63 failed.
 * 
 * @note        I2S MCK must already be running before calling this function.
 */
static int nrfadk_hwcodec_init(void)
{
	int ret = CS47L63_STATUS_OK;


	// Initialize driver
	ret += cs47l63_comm_init(&cs47l63_driver);


	// Start FLL1 and SYSCLK
	ret += cs47l63_fll_config(&cs47l63_driver, CS47L63_FLL1,
	                          CS47L63_FLL_SRC_MCLK1, 6144000, 49152000);

	ret += cs47l63_fll_enable(&cs47l63_driver, CS47L63_FLL1);

	ret += cs47l63_fll_wait_for_lock(&cs47l63_driver, CS47L63_FLL1);

	ret += cs47l63_update_reg(&cs47l63_driver, CS47L63_SYSTEM_CLOCK1,
	                          CS47L63_SYSCLK_EN_MASK, CS47L63_SYSCLK_EN);


	// Configure subsystems
	ret += nrfadk_hwcodec_config(cs47l63_cfg, ARRAY_SIZE(cs47l63_cfg));


	return ret;
}



////////////////////////////////////////////////////////////////////////////////
// MAIN

int main(void)
{
	// Initialize Audio DK

	if (nrfadk_hfclocks_init() ||
	    nrfadk_i2s_reg_init()  ||
	    nrfadk_hwcodec_init())
	{
		printk("\nError initializing Audio DK\n");
		return -1;
	}

	printk("\nAudio DK initialized\n");
	k_msleep(1250);



	// Unmute OUT1L I2S playback and enable NOISE/TONE1 generators

	cs47l63_update_reg(&cs47l63_driver, CS47L63_OUT1L_VOLUME_1,
	                   CS47L63_OUT_VU_MASK | CS47L63_OUT1L_MUTE_MASK,
	                   CS47L63_OUT_VU | 0);

	printk("\nOUT1L unmuted\n");
	k_msleep(3000);


	cs47l63_update_reg(&cs47l63_driver, CS47L63_COMFORT_NOISE_GENERATOR,
	                   CS47L63_NOISE_GEN_EN_MASK, CS47L63_NOISE_GEN_EN);

	printk("NOISE enabled\n");
	k_msleep(3000);


	cs47l63_update_reg(&cs47l63_driver, CS47L63_TONE_GENERATOR1,
	                   CS47L63_TONE1_EN_MASK, CS47L63_TONE1_EN);

	printk("TONE1 enabled\n");
	k_msleep(3000);



	// Disable NOISE/TONE1 generators and mute OUT1L I2S playback

	cs47l63_update_reg(&cs47l63_driver, CS47L63_COMFORT_NOISE_GENERATOR,
	                   CS47L63_NOISE_GEN_EN_MASK, 0);

	printk("\nNOISE disabled\n");
	k_msleep(3000);


	cs47l63_update_reg(&cs47l63_driver, CS47L63_TONE_GENERATOR1,
	                   CS47L63_TONE1_EN_MASK, 0);

	printk("TONE1 disabled\n");
	k_msleep(3000);


	cs47l63_update_reg(&cs47l63_driver, CS47L63_OUT1L_VOLUME_1,
	                   CS47L63_OUT_VU_MASK | CS47L63_OUT1L_MUTE_MASK,
	                   CS47L63_OUT_VU | CS47L63_OUT1L_MUTE);

	printk("OUT1L muted\n");
	k_msleep(1250);



	// Shutdown Audio DK (reverse order initialized)

	cs47l63_update_reg(&cs47l63_driver, CS47L63_OUTPUT_ENABLE_1,
	                   CS47L63_OUT1L_EN_MASK, 0);
	printk("\nOUT1L disabled\n");
	k_msleep(250);

	cs47l63_update_reg(&cs47l63_driver, CS47L63_SYSTEM_CLOCK1,
	                   CS47L63_SYSCLK_EN_MASK, 0);
	printk("SYSCLK disabled\n");
	k_msleep(250);

	cs47l63_fll_disable(&cs47l63_driver, CS47L63_FLL1);
	printk("FLL1 disabled\n");
	k_msleep(250);

	NRF_I2S0->TASKS_STOP =  I2S_TASKS_STOP_TASKS_STOP_Trigger;
	NRF_I2S0->ENABLE =      I2S_ENABLE_ENABLE_Disabled;
	printk("I2S disabled\n");
	k_msleep(250);

	nrfx_clock_stop(NRF_CLOCK_DOMAIN_HFCLKAUDIO);
	while (nrfx_clock_is_running(NRF_CLOCK_DOMAIN_HFCLKAUDIO, NULL)) k_msleep(1);
	printk("HFCLKAUDIO stopped\n");
	k_msleep(250);

	nrfx_clock_stop(NRF_CLOCK_DOMAIN_HFCLK);
	while (nrfx_clock_is_running(NRF_CLOCK_DOMAIN_HFCLK, NULL)) k_msleep(1);
	printk("HFCLK stopped\n");
	k_msleep(250);


	printk("\nAudio DK shutdown\n\n");
	k_msleep(100);


	return 0;
}

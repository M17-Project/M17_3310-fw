#include "tuning.h"
#include <string.h>
#include <stdio.h>
#include "rf_module.h"

extern codeplug_t codeplug;

static dev_settings_t *ds;

//AT1846S tuning ranges
uint8_t tuneFreqValid(uint32_t hz)
{
	return (hz >= 134000000U && hz <= 174000000U) ||
		   (hz >= 200000000U && hz <= 260000000U) ||
		   (hz >= 400000000U && hz <= 520000000U);
}

uint16_t tuneNumChannels(void)
{
	return (codeplug.num_items > CH_MAX_NUM) ? CH_MAX_NUM : codeplug.num_items;
}

const char *tuneChannelName(uint16_t idx)
{
	if (idx >= tuneNumChannels())
		return "";

	return codeplug.channel[idx].ch_name;
}

static void loadActive(void)
{
	if (ds->tuning_mode == TUNING_MEM && tuneNumChannels() > 0)
	{
		if (ds->ch_num >= tuneNumChannels())
			ds->ch_num = 0;

		ds->channel = codeplug.channel[ds->ch_num];
	}
	else
	{
		ds->tuning_mode = TUNING_VFO;
		ds->channel = ds->ext.vfo;
	}
}

void tuneInit(dev_settings_t *settings)
{
	ds = settings;
	loadActive();
}

void tuneApply(void)
{
	//RX always uses the 25 kHz filter (see the chBwRF() workaround in main.c)
	setPowerRF(ds->channel.rf_pwr);
	setFreqRF(ds->channel.rx_frequency, ds->freq_corr);
}

void tuneChannelEdited(void)
{
	if (ds->tuning_mode == TUNING_VFO)
		ds->ext.vfo = ds->channel;
	else if (ds->ch_num < tuneNumChannels())
		codeplug.channel[ds->ch_num] = ds->channel;
}

void tuneStep(int8_t dir)
{
	if (ds->tuning_mode == TUNING_VFO)
	{
		int32_t step = (int32_t)ds->ext.vfo_step * dir;
		uint32_t rx = ds->channel.rx_frequency + step;
		uint32_t tx = ds->channel.tx_frequency + step;

		if (!tuneFreqValid(rx) || !tuneFreqValid(tx))
			return;

		ds->channel.rx_frequency = rx;
		ds->channel.tx_frequency = tx;
		ds->ext.vfo = ds->channel;
	}
	else
	{
		uint16_t n = tuneNumChannels();

		if (n == 0)
			return;

		ds->ch_num = (dir > 0) ? (ds->ch_num + 1) % n : (ds->ch_num + n - 1) % n;
		ds->channel = codeplug.channel[ds->ch_num];
	}

	tuneApply();
}

uint8_t tuneSetMode(tuning_mode_t mode)
{
	if (mode == TUNING_MEM && tuneNumChannels() == 0)
		return 1;

	if (mode != ds->tuning_mode)
	{
		ds->tuning_mode = mode;
		loadActive();
		tuneApply();
	}

	return 0;
}

void tuneToggleMode(void)
{
	tuneSetMode((ds->tuning_mode == TUNING_VFO) ? TUNING_MEM : TUNING_VFO);
}

uint8_t tuneSelectChannel(uint16_t idx)
{
	if (idx >= tuneNumChannels())
		return 1;

	ds->tuning_mode = TUNING_MEM;
	ds->ch_num = idx;
	loadActive();
	tuneApply();

	return 0;
}

uint8_t tuneSetVfoFreq(uint32_t rx_hz)
{
	if (!tuneFreqValid(rx_hz))
		return 1;

	int32_t shift = (int32_t)(ds->ext.vfo.tx_frequency - ds->ext.vfo.rx_frequency);
	uint32_t tx = rx_hz + shift;

	ds->ext.vfo.rx_frequency = rx_hz;
	ds->ext.vfo.tx_frequency = tuneFreqValid(tx) ? tx : rx_hz;

	ds->tuning_mode = TUNING_VFO;
	loadActive();
	tuneApply();

	return 0;
}

uint8_t tuneSetVfoShift(int32_t shift_hz)
{
	uint32_t tx = ds->ext.vfo.rx_frequency + shift_hz;

	if (!tuneFreqValid(tx))
		return 1;

	ds->ext.vfo.tx_frequency = tx;

	if (ds->tuning_mode == TUNING_VFO)
		ds->channel = ds->ext.vfo;

	return 0;
}

//--- text helpers (integer math, no float printf needed) ---

//decimal number with an optional fraction -> value * 10^decimals
static uint8_t parseFixed(const char *s, uint8_t decimals, uint64_t *out)
{
	uint64_t v = 0;
	uint8_t frac = 0, dot = 0, digits = 0;

	for (; *s; s++)
	{
		if (*s >= '0' && *s <= '9')
		{
			if (dot)
			{
				if (frac >= decimals)
					continue; //ignore extra digits
				frac++;
			}
			v = v * 10 + (*s - '0');
			digits++;
			if (v > 100000000000000ULL)
				return 1;
		}
		else if ((*s == '.' || *s == ',') && !dot)
			dot = 1;
		else
			return 1;
	}

	if (!digits)
		return 1;

	while (frac++ < decimals)
		v *= 10;

	*out = v;
	return 0;
}

uint8_t parseFreq(const char *s, uint32_t *hz)
{
	uint64_t v;

	//with a decimal point: MHz
	if (strchr(s, '.') || strchr(s, ','))
	{
		if (parseFixed(s, 6, &v))
			return 1;
	}
	else
	{
		if (parseFixed(s, 0, &v))
			return 1;

		if (v < 1000U)				//MHz
			v *= 1000000U;
		else if (v < 1000000U)		//kHz
			v *= 1000U;
	}

	if (v > 0xFFFFFFFFULL)
		return 1;

	*hz = (uint32_t)v;
	return 0;
}

uint8_t parseSignedMHz(const char *s, int32_t *hz)
{
	int8_t sign = 1;
	uint64_t v;

	if (*s == '-' || *s == '+')
	{
		if (*s == '-')
			sign = -1;
		s++;
	}

	if (parseFixed(s, 6, &v) || v > 1000000000ULL)
		return 1;

	*hz = sign * (int32_t)v;
	return 0;
}

void fmtFreq(char *out, uint8_t n, uint32_t hz)
{
	snprintf(out, n, "%lu.%04lu", (unsigned long)(hz / 1000000U),
			(unsigned long)((hz % 1000000U) / 100U));
}

void fmtSignedMHz(char *out, uint8_t n, int32_t hz)
{
	uint32_t a = (hz < 0) ? -hz : hz;
	char frac[8];

	snprintf(frac, sizeof(frac), "%06lu", (unsigned long)(a % 1000000U));

	//strip trailing zeros
	int8_t i = 5;
	while (i >= 0 && frac[i] == '0')
		frac[i--] = 0;

	if (frac[0])
		snprintf(out, n, "%c%lu.%s", (hz < 0) ? '-' : '+', (unsigned long)(a / 1000000U), frac);
	else
		snprintf(out, n, "%c%lu", (hz < 0) ? '-' : '+', (unsigned long)(a / 1000000U));
}

//Menu definitions and the table of menu fields (settings, info values, actions)
//
//Every field knows how to show its current value, how to fill the editor with
//it and how to commit a new value - this keeps the menus, the editors and the
//stored settings consistent.
#include "menus.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <m17.h>
#include "display.h"
#include "keypad.h"
#include "platform.h"
#include "ringtones.h"
#include "rf_module.h"
#include "tuning.h"
#include "inbox.h"
#include "charset.h"
#include "ui.h"
#include "nvmem.h"
#include "codeplug.h"
#include "version.h"
#include "macros.h"

extern disp_dev_t disp_dev;

//--- menus ---
const disp_t displays[DISP_NUM_ITEMS] =
{
	[DISP_MAIN_MENU] =
	{
		"Main menu", 5,
		{"Messages", "Channels", "Settings", "Info", "Debug"},
		{DISP_MESSAGES, DISP_CHANNELS, DISP_SETTINGS, DISP_INFO, DISP_DEBUG},
		{EDIT_NONE},
		DISP_MAIN_SCR
	},

	[DISP_MESSAGES] =
	{
		"Messages", 3,
		{"Write", "Inbox", "Delete all"},
		{DISP_NONE, DISP_INBOX, DISP_NONE},
		{EDIT_MSG_WRITE, EDIT_INFO_INBOX, EDIT_INBOX_CLEAR},
		DISP_MAIN_MENU
	},

	[DISP_MSG_OPTIONS] =
	{
		"Message", 5,
		{"Reply", "Forward", "Details", "Sender as Dest.", "Delete"},
		{DISP_NONE},
		{EDIT_MSG_REPLY, EDIT_MSG_FORWARD, EDIT_MSG_DETAILS, EDIT_MSG_SET_DST, EDIT_MSG_DELETE},
		DISP_MSG_VIEW
	},

	[DISP_CH_OPTIONS] =
	{
		"Channel", 3,
		{"Select", "Edit", "Delete"},
		{DISP_NONE, DISP_CH_EDIT, DISP_NONE},
		{EDIT_CH_SELECT, EDIT_NONE, EDIT_CH_DELETE},
		DISP_CHANNELS
	},

	[DISP_CH_EDIT] =
	{
		"Edit channel", 6,
		{"Name", "RX freq.", "TX freq.", "Dest.", "CAN", "RF power"},
		{DISP_NONE},
		{EDIT_CH_NAME, EDIT_CH_RX, EDIT_CH_TX, EDIT_CH_DST, EDIT_CH_CAN, EDIT_CH_PWR},
		DISP_CH_OPTIONS
	},

	[DISP_SETTINGS] =
	{
		"Settings", 5,
		{"Radio", "Display", "Alerts", "Keyboard", "M17"},
		{DISP_RADIO_SETTINGS, DISP_DISPLAY_SETTINGS, DISP_ALERT_SETTINGS, DISP_KEYBOARD_SETTINGS, DISP_M17_SETTINGS},
		{EDIT_NONE},
		DISP_MAIN_MENU
	},

	[DISP_RADIO_SETTINGS] =
	{
		"Radio settings", 7,
		{"Mode", "Channel", "VFO freq.", "VFO step", "TX shift", "Offset", "RF power"},
		{DISP_NONE},
		{EDIT_TUNING_MODE, EDIT_MEM_CHANNEL, EDIT_VFO_FREQ, EDIT_VFO_STEP, EDIT_VFO_SHIFT, EDIT_RF_PPM, EDIT_RF_PWR},
		DISP_SETTINGS
	},

	[DISP_DISPLAY_SETTINGS] =
	{
		"Display", 7,
		{"Intensity", "Timeout", "Always on", "Contrast", "Splash time", "Splash 1", "Splash 2"},
		{DISP_NONE},
		{EDIT_BL_LEVEL, EDIT_BL_TIMER, EDIT_BL_ALWAYS, EDIT_CONTRAST, EDIT_SPLASH_TIME, EDIT_SPLASH_LINE1, EDIT_SPLASH_LINE2},
		DISP_SETTINGS
	},

	[DISP_ALERT_SETTINGS] =
	{
		"Alerts", 2,
		{"Ringtone", "Vibration"},
		{DISP_NONE},
		{EDIT_RINGTONE, EDIT_VIBRATION},
		DISP_SETTINGS
	},

	[DISP_KEYBOARD_SETTINGS] =
	{
		"Keyboard", 3,
		{"Multi-tap", "Repeat", "Ukrainian"},
		{DISP_NONE},
		{EDIT_KBD_TIMEOUT, EDIT_KBD_DELAY, EDIT_KBD_UKRAINIAN},
		DISP_SETTINGS
	},

	[DISP_M17_SETTINGS] =
	{
		"M17 settings", 3,
		{"Callsign", "Dest.", "CAN"},
		{DISP_NONE},
		{EDIT_M17_SRC_CALLSIGN, EDIT_M17_DST_CALLSIGN, EDIT_M17_CAN},
		DISP_SETTINGS
	},

	[DISP_INFO] =
	{
		"Info", 6,
		{"FW ver.", "libm17", "RF FW", "RF model", "Battery", "Author"},
		{DISP_NONE},
		{EDIT_INFO_FW, EDIT_INFO_LIBM17, EDIT_INFO_RF_FW, EDIT_INFO_RF_MODEL, EDIT_INFO_BATT, EDIT_INFO_AUTHOR},
		DISP_MAIN_MENU
	},

	[DISP_DEBUG] =
	{
		"Debug menu", 1,
		{"Debug test"},
		{DISP_NONE},
		{EDIT_NONE},
		DISP_MAIN_MENU
	},
};

uint8_t isStaticMenu(disp_state_t s)
{
	return (s < DISP_NUM_ITEMS && displays[s].num_items > 0) ? 1 : 0;
}

//--- helpers ---
static void put(char *out, uint8_t n, const char *s)
{
	snprintf(out, n, "%s", s);
}

static uint8_t nearestIdx(const uint32_t *vals, uint8_t num, uint32_t v)
{
	uint8_t best = 0;
	uint32_t best_d = 0xFFFFFFFFU;

	for (uint8_t i=0; i<num; i++)
	{
		uint32_t d = (vals[i] > v) ? vals[i] - v : v - vals[i];
		if (d < best_d)
		{
			best_d = d;
			best = i;
		}
	}

	return best;
}

static uint8_t noYesNum(void)
{
	return 2;
}

static void noYesName(uint8_t i, char *out, uint8_t n)
{
	put(out, n, i ? "Yes" : "No");
}

static void offOnName(uint8_t i, char *out, uint8_t n)
{
	put(out, n, i ? "On" : "Off");
}

//M17 callsign: up to 9 characters of A-Z 0-9 - / . (spec, Table A.1),
//'@' or '#' allowed as the first character of a destination
static uint8_t cleanCallsign(char *out, const char *in, uint8_t allow_special)
{
	uint8_t n = 0;
	const char *end = in + strlen(in);

	//ignore spaces around (an extra tap on the 0 key)
	while (*in == ' ')
		in++;
	while (end > in && end[-1] == ' ')
		end--;

	for (; in < end; in++)
	{
		char c = localToUpper(*in);

		if (n >= 9)
			return 1;

		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '/' || c == '.' ||
			(n == 0 && allow_special && (c == '@' || c == '#'))))
			return 1;

		out[n++] = c;
	}

	out[n] = 0;

	if (n == 0)
		return 1;

	uint64_t v;
	return (encode_callsign_value(&v, out) == 0) ? 0 : 1;
}

//--- M17 ---
static void getSrc(char *out, uint8_t n)
{
	put(out, n, dev_settings.src_callsign);
}

static uint8_t setSrc(const char *s)
{
	char c[10];

	if (cleanCallsign(c, s, 0))
		return 1;

	copyCallsign(dev_settings.src_callsign, sizeof(dev_settings.src_callsign), c);
	return 0;
}

static void getDst(char *out, uint8_t n)
{
	put(out, n, dev_settings.channel.dst);
}

static uint8_t setDst(const char *s)
{
	char c[10];

	if (cleanCallsign(c, s, 1))
		return 1;

	copyCallsign(dev_settings.channel.dst, sizeof(dev_settings.channel.dst), c);
	tuneChannelEdited();
	return 0;
}

static void getCan(char *out, uint8_t n)
{
	snprintf(out, n, "%u", dev_settings.channel.can);
}

static uint8_t setCan(const char *s)
{
	char *end;
	long v = strtol(s, &end, 10);

	if (*s == 0 || *end != 0 || v < 0 || v > 15)
		return 1;

	dev_settings.channel.can = (uint8_t)v;
	tuneChannelEdited();
	return 0;
}

//--- radio ---
static void getPpm(char *out, uint8_t n)
{
	int32_t v = (int32_t)lroundf(dev_settings.freq_corr * 10.0f);
	uint32_t a = (v < 0) ? -v : v;

	snprintf(out, n, "%c%lu.%luppm", (v < 0) ? '-' : '+', (unsigned long)(a / 10), (unsigned long)(a % 10));
}

static void getPpmEdit(char *out, uint8_t n)
{
	int32_t v = (int32_t)lroundf(dev_settings.freq_corr * 10.0f);
	uint32_t a = (v < 0) ? -v : v;

	snprintf(out, n, "%s%lu.%lu", (v < 0) ? "-" : "", (unsigned long)(a / 10), (unsigned long)(a % 10));
}

static uint8_t setPpm(const char *s)
{
	char *end;
	float v = strtof(s, &end);

	if (*s == 0 || *end != 0 || !(fabsf(v) <= 50.0f))
		return 1;

	dev_settings.freq_corr = v;
	tuneApply();
	return 0;
}

static void pwrName(uint8_t i, char *out, uint8_t n)
{
	put(out, n, i ? "High" : "Low");
}

static uint8_t pwrGet(void)
{
	return (dev_settings.channel.rf_pwr == RF_PWR_HIGH) ? 1 : 0;
}

static void pwrSet(uint8_t i)
{
	dev_settings.channel.rf_pwr = i ? RF_PWR_HIGH : RF_PWR_LOW;
	tuneChannelEdited();
	tuneApply();
}

static void getPwr(char *out, uint8_t n)
{
	pwrName(pwrGet(), out, n);
}

static void modeName(uint8_t i, char *out, uint8_t n)
{
	put(out, n, i ? "Memory" : "VFO");
}

static uint8_t modeGet(void)
{
	return (dev_settings.tuning_mode == TUNING_MEM) ? 1 : 0;
}

static void modeSet(uint8_t i)
{
	tuneSetMode(i ? TUNING_MEM : TUNING_VFO);
}

static void getMode(char *out, uint8_t n)
{
	put(out, n, modeGet() ? "Mem" : "VFO");
}

static uint8_t chanNum(void)
{
	return (tuneNumChannels() > 255) ? 255 : tuneNumChannels();
}

static void chanName(uint8_t i, char *out, uint8_t n)
{
	snprintf(out, n, "%u %s", i + 1, tuneChannelName(i));
}

static uint8_t chanGet(void)
{
	return (uint8_t)dev_settings.ch_num;
}

static void chanSet(uint8_t i)
{
	tuneSelectChannel(i);
}

static void getChan(char *out, uint8_t n)
{
	if (tuneNumChannels() == 0)
		put(out, n, "-");
	else
		put(out, n, tuneChannelName(dev_settings.ch_num));
}

static void getVfoFreq(char *out, uint8_t n)
{
	fmtFreq(out, n, dev_settings.ext.vfo.rx_frequency);
}

static uint8_t setVfoFreq(const char *s)
{
	uint32_t hz;

	if (parseFreq(s, &hz))
		return 1;

	return tuneSetVfoFreq(hz);
}

static const uint32_t step_vals[] = {5000, 6250, 10000, 12500, 25000, 100000, 1000000};
static const char *const step_names[] = {"5k", "6.25k", "10k", "12.5k", "25k", "100k", "1M"};

static uint8_t stepNum(void)
{
	return arrlen(step_vals);
}

static void stepName(uint8_t i, char *out, uint8_t n)
{
	put(out, n, step_names[i]);
}

static uint8_t stepGet(void)
{
	return nearestIdx(step_vals, arrlen(step_vals), dev_settings.ext.vfo_step);
}

static void stepSet(uint8_t i)
{
	dev_settings.ext.vfo_step = step_vals[i];
}

static void getStep(char *out, uint8_t n)
{
	stepName(stepGet(), out, n);
}

//editor: empty for simplex (nothing to delete before typing a shift)
static void getShiftEdit(char *out, uint8_t n)
{
	int32_t sh = (int32_t)(dev_settings.ext.vfo.tx_frequency - dev_settings.ext.vfo.rx_frequency);

	if (sh == 0)
		out[0] = 0;
	else
		fmtSignedMHz(out, n, sh);
}

static void getShift(char *out, uint8_t n)
{
	if (dev_settings.ext.vfo.tx_frequency == dev_settings.ext.vfo.rx_frequency)
		put(out, n, "Simplex");
	else
		fmtSignedMHz(out, n, (int32_t)(dev_settings.ext.vfo.tx_frequency - dev_settings.ext.vfo.rx_frequency));
}

static uint8_t setShift(const char *s)
{
	int32_t hz = 0;

	if (*s && parseSignedMHz(s, &hz)) //empty: simplex
		return 1;

	return tuneSetVfoShift(hz);
}

static uint8_t setMemNumber(const char *s)
{
	char *end;
	long v = strtol(s, &end, 10);

	if (*s == 0 || *end != 0 || v < 1 || v > tuneNumChannels())
		return 1;

	return tuneSelectChannel(v - 1);
}

static void getEmpty(char *out, uint8_t n)
{
	if (n)
		out[0] = 0;
}

//--- display ---
//roughly doubling the perceived brightness per step (setBacklight() applies gamma 2)
static const uint32_t bl_vals[] = {3, 6, 12, 24, 45, 80, 140, 255};

static uint8_t blNum(void)
{
	return arrlen(bl_vals);
}

static void blName(uint8_t i, char *out, uint8_t n)
{
	snprintf(out, n, "%u/%u", i + 1, (unsigned)arrlen(bl_vals));
}

static uint8_t blGet(void)
{
	return nearestIdx(bl_vals, arrlen(bl_vals), dev_settings.backlight_level);
}

static void blSet(uint8_t i)
{
	dev_settings.backlight_level = bl_vals[i];
	setBacklight(dev_settings.backlight_level);
}

static void blPreview(uint8_t i, uint8_t active)
{
	setBacklight(active ? bl_vals[i] : dev_settings.backlight_level);
}

static void getBl(char *out, uint8_t n)
{
	blName(blGet(), out, n);
}

static const uint32_t blt_vals[] = {3, 5, 10, 15, 20, 30};

static uint8_t bltNum(void)
{
	return arrlen(blt_vals);
}

static void bltName(uint8_t i, char *out, uint8_t n)
{
	snprintf(out, n, "%lus", (unsigned long)blt_vals[i]);
}

static uint8_t bltGet(void)
{
	return nearestIdx(blt_vals, arrlen(blt_vals), dev_settings.backlight_timer);
}

static void bltSet(uint8_t i)
{
	dev_settings.backlight_timer = blt_vals[i];
	setBacklightTimer(dev_settings.backlight_timer);
}

static void getBlt(char *out, uint8_t n)
{
	snprintf(out, n, "%us", dev_settings.backlight_timer);
}

static uint8_t blaGet(void)
{
	return dev_settings.backlight_always ? 1 : 0;
}

static void blaSet(uint8_t i)
{
	dev_settings.backlight_always = i;
	applyDeviceSettings();
}

static void getBla(char *out, uint8_t n)
{
	noYesName(blaGet(), out, n);
}

//the full Vop range: 0 should be blank, 127 black on a PCD8544
#define CONTRAST_MIN	0
#define CONTRAST_STEP	8
#define CONTRAST_NUM	17

static uint8_t conNum(void)
{
	return CONTRAST_NUM;
}

static uint8_t conValue(uint8_t i)
{
	uint16_t v = CONTRAST_MIN + i * CONTRAST_STEP;
	return (v > 127) ? 127 : v;
}

static void conName(uint8_t i, char *out, uint8_t n)
{
	snprintf(out, n, "%u", conValue(i));
}

static uint8_t conGet(void)
{
	int16_t i = ((int16_t)dev_settings.ext.contrast - CONTRAST_MIN + CONTRAST_STEP/2) / CONTRAST_STEP;
	return (i < 0) ? 0 : (i >= CONTRAST_NUM) ? CONTRAST_NUM - 1 : i;
}

static void conSet(uint8_t i)
{
	dev_settings.ext.contrast = conValue(i);
	dispSetContrast(&disp_dev, dev_settings.ext.contrast);
}

static void conPreview(uint8_t i, uint8_t active)
{
	dispSetContrast(&disp_dev, active ? conValue(i) : dev_settings.ext.contrast);
}

static void getCon(char *out, uint8_t n)
{
	snprintf(out, n, "%u", dev_settings.ext.contrast);
}

static const uint32_t splash_vals[] = {0, 1, 2, 3, 5, 10};

static uint8_t splNum(void)
{
	return arrlen(splash_vals);
}

static void splName(uint8_t i, char *out, uint8_t n)
{
	if (splash_vals[i] == 0)
		put(out, n, "Off");
	else
		snprintf(out, n, "%lus", (unsigned long)splash_vals[i]);
}

static uint8_t splGet(void)
{
	return nearestIdx(splash_vals, arrlen(splash_vals), dev_settings.ext.splash_time);
}

static void splSet(uint8_t i)
{
	dev_settings.ext.splash_time = splash_vals[i];
}

static void getSpl(char *out, uint8_t n)
{
	splName(splGet(), out, n);
}

static void getSplash1(char *out, uint8_t n)
{
	put(out, n, dev_settings.welcome_msg[0]);
}

static void getSplash2(char *out, uint8_t n)
{
	put(out, n, dev_settings.welcome_msg[1]);
}

static uint8_t setSplash1(const char *s)
{
	snprintf(dev_settings.welcome_msg[0], sizeof(dev_settings.welcome_msg[0]), "%s", s);
	return 0;
}

static uint8_t setSplash2(const char *s)
{
	snprintf(dev_settings.welcome_msg[1], sizeof(dev_settings.welcome_msg[1]), "%s", s);
	return 0;
}

//--- keyboard ---
static const uint32_t kto_vals[] = {300, 500, 750, 1000, 1500};

static uint8_t ktoNum(void)
{
	return arrlen(kto_vals);
}

static void msName(const uint32_t *v, uint8_t i, char *out, uint8_t n)
{
	snprintf(out, n, "%lums", (unsigned long)v[i]);
}

static void ktoName(uint8_t i, char *out, uint8_t n)
{
	msName(kto_vals, i, out, n);
}

static uint8_t ktoGet(void)
{
	return nearestIdx(kto_vals, arrlen(kto_vals), dev_settings.kbd_timeout);
}

static void ktoSet(uint8_t i)
{
	dev_settings.kbd_timeout = kto_vals[i];
	setKeysTimeout(dev_settings.kbd_timeout);
}

static void getKto(char *out, uint8_t n)
{
	snprintf(out, n, "%ums", dev_settings.kbd_timeout);
}

static const uint32_t kdl_vals[] = {100, 150, 200, 300};

static uint8_t kdlNum(void)
{
	return arrlen(kdl_vals);
}

static void kdlName(uint8_t i, char *out, uint8_t n)
{
	msName(kdl_vals, i, out, n);
}

static uint8_t kdlGet(void)
{
	return nearestIdx(kdl_vals, arrlen(kdl_vals), dev_settings.kbd_delay);
}

static void kdlSet(uint8_t i)
{
	dev_settings.kbd_delay = kdl_vals[i];
}

static void getKdl(char *out, uint8_t n)
{
	snprintf(out, n, "%ums", dev_settings.kbd_delay);
}

static uint8_t ukrGet(void)
{
	return dev_settings.ext.ukrainian;
}

static void ukrSet(uint8_t i)
{
	dev_settings.ext.ukrainian = i;
}

static void getUkr(char *out, uint8_t n)
{
	offOnName(ukrGet(), out, n);
}

//--- alerts ---
static uint8_t ringNum(void)
{
	return ringtones_num;
}

static void ringName(uint8_t i, char *out, uint8_t n)
{
	put(out, n, ringtones[i].name);
}

static uint8_t ringGet(void)
{
	return dev_settings.ext.ringtone;
}

static void ringSet(uint8_t i)
{
	dev_settings.ext.ringtone = i;
}

static void ringPreview(uint8_t i, uint8_t active)
{
	if (active)
		ringtonePlay(i);
	else
		ringtoneStop();
}

static void getRing(char *out, uint8_t n)
{
	ringName(ringGet(), out, n);
}

static uint8_t vibGet(void)
{
	return dev_settings.ext.vibration;
}

static void vibSet(uint8_t i)
{
	dev_settings.ext.vibration = i;
}

static void vibPreview(uint8_t i, uint8_t active)
{
	if (active && i)
		actVibr(150);
}

static void getVib(char *out, uint8_t n)
{
	offOnName(vibGet(), out, n);
}

//--- messages ---
static uint8_t noGet(void)
{
	return 0;
}

static void clearSet(uint8_t i)
{
	if (i)
		inboxClear();
}

static void delSet(uint8_t i)
{
	if (i)
		uiDeleteViewedMessage();
}

static void getInbox(char *out, uint8_t n)
{
	uint8_t u = inboxUnread();

	if (u)
		snprintf(out, n, "%u new", u);
	else
		snprintf(out, n, "%u", inboxCount());
}

//--- info ---
static void getFw(char *out, uint8_t n)
{
	put(out, n, FW_VER);
}

static void getLibm17(char *out, uint8_t n)
{
	put(out, n, LIBM17_VERSION);
}

//"sa8x8-fw/v1.4.r3.gabc1234" -> "v1.4.r3"
static void getRfFw(char *out, uint8_t n)
{
	const char *v = getVersionRF();
	const char *s = strchr(v, '/');

	if (!*v)
	{
		put(out, n, "n/a");
		return;
	}

	put(out, n, s ? s + 1 : v);

	char *g = strstr(out, ".g");
	if (g)
		*g = 0;
}

static void getRfFwFull(char *out, uint8_t n)
{
	const char *v = getVersionRF();

	put(out, n, *v ? v : "No answer to AT+VERSION - factory firmware?");
}

static void getRfModel(char *out, uint8_t n)
{
	const char *m = getModelRF();

	put(out, n, *m ? m : "n/a");
}

static void getBatt(char *out, uint8_t n)
{
	uint16_t mv = getBattVoltage();

	snprintf(out, n, "%u.%02uV", mv / 1000, (mv % 1000) / 10);
}

static void getAuthor(char *out, uint8_t n)
{
	put(out, n, "SP5WWP");
}

//--- channels (the one picked in Channels, see uiEditChannel()) ---
static ch_settings_t *editCh(void)
{
	uint16_t i = uiEditChannel();

	return (i < codeplug.num_items) ? &codeplug.channel[i] : &codeplug.channel[0];
}

//a channel was changed: store it, update the radio if it is the active one
static void chEdited(void)
{
	codeplugTouch();
	if (dev_settings.tuning_mode == TUNING_MEM && dev_settings.ch_num == uiEditChannel())
		tuneChannelsChanged();
}

static void getChName(char *out, uint8_t n)
{
	put(out, n, editCh()->ch_name);
}

static uint8_t setChName(const char *s)
{
	if (*s == 0)
		return 1;

	snprintf(editCh()->ch_name, sizeof(editCh()->ch_name), "%s", s);
	chEdited();
	return 0;
}

static void getChRx(char *out, uint8_t n)
{
	fmtFreq(out, n, editCh()->rx_frequency);
}

static void getChTx(char *out, uint8_t n)
{
	fmtFreq(out, n, editCh()->tx_frequency);
}

static uint8_t setChFreq(const char *s, uint32_t *f)
{
	uint32_t hz;

	if (parseFreq(s, &hz) || !tuneFreqValid(hz))
		return 1;

	*f = hz;
	chEdited();
	return 0;
}

static uint8_t setChRx(const char *s)
{
	//a simplex channel stays simplex
	ch_settings_t *c = editCh();
	uint8_t simplex = (c->rx_frequency == c->tx_frequency);

	if (setChFreq(s, &c->rx_frequency))
		return 1;
	if (simplex)
		c->tx_frequency = c->rx_frequency;

	return 0;
}

static uint8_t setChTx(const char *s)
{
	return setChFreq(s, &editCh()->tx_frequency);
}

static void getChDst(char *out, uint8_t n)
{
	put(out, n, editCh()->dst);
}

static uint8_t setChDst(const char *s)
{
	char c[10];

	if (cleanCallsign(c, s, 1))
		return 1;

	copyCallsign(editCh()->dst, sizeof(editCh()->dst), c);
	chEdited();
	return 0;
}

static void getChCan(char *out, uint8_t n)
{
	snprintf(out, n, "%u", editCh()->can);
}

static uint8_t setChCan(const char *s)
{
	char *end;
	long v = strtol(s, &end, 10);

	if (*s == 0 || *end != 0 || v < 0 || v > 15)
		return 1;

	editCh()->can = (uint8_t)v;
	chEdited();
	return 0;
}

static uint8_t chPwrGet(void)
{
	return (editCh()->rf_pwr == RF_PWR_HIGH) ? 1 : 0;
}

static void chPwrSet(uint8_t i)
{
	editCh()->rf_pwr = i ? RF_PWR_HIGH : RF_PWR_LOW;
	chEdited();
}

static void getChPwr(char *out, uint8_t n)
{
	pwrName(chPwrGet(), out, n);
}

static void chDelSet(uint8_t i)
{
	if (i)
		uiDeleteEditedChannel();
}

//--- the table ---
#define TEXT_FIELD(g, gf, s, len, m, first, t)	{ .kind = FK_TEXT, .title = t, .get = g, .get_full = gf, .set_text = s, \
												.max_len = len, .modes = m, .first_mode = first, .save = 1 }
#define NUM_FIELD(g, gf, s, len, t)				{ .kind = FK_NUMBER, .title = t, .get = g, .get_full = gf, .set_text = s, \
												.max_len = len, .modes = TM(TEXT_NUMERIC), .first_mode = TEXT_NUMERIC, .save = 1 }
#define OPT_FIELD(g, num, name, og, os, prev)	{ .kind = FK_OPTION, .get = g, .opt_num = num, .opt_name = name, \
												.opt_get = og, .opt_set = os, .opt_preview = prev, .save = 1 }
#define INFO_FIELD(g, gf)						{ .kind = FK_INFO, .get = g, .get_full = gf }
#define ACTION_FIELD(a)							{ .kind = FK_ACTION, .action = a }

#define CALL_MODES	(TM_LATIN | TM(TEXT_NUMERIC))

static const field_t fields[EDIT_NUM_ITEMS] =
{
	[EDIT_M17_SRC_CALLSIGN]	= TEXT_FIELD(getSrc, NULL, setSrc, 9, CALL_MODES, TEXT_UPPERCASE, "Callsign"),
	[EDIT_M17_DST_CALLSIGN]	= TEXT_FIELD(getDst, NULL, setDst, 9, CALL_MODES, TEXT_UPPERCASE, "Dest."),
	[EDIT_M17_CAN]			= NUM_FIELD(getCan, NULL, setCan, 2, "CAN"),

	[EDIT_RF_PPM]			= NUM_FIELD(getPpm, getPpmEdit, setPpm, 6, "Offset ppm"),
	[EDIT_RF_PWR]			= OPT_FIELD(getPwr, noYesNum, pwrName, pwrGet, pwrSet, NULL),
	[EDIT_TUNING_MODE]		= OPT_FIELD(getMode, noYesNum, modeName, modeGet, modeSet, NULL),
	[EDIT_MEM_CHANNEL]		= OPT_FIELD(getChan, chanNum, chanName, chanGet, chanSet, NULL),
	[EDIT_VFO_FREQ]			= NUM_FIELD(getVfoFreq, NULL, setVfoFreq, 10, "MHz"),
	[EDIT_VFO_STEP]			= OPT_FIELD(getStep, stepNum, stepName, stepGet, stepSet, NULL),
	[EDIT_VFO_SHIFT]		= NUM_FIELD(getShift, getShiftEdit, setShift, 10, "Shift MHz"),
	[EDIT_MEM_NUMBER]		= NUM_FIELD(getEmpty, NULL, setMemNumber, 3, "Channel"),

	[EDIT_BL_LEVEL]			= OPT_FIELD(getBl, blNum, blName, blGet, blSet, blPreview),
	[EDIT_BL_TIMER]			= OPT_FIELD(getBlt, bltNum, bltName, bltGet, bltSet, NULL),
	[EDIT_BL_ALWAYS]		= OPT_FIELD(getBla, noYesNum, noYesName, blaGet, blaSet, NULL),
	[EDIT_CONTRAST]			= OPT_FIELD(getCon, conNum, conName, conGet, conSet, conPreview),
	[EDIT_SPLASH_TIME]		= OPT_FIELD(getSpl, splNum, splName, splGet, splSet, NULL),
	[EDIT_SPLASH_LINE1]		= TEXT_FIELD(getSplash1, NULL, setSplash1, 23, TM_ALL & ~TM(TEXT_T9), TEXT_LOWERCASE, "Line 1"),
	[EDIT_SPLASH_LINE2]		= TEXT_FIELD(getSplash2, NULL, setSplash2, 23, TM_ALL & ~TM(TEXT_T9), TEXT_LOWERCASE, "Line 2"),

	[EDIT_KBD_TIMEOUT]		= OPT_FIELD(getKto, ktoNum, ktoName, ktoGet, ktoSet, NULL),
	[EDIT_KBD_DELAY]		= OPT_FIELD(getKdl, kdlNum, kdlName, kdlGet, kdlSet, NULL),
	[EDIT_KBD_UKRAINIAN]	= { .kind = FK_OPTION, .title = "Ukrainian input", .get = getUkr, .opt_num = noYesNum,
								.opt_name = offOnName, .opt_get = ukrGet, .opt_set = ukrSet, .save = 1 },

	[EDIT_RINGTONE]			= OPT_FIELD(getRing, ringNum, ringName, ringGet, ringSet, ringPreview),
	[EDIT_VIBRATION]		= OPT_FIELD(getVib, noYesNum, offOnName, vibGet, vibSet, vibPreview),

	[EDIT_MSG_WRITE]		= ACTION_FIELD(uiActWrite),
	[EDIT_MSG_REPLY]		= ACTION_FIELD(uiActReply),
	[EDIT_MSG_FORWARD]		= ACTION_FIELD(uiActForward),
	[EDIT_MSG_DETAILS]		= ACTION_FIELD(uiActDetails),
	[EDIT_MSG_SET_DST]		= ACTION_FIELD(uiActSenderAsDst),
	[EDIT_MSG_DELETE]		= { .kind = FK_OPTION, .title = "Delete?", .opt_num = noYesNum, .opt_name = noYesName,
								.opt_get = noGet, .opt_set = delSet },
	[EDIT_INBOX_CLEAR]		= { .kind = FK_OPTION, .title = "Delete all?", .opt_num = noYesNum, .opt_name = noYesName,
								.opt_get = noGet, .opt_set = clearSet },
	[EDIT_INFO_INBOX]		= INFO_FIELD(getInbox, NULL),

	[EDIT_CH_SELECT]		= ACTION_FIELD(uiActChannelSelect),
	[EDIT_CH_DELETE]		= { .kind = FK_OPTION, .title = "Delete?", .opt_num = noYesNum, .opt_name = noYesName,
								.opt_get = noGet, .opt_set = chDelSet },
	[EDIT_CH_NAME]			= { .kind = FK_TEXT, .title = "Name", .get = getChName, .set_text = setChName, .max_len = 23,
								.modes = TM_ALL & ~TM(TEXT_T9), .first_mode = TEXT_UPPERCASE },
	[EDIT_CH_RX]			= { .kind = FK_NUMBER, .title = "RX MHz", .get = getChRx, .set_text = setChRx, .max_len = 10,
								.modes = TM(TEXT_NUMERIC), .first_mode = TEXT_NUMERIC },
	[EDIT_CH_TX]			= { .kind = FK_NUMBER, .title = "TX MHz", .get = getChTx, .set_text = setChTx, .max_len = 10,
								.modes = TM(TEXT_NUMERIC), .first_mode = TEXT_NUMERIC },
	[EDIT_CH_DST]			= { .kind = FK_TEXT, .title = "Dest.", .get = getChDst, .set_text = setChDst, .max_len = 9,
								.modes = CALL_MODES, .first_mode = TEXT_UPPERCASE },
	[EDIT_CH_CAN]			= { .kind = FK_NUMBER, .title = "CAN", .get = getChCan, .set_text = setChCan, .max_len = 2,
								.modes = TM(TEXT_NUMERIC), .first_mode = TEXT_NUMERIC },
	[EDIT_CH_PWR]			= { .kind = FK_OPTION, .get = getChPwr, .opt_num = noYesNum, .opt_name = pwrName,
								.opt_get = chPwrGet, .opt_set = chPwrSet },

	[EDIT_INFO_FW]			= INFO_FIELD(getFw, NULL),
	[EDIT_INFO_LIBM17]		= INFO_FIELD(getLibm17, NULL),
	[EDIT_INFO_RF_FW]		= INFO_FIELD(getRfFw, getRfFwFull),
	[EDIT_INFO_RF_MODEL]	= INFO_FIELD(getRfModel, NULL),
	[EDIT_INFO_BATT]		= INFO_FIELD(getBatt, NULL),
	[EDIT_INFO_AUTHOR]		= INFO_FIELD(getAuthor, NULL),
};

const field_t *fieldGet(edit_set_t e)
{
	static const field_t none = { .kind = FK_NONE };

	return (e > EDIT_NONE && e < EDIT_NUM_ITEMS) ? &fields[e] : &none;
}

void fieldValue(edit_set_t e, char *out, uint8_t n)
{
	const field_t *f = fieldGet(e);

	out[0] = 0;
	if (f->get != NULL)
		f->get(out, n);
}

void fieldFullText(edit_set_t e, char *out, uint8_t n)
{
	const field_t *f = fieldGet(e);

	out[0] = 0;
	if (f->get_full != NULL)
		f->get_full(out, n);
	else if (f->get != NULL)
		f->get(out, n);
}

void applyDeviceSettings(void)
{
	dispSetContrast(&disp_dev, dev_settings.ext.contrast);
	setKeysTimeout(dev_settings.kbd_timeout);
	setBacklightTimer(dev_settings.backlight_timer);

	if (dev_settings.backlight_always)
	{
		HAL_TIM_Base_Stop_IT(&htim14);
		TIM14->CNT = 0;
		setBacklight(dev_settings.backlight_level);
	}
}

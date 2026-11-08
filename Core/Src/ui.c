//User interface state machine
//
//Screens are kept on a small stack, so "C" always returns to where you came
//from, with the same item highlighted. Every key press updates the state and
//redraws the whole screen into the frame buffer (main loop flushes it).
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "display.h"
#include "keypad.h"
#include "menus.h"
#include "platform.h"
#include "ringtones.h"
#include "nvmem.h"
#include "tuning.h"
#include "inbox.h"
#include "msginfo.h"
#include "charset.h"
#include "m17_tx.h"

#define UI_STACK_DEPTH		8
#define SAVE_DELAY_MS		3000U	//deferred settings save
#define POPUP_MS			1000U

extern dev_settings_t dev_settings;
extern radio_state_t radio_state;
extern TIM_HandleTypeDef htim14;

typedef struct
{
	disp_state_t state;
	uint8_t top, sel;		//list: first visible item, selected item
} ui_frame_t;

static disp_dev_t *dd;

static ui_frame_t cur = {DISP_MAIN_SCR, 0, 0};
static ui_frame_t stack[UI_STACK_DEPTH];
static uint8_t sp;

//editors
static abc_t te;
static edit_set_t edit_field;
static uint8_t opt_cur;							//option selected when the list was opened
static char edit_title[24];
static text_entry_t msg_mode = TEXT_LOWERCASE;	//kept between messages
static char compose_dst[12];

//viewers
static uint32_t view_seq;						//inbox message being viewed
static char msg_local[sizeof(((msg_t*)0)->text)];
static char view_title[24];
static char view_text[400];
static uint16_t scroll;
static uint8_t msg_deleted;

//popup
static uint8_t popup_on;
static uint32_t popup_end;						//0: until replaced
static char popup_text[32];

static uint8_t tx_from_ui;
static uint8_t save_pending;
static uint32_t save_tick;

static void redraw(void);

//--- navigation ---
static void push(disp_state_t s)
{
	if (sp < UI_STACK_DEPTH)
		stack[sp++] = cur;
	else
		stack[UI_STACK_DEPTH-1] = cur;

	cur.state = s;
	cur.top = cur.sel = 0;
	scroll = 0;
}

static void pop(void)
{
	if (sp)
		cur = stack[--sp];
	else
		cur = (ui_frame_t){DISP_MAIN_SCR, 0, 0};

	scroll = 0;
}

static void goHome(void)
{
	sp = 0;
	cur = (ui_frame_t){DISP_MAIN_SCR, 0, 0};
}

static void popup(const char *text, uint32_t ms)
{
	snprintf(popup_text, sizeof(popup_text), "%s", text);
	popup_on = 1;
	popup_end = ms ? (HAL_GetTick() + ms) | 1 : 0;
	redraw();
}

static void backlightKick(void)
{
	if (dev_settings.backlight_always)
		return;

	if (TIM14->CNT == 0)
	{
		setBacklight(dev_settings.backlight_level);
		startBacklightTimer();
	}
	else
	{
		TIM14->CNT = 0;
	}
}

//--- lists ---
static uint8_t listNum(void)
{
	if (cur.state == DISP_INBOX)
		return inboxCount();

	if (cur.state == DISP_OPTION_SELECT)
		return fieldGet(edit_field)->opt_num();

	if (isStaticMenu(cur.state))
		return displays[cur.state].num_items;

	return 0;
}

static void listFix(uint8_t num)
{
	if (num == 0)
	{
		cur.sel = cur.top = 0;
		return;
	}

	if (cur.sel >= num)
		cur.sel = num - 1;
	if (cur.sel < cur.top)
		cur.top = cur.sel;
	else if (cur.sel >= cur.top + LIST_ROWS)
		cur.top = cur.sel - LIST_ROWS + 1;
}

//dir > 0: down the list (wraps around)
static void listMove(int8_t dir)
{
	uint8_t num = listNum();

	if (num == 0)
		return;

	if (dir > 0)
		cur.sel = (cur.sel + 1 < num) ? cur.sel + 1 : 0;
	else
		cur.sel = cur.sel ? cur.sel - 1 : num - 1;

	listFix(num);
}

static void menuItem(uint8_t idx, char *label, char *value, uint8_t n)
{
	const disp_t *d = &displays[cur.state];

	snprintf(label, n, "%s", d->item[idx]);
	fieldValue(d->edit[idx], value, n);
}

static void inboxItem(uint8_t idx, char *label, char *value, uint8_t n)
{
	const inbox_msg_t *m = inboxGet(idx);
	char preview[32];

	(void)value;
	if (m == NULL)
		return;

	utf8ToLocal(preview, sizeof(preview), m->text);
	snprintf(label, n, "%s%s: %s", m->unread ? ICON_ENVELOPE : "   ", m->src, preview);
}

static void optionItem(uint8_t idx, char *label, char *value, uint8_t n)
{
	fieldGet(edit_field)->opt_name(idx, label, n);
	snprintf(value, n, "%s", (idx == opt_cur) ? ICON_RADIO_ON : ICON_RADIO_OFF);
}

//--- messages ---
static const inbox_msg_t *viewedMsg(int16_t *idx)
{
	int16_t i = inboxFind(view_seq);

	if (idx != NULL)
		*idx = i;

	return (i < 0) ? NULL : inboxGet(i);
}

static void openMessage(uint8_t idx)
{
	const inbox_msg_t *m = inboxGet(idx);

	if (m == NULL)
		return;

	view_seq = m->seq;
	inboxMarkRead(idx);
	utf8ToLocal(msg_local, sizeof(msg_local), m->text);

	push(DISP_MSG_VIEW);
}

static uint8_t entryModes(uint8_t modes)
{
	if (!dev_settings.ext.ukrainian)
		modes &= ~TM_UKR;

	return modes;
}

static void compose(const char *dst, const char *text)
{
	snprintf(compose_dst, sizeof(compose_dst), "%s", dst);

	if (!dev_settings.ext.ukrainian && (msg_mode == TEXT_UKR_LOWER || msg_mode == TEXT_UKR_UPPER))
		msg_mode = TEXT_LOWERCASE;

	textEntryInit(&te, text, sizeof(te.buffer) - 1, entryModes(TM_ALL), msg_mode);
	push(DISP_TEXT_MSG_ENTRY);
}

void uiActWrite(void)
{
	compose(dev_settings.channel.dst, "");
}

void uiActReply(void)
{
	const inbox_msg_t *m = viewedMsg(NULL);

	if (m != NULL)
		compose(m->src, "");
}

void uiActForward(void)
{
	if (viewedMsg(NULL) != NULL)
		compose(dev_settings.channel.dst, msg_local);
}

void uiActDetails(void)
{
	const inbox_msg_t *m = viewedMsg(NULL);

	if (m == NULL)
		return;

	snprintf(view_title, sizeof(view_title), "Details");
	msgInfoText(m, view_text, sizeof(view_text));
	push(DISP_TEXT_VIEW);
}

void uiActSenderAsDst(void)
{
	const inbox_msg_t *m = viewedMsg(NULL);

	if (m == NULL)
		return;

	copyCallsign(dev_settings.channel.dst, sizeof(dev_settings.channel.dst), m->src);
	tuneChannelEdited();
	uiSaveNow();
	pop();
	popup("Dest. set", POPUP_MS);
}

void uiDeleteViewedMessage(void)
{
	int16_t idx;

	if (viewedMsg(&idx) != NULL)
	{
		inboxDelete(idx);
		msg_deleted = 1;
	}
}

//--- fields ---
static void openField(edit_set_t e, const char *label, const char *initial)
{
	const field_t *f = fieldGet(e);
	char buf[48];

	snprintf(edit_title, sizeof(edit_title), "%s", f->title ? f->title : label);

	switch (f->kind)
	{
		case FK_TEXT:
		case FK_NUMBER:
			edit_field = e;
			if (initial != NULL)
				snprintf(buf, sizeof(buf), "%s", initial);
			else
				fieldFullText(e, buf, sizeof(buf));
			textEntryInit(&te, buf, f->max_len, entryModes(f->modes), f->first_mode);
			push(DISP_TEXT_VALUE_ENTRY);
		break;

		case FK_OPTION:
			edit_field = e;
			opt_cur = f->opt_get();
			push(DISP_OPTION_SELECT);
			cur.sel = opt_cur;
			listFix(f->opt_num());
			if (f->opt_preview != NULL)
				f->opt_preview(cur.sel, 1);
		break;

		case FK_INFO:
			snprintf(view_title, sizeof(view_title), "%s", label);
			fieldFullText(e, view_text, 255);
			push(DISP_TEXT_VIEW);
		break;

		case FK_ACTION:
			f->action();
		break;

		default:
		break;
	}
}

static void commitText(void)
{
	const field_t *f = fieldGet(edit_field);

	textEntryConfirm(&te);

	if (f->set_text(te.buffer))
	{
		popup("Invalid", POPUP_MS);
		return;
	}

	if (f->save)
		uiSaveNow();

	pop();
}

static void commitOption(void)
{
	const field_t *f = fieldGet(edit_field);

	uint8_t chosen = cur.sel;

	msg_deleted = 0;
	f->opt_set(chosen);
	if (f->opt_preview != NULL)
		f->opt_preview(chosen, 0);

	if (f->save)
		uiSaveNow();

	pop();

	if (edit_field == EDIT_MSG_DELETE && msg_deleted)
	{
		//leave the options and the deleted message
		while (cur.state == DISP_MSG_OPTIONS || cur.state == DISP_MSG_VIEW)
			pop();
		if (cur.state == DISP_INBOX && inboxCount() == 0)
			pop();
		popup("Deleted", POPUP_MS);
	}
	else if (edit_field == EDIT_INBOX_CLEAR && chosen == 1)
	{
		popup("Deleted", POPUP_MS);
	}
}

static void selectMenuItem(uint8_t idx)
{
	const disp_t *d = &displays[cur.state];

	if (idx >= d->num_items)
		return;

	cur.sel = idx;
	listFix(d->num_items);

	disp_state_t next = d->next_disp[idx];

	if (next == DISP_INBOX && inboxCount() == 0)
		popup("Inbox empty", POPUP_MS);
	else if (next != DISP_NONE)
		push(next);
	else
		openField(d->edit[idx], d->item[idx], NULL);
}

//--- drawing ---
static void redraw(void)
{
	if (popup_on)
	{
		showPopup(dd, popup_text);
		return;
	}

	switch (cur.state)
	{
		case DISP_MAIN_SCR:
			showMainScreen(dd);
		break;

		case DISP_INBOX:
		{
			char t[20];
			listFix(inboxCount());
			snprintf(t, sizeof(t), "Inbox (%u)", inboxCount());
			showList(dd, t, inboxCount(), cur.top, cur.sel, inboxItem);
		}
		break;

		case DISP_MSG_VIEW:
		{
			int16_t idx;
			const inbox_msg_t *m = viewedMsg(&idx);
			char age[8];

			if (m == NULL)
			{
				goHome();
				showMainScreen(dd);
				break;
			}

			msgAge(m, age, sizeof(age));
			showMessageView(dd, m->src, m->dst, age, msg_local, scroll, idx, inboxCount());
		}
		break;

		case DISP_MSG_NOTIFY:
		{
			const inbox_msg_t *m = inboxGet(0);
			showNotify(dd, m ? m->src : "?", inboxUnread());
		}
		break;

		case DISP_TEXT_MSG_ENTRY:
			showTextMessageEntry(dd, te.mode, compose_dst, te.buffer);
		break;

		case DISP_TEXT_VALUE_ENTRY:
			showTextValueEntry(dd, te.mode, edit_title, te.buffer);
		break;

		case DISP_OPTION_SELECT:
			showList(dd, edit_title, listNum(), cur.top, cur.sel, optionItem);
		break;

		case DISP_TEXT_VIEW:
			showTextView(dd, view_title, view_text, scroll);
		break;

		default:
			if (isStaticMenu(cur.state))
			{
				listFix(displays[cur.state].num_items);
				showList(dd, displays[cur.state].title, displays[cur.state].num_items,
						cur.top, cur.sel, menuItem);
			}
			else
			{
				goHome();
				showMainScreen(dd);
			}
		break;
	}
}

void uiRefresh(void)
{
	redraw();
}

//--- keys ---
static uint8_t keyDigit(kbd_key_t key)
{
	if (key >= KEY_1 && key <= KEY_9)
		return 1 + key - KEY_1;
	if (key == KEY_0)
		return 0;

	return 0xFF;
}

static void keyMainScreen(kbd_key_t key)
{
	uint8_t d = keyDigit(key);

	switch (key)
	{
		case KEY_OK:
			push(DISP_MAIN_MENU);
		break;

		case KEY_RIGHT:
			tuneStep(+1);
			uiSaveLater();
		break;

		case KEY_LEFT:
			tuneStep(-1);
			uiSaveLater();
		break;

		case KEY_HASH:
			if (dev_settings.tuning_mode == TUNING_VFO && tuneNumChannels() == 0)
				popup("No channels", POPUP_MS);
			else
			{
				tuneToggleMode();
				uiSaveLater();
			}
		break;

		case KEY_ASTERISK:
			uiActWrite();
		break;

		default:
			//digits: direct frequency / channel number entry
			if (d != 0xFF)
			{
				char init[2] = {'0' + d, 0};

				if (dev_settings.tuning_mode == TUNING_VFO)
					openField(EDIT_VFO_FREQ, "MHz", init);
				else
					openField(EDIT_MEM_NUMBER, "Channel", init);
			}
		break;
	}
}

static void keyTextEntry(kbd_key_t key)
{
	switch (key)
	{
		case KEY_OK:
			if (cur.state == DISP_TEXT_VALUE_ENTRY)
				commitText();
			else if (te.pos > 0)
			{
				static char utf[SMS_MAX_LEN + 1];

				textEntryConfirm(&te);
				localToUtf8(utf, sizeof(utf), te.buffer);
				uiSendUtf8(utf, compose_dst, 1);
			}
		break;

		case KEY_C:
			if (!textEntryBackspace(&te))
				pop(); //empty: cancel
		break;

		case KEY_HASH:
			textEntryNextMode(&te);
			if (cur.state == DISP_TEXT_MSG_ENTRY)
				msg_mode = te.mode;
		break;

		case KEY_RIGHT:
			textEntryConfirm(&te);
		break;

		case KEY_LEFT:
		break;

		default:
			textEntryKey(&te, key);
		break;
	}
}

void uiHandleKey(kbd_key_t key)
{
	if (key == KEY_NONE)
		return;

	backlightKick();

	//any key closes a popup
	if (popup_on)
	{
		if (popup_end == 0) //"Sending..."
			return;

		popup_on = 0;
		redraw();
		return;
	}

	switch (cur.state)
	{
		case DISP_MAIN_SCR:
			keyMainScreen(key);
		break;

		case DISP_TEXT_MSG_ENTRY:
		case DISP_TEXT_VALUE_ENTRY:
			keyTextEntry(key);
		break;

		case DISP_OPTION_SELECT:
		{
			const field_t *f = fieldGet(edit_field);

			if (key == KEY_LEFT || key == KEY_RIGHT)
			{
				listMove((key == KEY_LEFT) ? +1 : -1);
				if (f->opt_preview != NULL)
					f->opt_preview(cur.sel, 1);
			}
			else if (key == KEY_OK)
			{
				commitOption();
			}
			else if (key == KEY_C)
			{
				if (f->opt_preview != NULL)
					f->opt_preview(opt_cur, 0);
				pop();
			}
		}
		break;

		case DISP_INBOX:
			if (key == KEY_LEFT || key == KEY_RIGHT)
				listMove((key == KEY_LEFT) ? +1 : -1);
			else if (key == KEY_OK)
				openMessage(cur.sel);
			else if (key == KEY_C)
				pop();
		break;

		case DISP_MSG_VIEW:
		{
			uint16_t lines = countLines(&nokia_small, msg_local);

			if (key == KEY_LEFT && scroll + 3 < lines)
				scroll++;
			else if (key == KEY_RIGHT && scroll)
				scroll--;
			else if (key == KEY_OK)
				push(DISP_MSG_OPTIONS);
			else if (key == KEY_C)
				pop();
		}
		break;

		case DISP_MSG_NOTIFY:
			if (key == KEY_OK)
			{
				pop();
				openMessage(0);
			}
			else if (key == KEY_C)
				pop();
		break;

		case DISP_TEXT_VIEW:
		{
			uint16_t lines = countLines(&nokia_small, view_text);

			if (key == KEY_LEFT && scroll + 4 < lines)
				scroll++;
			else if (key == KEY_RIGHT && scroll)
				scroll--;
			else if (key == KEY_OK || key == KEY_C)
				pop();
		}
		break;

		default: //static menus
			if (key == KEY_LEFT || key == KEY_RIGHT)
				listMove((key == KEY_LEFT) ? +1 : -1);
			else if (key == KEY_OK)
				selectMenuItem(cur.sel);
			else if (key == KEY_C)
				pop();
			else if (keyDigit(key) >= 1 && keyDigit(key) <= 9) //shortcut: n-th item
				selectMenuItem(keyDigit(key) - 1);
		break;
	}

	if (!popup_on)
		redraw();
}

//--- messaging ---
int8_t uiSendUtf8(const char *utf8, const char *dst, uint8_t from_ui)
{
	if (radio_state != RF_RX || m17TxActive())
		return -1;

	ringtoneStop();
	popup("Sending...", 0);
	dispFlush(dd);

	int8_t ret = m17TxStart(utf8, dst, &dev_settings);

	if (ret == -2)
	{
		popup("Bad call", 1500);
		return ret;
	}
	else if (ret != 0)
	{
		popup_on = 0;
		redraw();
		return ret;
	}

	tx_from_ui = from_ui;
	return 0;
}

void uiTxDone(void)
{
	popup_on = 0;

	if (tx_from_ui)
	{
		if (cur.state == DISP_TEXT_MSG_ENTRY)
			pop();
		if (cur.state == DISP_MSG_OPTIONS) //reply/forward: back to the message
			pop();

		tx_from_ui = 0;
		popup("Sent", POPUP_MS);
		return;
	}

	redraw();
}

void uiMessageReceived(void)
{
	backlightKick();

	if (!m17TxActive())
	{
		if (dev_settings.ext.ringtone)
			ringtonePlay(dev_settings.ext.ringtone);
		if (dev_settings.ext.vibration)
			actVibr(200);
	}

	//interrupt only when idle - never lose a message being written
	if (cur.state == DISP_MAIN_SCR)
	{
		popup_on = 0;
		push(DISP_MSG_NOTIFY);
	}
	else if (cur.state == DISP_INBOX)
	{
		//keep the same message highlighted (the new one is on top)
		cur.sel++;
	}

	if (!popup_on)
		redraw();
}

void uiShowNewestMessage(void)
{
	if (inboxCount() == 0)
		return;

	goHome();
	popup_on = 0;
	openMessage(0);
	redraw();
}

//--- settings storage ---
void uiSaveNow(void)
{
	if (saveNeedsErase(sizeof(dev_settings_t)))
	{
		showPopup(dd, "Saving...");
		dispFlush(dd); //show it now - the Flash erase blocks for a while
	}

	saveData(&dev_settings, sizeof(dev_settings_t));
	save_pending = 0;
}

void uiSaveLater(void)
{
	save_pending = 1;
	save_tick = HAL_GetTick();
}

//--- periodic ---
void uiTick(void)
{
	uint32_t now = HAL_GetTick();

	if (popup_on && popup_end && (int32_t)(now - popup_end) >= 0)
	{
		popup_on = 0;
		redraw();
	}

	if (save_pending && (now - save_tick) > SAVE_DELAY_MS && radio_state == RF_RX && !m17TxActive())
		uiSaveNow();
}

void uiSecondTick(void)
{
	if (popup_on)
		return;

	if (cur.state == DISP_MAIN_SCR)
		drawBattStatus(dd);
}

void uiInit(disp_dev_t *disp)
{
	dd = disp;
	goHome();
	redraw();
}

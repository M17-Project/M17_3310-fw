#ifndef INC_UI_H_
#define INC_UI_H_

#include <stdint.h>
#include "typedefs.h"
#include "ui_types.h"

void uiInit(disp_dev_t *disp);
void uiHandleKey(kbd_key_t key);
void uiTick(void);					//every main loop pass: popups, deferred settings save
void uiSecondTick(void);			//once per second
void uiRefresh(void);				//redraw the current screen

//messaging
void uiMessageReceived(void);		//a new message is in the inbox
void uiTxDone(void);				//the transmission has ended
int8_t uiSendUtf8(const char *utf8, const char *dst, uint8_t from_ui);
void uiShowNewestMessage(void);

//settings storage
void uiSaveNow(void);
void uiSaveLater(void);				//coalesces frequent changes (VFO tuning)

//menu actions (see menus.c)
void uiActWrite(void);
void uiActReply(void);
void uiActForward(void);
void uiActDetails(void);
void uiActSenderAsDst(void);
void uiDeleteViewedMessage(void);

#endif /* INC_UI_H_ */

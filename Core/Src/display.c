#include "display.h"

void dispWrite(disp_dev_t *disp_dev, uint8_t dc, uint8_t val)
{
	HAL_GPIO_WritePin(DISP_DC_GPIO_Port, DISP_DC_Pin, dc);

	HAL_GPIO_WritePin(DISP_CE_GPIO_Port, DISP_CE_Pin, 0);
	HAL_SPI_Transmit(disp_dev->spi, &val, 1, 10);
	HAL_GPIO_WritePin(DISP_CE_GPIO_Port, DISP_CE_Pin, 1);
}

void dispInit(disp_dev_t *disp_dev)
{
	HAL_GPIO_WritePin(DISP_DC_GPIO_Port, DISP_DC_Pin, 1);
	HAL_GPIO_WritePin(DISP_CE_GPIO_Port, DISP_CE_Pin, 1);
	HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, 1);

	//toggle LCD reset
	HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, 0); //RES low
	HAL_Delay(10);
	HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, 1); //RES high
	HAL_Delay(10);

	dispWrite(disp_dev, 0, 0x21);			//extended commands
	dispWrite(disp_dev, 0, 0x80|0x48);		//contrast (0x00 to 0x7F)
	dispWrite(disp_dev, 0, 0x06);			//temp coeff.
	dispWrite(disp_dev, 0, 0x13);			//LCD bias 1:48
	dispWrite(disp_dev, 0, 0x20);			//standard commands
	dispWrite(disp_dev, 0, 0x0C);			//normal mode
}

void dispGotoXY(disp_dev_t *disp_dev, uint8_t x, uint8_t y)
{
	dispWrite(disp_dev, 0, 0x80|(6*x));
	dispWrite(disp_dev, 0, 0x40|y);
}

void dispRefresh(disp_dev_t *disp_dev)
{
	dispGotoXY(disp_dev, 0, 0);

	for(uint16_t i=0; i<DISP_BUFF_SIZ; i++)
		dispWrite(disp_dev, 1, disp_dev->buff[i]);
}

void dispClear(disp_dev_t *disp_dev, uint8_t fill)
{
	uint8_t val = fill ? 0xFF : 0;

	memset(disp_dev->buff, val, DISP_BUFF_SIZ);

	dispGotoXY(disp_dev, 0, 0);

    for(uint16_t i=0; i<DISP_BUFF_SIZ; i++)
    	dispWrite(disp_dev, 1, val);
}

void setPixel(disp_dev_t *disp_dev, uint8_t x, uint8_t y, uint8_t set)
{
	if(x<RES_X && y<RES_Y)
	{
		uint16_t loc = x + (y/8)*RES_X;

		if(!set)
			disp_dev->buff[loc] |= (1<<(y%8));
		else
			disp_dev->buff[loc] &= ~(1<<(y%8));
	}
}

void setChar(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, char c, uint8_t color)
{
	uint8_t h=f->height;
	c-=' ';

	for(uint8_t i=0; i<h; i++)
	{
		for(uint8_t j=0; j<f->symbol[(uint8_t)c].width; j++)
		{
			if(f->symbol[(uint8_t)c].rows[i] & 1<<(((h>8)?(h+3):(h))-1-j)) //fonts are right-aligned
				setPixel(disp_dev, x+j, y+i, color);
		}
	}
}

//TODO: fix multiline text alignment when not in ALIGN_LEFT mode
void setString(disp_dev_t *disp_dev, uint8_t x, uint8_t y, const font_t *f, const char *str, uint8_t color, align_t align)
{
	uint8_t xp=0, w=0;
	uint8_t len = strlen(str);

    //get width
    for(uint8_t i=0; i<len; i++)
		w+=f->symbol[str[i]-' '].width;

	switch(align)
	{
		case ALIGN_LEFT:
			xp=0;
		break;

		case ALIGN_CENTER:
			xp=(RES_X-w)/2+1;
		break;

		case ALIGN_RIGHT:
			xp=RES_X-w;
		break;

		case ALIGN_ARB:
			xp=x;
		break;

		default: //ALIGN_LEFT
			xp=0;
        break;
	}

	for(uint8_t i=0; i<len; i++)
	{
		if(xp > RES_X-f->symbol[str[i]-' '].width)
		{
			y+=f->height+1;
			xp=0; //ALIGN_LEFT
		}

		setChar(disp_dev, xp, y, f, str[i], color);
		xp+=f->symbol[str[i]-' '].width;
	}

	dispRefresh(disp_dev);
}

void drawRect(disp_dev_t *disp_dev, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color, uint8_t fill)
{
	x0 = (x0>=RES_X) ? RES_X-1 : x0;
	y0 = (y0>=RES_Y) ? RES_Y-1 : y0;
	x1 = (x1>=RES_X) ? RES_X-1 : x1;
	y1 = (y1>=RES_Y) ? RES_Y-1 : y1;

	if(fill)
	{
		for(uint8_t i=x0; i<=x1; i++)
		{
			for(uint8_t j=y0; j<=y1; j++)
			{
				setPixel(disp_dev, i, j, color);
            }
		}
	}
	else
	{
		for(uint8_t i=x0; i<=x1; i++)
		{
			setPixel(disp_dev, i, y0, color);
			setPixel(disp_dev, i, y1, color);
		}
		for(uint8_t i=y0+1; i<=y1-1; i++)
		{
			setPixel(disp_dev, x0, i, color);
			setPixel(disp_dev, x1, i, color);
        }
    }

	dispRefresh(disp_dev);
}

void showMainScreen(disp_dev_t *disp_dev)
{
	char str[24];

	dispClear(disp_dev, 0);

	sprintf(str, "%s", (dev_settings.channel.mode==RF_MODE_4FSK)?"M17":"FM");
	setString(disp_dev, 0, 0, &nokia_small, str, 0, ALIGN_LEFT);

	setString(disp_dev, 0, 12, &nokia_big, dev_settings.channel.ch_name, 0, ALIGN_CENTER);

	sprintf(str, "R %ld.%04ld",
			dev_settings.channel.rx_frequency/1000000,
			(dev_settings.channel.rx_frequency - (dev_settings.channel.rx_frequency/1000000)*1000000)/100);
	setString(disp_dev, 0, 27, &nokia_small, str, 0, ALIGN_CENTER);

	sprintf(str, "T %ld.%04ld",
			dev_settings.channel.tx_frequency/1000000,
			(dev_settings.channel.tx_frequency - (dev_settings.channel.tx_frequency/1000000)*1000000)/100);
	setString(disp_dev, 0, 36, &nokia_small, str, 0, ALIGN_CENTER);

	//is the battery charging? (read /CHG signal)
	if(!(CHG_GPIO_Port->IDR & CHG_Pin))
	{
		setString(disp_dev, RES_X-1, 0, &nokia_small, "B+", 0, ALIGN_RIGHT); //display charging status
	}
	else
	{
		char u_batt_str[8];
		uint16_t u_batt = getBattVoltage();
		sprintf(u_batt_str, "%1d.%1d", u_batt/1000, (u_batt-(u_batt/1000)*1000)/100);
		setString(disp_dev, RES_X-1, 0, &nokia_small, u_batt_str, 0, ALIGN_RIGHT); //display voltage
	}
}

void dispSplash(disp_dev_t *disp_dev, const char *line1, const char *line2, const char *callsign)
{
	setBacklight(0);

	setString(disp_dev, 0, 9, &nokia_big, line1, 0, ALIGN_CENTER);
	setString(disp_dev, 0, 22, &nokia_big, line2, 0, ALIGN_CENTER);
	setString(disp_dev, 0, 40, &nokia_small, callsign, 0, ALIGN_CENTER);

	//fade in
	if(dev_settings.backlight_always==1)
	{
		for(uint16_t i=0; i<dev_settings.backlight_level; i++)
		{
			setBacklight(i);
			HAL_Delay(5);
		}
	}
}

void showTextMessageEntry(disp_dev_t *disp_dev, text_entry_t text_mode)
{
	dispClear(disp_dev, 0);

	redrawTextEntryIcon(disp_dev, text_mode);

	setString(disp_dev, 0, RES_Y-8, &nokia_small_bold, "Send", 0, ALIGN_CENTER);
}

void showTextValueEntry(disp_dev_t *disp_dev, text_entry_t text_mode)
{
	dispClear(disp_dev, 0);

	redrawTextEntryIcon(disp_dev, text_mode);

	drawRect(disp_dev, 0, 9, RES_X-1, RES_Y-11, 0, 0);

	setString(disp_dev, 0, RES_Y-8, &nokia_small_bold, "Ok", 0, ALIGN_CENTER);
}

//for text messages
void redrawMsgEntry(disp_dev_t *disp_dev, const char *text)
{
    drawRect(disp_dev, 0, 10, RES_X-1, RES_Y-9, 1, 1);
    setString(disp_dev, 0, 10, &nokia_small, text, 0, ALIGN_LEFT);
}

void redrawValueEntry(disp_dev_t *disp_dev, const char *text)
{
    drawRect(disp_dev, 1, 10, RES_X-2, RES_Y-12, 1, 1);
    setString(disp_dev, 3, 13, &nokia_big, text, 0, ALIGN_ARB);
}

void redrawTextEntryIcon(disp_dev_t *disp_dev, text_entry_t mode)
{
    drawRect(disp_dev, 0, 0, 21, 8, 1, 1);

    if (mode == TEXT_LOWERCASE)
        setString(disp_dev, 0, 0, &nokia_small, ICON_PEN "abc", 0, ALIGN_LEFT);
    else if (mode == TEXT_UPPERCASE)
        setString(disp_dev, 0, 0, &nokia_small, ICON_PEN "ABC", 0, ALIGN_LEFT);
    else
        setString(disp_dev, 0, 0, &nokia_small, ICON_PEN "T9", 0, ALIGN_LEFT);
}

//show menu with item highlighting
//start_item - absolute index of first item to display
//h_item - item to highlight, relative value: 0..3
void showMenu(disp_dev_t *disp_dev, disp_t menu, uint8_t start_item, uint8_t h_item)
{
    dispClear(disp_dev, 0);

    setString(disp_dev, 0, 0, &nokia_small_bold, menu.title, 0, ALIGN_CENTER);

    for(uint8_t i=0; i<start_item+menu.num_items && i<4; i++)
    {
    	//highlight
    	if(i==h_item)
    		drawRect(disp_dev, 0, (i+1)*9-1, RES_X-1, (i+1)*9+8, 0, 1);

    	setString(disp_dev, 1, (i+1)*9, &nokia_small, menu.item[i+start_item], (i==h_item)?1:0, ALIGN_ARB);
    	if(menu.value[i+start_item][0]!=0)
    		setString(disp_dev, 0, (i+1)*9, &nokia_small, menu.value[i+start_item], (i==h_item)?1:0, ALIGN_RIGHT);
    }
}

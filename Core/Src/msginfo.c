//Message details: LSF TYPE/META decoding
//
//TYPE field (libm17 layout): bit 0 packet/stream, bits 1-2 data type,
//bits 3-4 encryption type, bits 5-6 encryption subtype, bits 7-10 CAN.
//Without encryption, the subtype tells what META holds: text, GNSS position
//or extended callsign data. Packet mode uses the same META rules as stream mode.
#include "msginfo.h"
#include <stdio.h>
#include <string.h>
#include <m17.h>
#include "main.h"
#include "charset.h"

#define TYPE_ENCR(t)	(((t) >> 3) & 3U)
#define TYPE_SUB(t)		(((t) >> 5) & 3U)
#define TYPE_CAN(t)		(((t) >> 7) & 0xFU)

meta_kind_t metaKind(uint16_t type, const uint8_t meta[14])
{
	uint8_t empty = 1;

	for (uint8_t i=0; i<14; i++)
		if (meta[i])
			empty = 0;

	if (TYPE_ENCR(type) != 0)
		return META_ENCRYPTED;

	if (empty)
		return META_NONE;

	switch (TYPE_SUB(type) << 5)
	{
		case M17_TYPE_META_TEXT:		return META_TEXT;
		case M17_TYPE_META_POSITION:	return META_POSITION;
		case M17_TYPE_META_EXT_CALL:	return META_EXT_CALL;
		default:						return META_UNKNOWN;
	}
}

static int32_t s24(const uint8_t *p)
{
	uint32_t u = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
	return (int32_t)u - (int32_t)((u & 0x800000UL) << 1);
}

void metaPosition(const uint8_t meta[14], gnss_t *g)
{
	g->source = meta[0] >> 4;
	g->station = meta[0] & 0x0F;
	g->validity = meta[1] >> 4;
	g->radius_m = 1U << ((meta[1] >> 1) & 7U);
	g->bearing = ((uint16_t)(meta[1] & 1U) << 8) | meta[2];
	g->lat_ud = (int32_t)((int64_t)s24(&meta[3]) * 90000000LL / 8388607LL);
	g->lon_ud = (int32_t)((int64_t)s24(&meta[6]) * 180000000LL / 8388607LL);
	g->alt_dm = (int32_t)((((uint16_t)meta[9] << 8) | meta[10]) * 5) - 5000;
	g->speed_x2 = ((uint16_t)meta[11] << 4) | (meta[12] >> 4);
}

//6-character Maidenhead locator, e.g. "KO02mf"
void maidenhead(int32_t lat_ud, int32_t lon_ud, char out[7])
{
	int64_t lon = (int64_t)lon_ud + 180000000LL;
	int64_t lat = (int64_t)lat_ud + 90000000LL;

	if (lon < 0) lon = 0;
	if (lon >= 360000000LL) lon = 360000000LL - 1;
	if (lat < 0) lat = 0;
	if (lat >= 180000000LL) lat = 180000000LL - 1;

	out[0] = 'A' + lon / 20000000LL;
	out[1] = 'A' + lat / 10000000LL;
	out[2] = '0' + (lon % 20000000LL) / 2000000LL;
	out[3] = '0' + (lat % 10000000LL) / 1000000LL;
	out[4] = 'a' + (lon % 2000000LL) * 24 / 2000000LL;
	out[5] = 'a' + (lat % 1000000LL) * 24 / 1000000LL;
	out[6] = 0;
}

static void fmtCoord(char *out, uint8_t n, int32_t ud, char pos, char neg)
{
	uint32_t a = (ud < 0) ? -ud : ud;

	snprintf(out, n, "%lu.%05lu%c", (unsigned long)(a / 1000000U),
			(unsigned long)((a % 1000000U) / 10U), (ud < 0) ? neg : pos);
}

void msgAge(const inbox_msg_t *m, char *out, uint8_t size)
{
	if (m->rx_tick == 0)
	{
		out[0] = 0;
		return;
	}

	uint32_t s = (HAL_GetTick() - m->rx_tick) / 1000U;

	if (s < 60)
		snprintf(out, size, "now");
	else if (s < 3600)
		snprintf(out, size, "%lum", (unsigned long)(s / 60));
	else if (s < 86400)
		snprintf(out, size, "%luh", (unsigned long)(s / 3600));
	else
		snprintf(out, size, "%lud", (unsigned long)(s / 86400));
}

//append formatted text
#define ADD(...)	do { if (n < size) n += snprintf(&out[n], size - n, __VA_ARGS__); } while (0)

void msgInfoText(const inbox_msg_t *m, char *out, uint16_t size)
{
	uint16_t n = 0;
	char a[24], b[24];

	out[0] = 0;

	ADD("From: %s\nTo: %s\nCAN: %u\n", m->src, m->dst, TYPE_CAN(m->type));

	msgAge(m, a, sizeof(a));
	if (a[0])
		ADD("Rcvd: %s%s\n", a, strcmp(a, "now") ? " ago" : "");

	ADD("Size: %u B\n", m->len);

	switch (metaKind(m->type, m->meta))
	{
		case META_POSITION:
		{
			gnss_t g;
			metaPosition(m->meta, &g);

			static const char *const station[] = {"Fixed", "Mobile", "Handheld"};

			if (g.validity & M17_GNSS_LAT_LON_VALID)
			{
				char loc[7];
				fmtCoord(a, sizeof(a), g.lat_ud, 'N', 'S');
				fmtCoord(b, sizeof(b), g.lon_ud, 'E', 'W');
				maidenhead(g.lat_ud, g.lon_ud, loc);
				ADD("Position:\n%s\n%s\nLoc: %s\n", a, b, loc);
			}
			else
			{
				ADD("Position: invalid\n");
			}

			if (g.validity & M17_GNSS_ALT_VALID)
				ADD("Alt: %ld m\n", (long)(g.alt_dm / 10));

			if (g.validity & M17_GNSS_SPD_BEARING_VALID)
				ADD("Speed: %u.%u km/h\nHdg: %u deg\n", g.speed_x2 / 2, (g.speed_x2 & 1) ? 5 : 0, g.bearing);

			if (g.validity & M17_GNSS_RADIUS_VALID)
				ADD("Radius: %u m\n", g.radius_m);

			if (g.station < 3)
				ADD("Station: %s\n", station[g.station]);
			else
				ADD("Station: %u\n", g.station);
		}
		break;

		case META_TEXT:
		{
			char raw[15], txt[15];
			memcpy(raw, m->meta, 14);
			raw[14] = 0;
			utf8ToLocal(txt, sizeof(txt), raw);
			ADD("META text:\n%s\n", txt);
		}
		break;

		case META_EXT_CALL:
		{
			decode_callsign_bytes(a, &m->meta[0]);
			ADD("Orig: %s\n", a);

			uint8_t cf2 = 0;
			for (uint8_t i=6; i<12; i++)
				cf2 |= m->meta[i];
			if (cf2)
			{
				decode_callsign_bytes(b, &m->meta[6]);
				ADD("Refl: %s\n", b);
			}
		}
		break;

		case META_ENCRYPTED:
			ADD("Encrypted\n");
		break;

		case META_UNKNOWN:
			ADD("META:\n");
			for (uint8_t i=0; i<14; i++)
				ADD("%02X", m->meta[i]);
			ADD("\n");
		break;

		default:
		break;
	}

	//no trailing newline
	if (n > 0 && n < size && out[n-1] == '\n')
		out[n-1] = 0;
}

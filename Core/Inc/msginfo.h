#ifndef INC_MSGINFO_H_
#define INC_MSGINFO_H_

#include <stdint.h>
#include "inbox.h"

//META field contents, decoded from the LSF TYPE (same rules for stream and packet mode)
typedef enum
{
	META_NONE,			//all zero
	META_TEXT,
	META_POSITION,
	META_EXT_CALL,
	META_ENCRYPTED,		//META holds a nonce/IV
	META_UNKNOWN
} meta_kind_t;

typedef struct
{
	uint8_t source;			//data source
	uint8_t station;		//station type (M17_GNSS_STATION_*)
	uint8_t validity;		//M17_GNSS_*_VALID bits
	int32_t lat_ud;			//latitude, microdegrees
	int32_t lon_ud;			//longitude, microdegrees
	int32_t alt_dm;			//altitude, decimeters
	uint16_t bearing;		//degrees
	uint16_t speed_x2;		//km/h * 2
	uint8_t radius_m;		//position uncertainty, meters
} gnss_t;

meta_kind_t metaKind(uint16_t type, const uint8_t meta[14]);
void metaPosition(const uint8_t meta[14], gnss_t *g);
void maidenhead(int32_t lat_ud, int32_t lon_ud, char out[7]);

//multi-line description of a message (internal charset, '\n' separated)
void msgInfoText(const inbox_msg_t *m, char *out, uint16_t size);

//"5m", "2h", "3d", "" if unknown
void msgAge(const inbox_msg_t *m, char *out, uint8_t size);

#endif /* INC_MSGINFO_H_ */

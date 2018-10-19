/**
* @file ts.h
* @brief Define MPEG2-TS
* @author sage
* @date 2018/08/31
*/

#ifndef __TS_HEADER__
#define __TS_HEADER__

/*------------------------------------------------------------------------------
 Macro
------------------------------------------------------------------------------*/
#define TS_PACKET_SIZE			( 188 )				// Only 188 byte
#define TS_START_IND_BIT		( 0x40 )
#define TS_ADAPTATION_FIELD		( 0x20 )
#define ADAPTATION_FIELD_PCR	( 0x10 )
#define GET_PID(x1,x2)			((((uint16_t)x1&0x1F)<<8)|x2)

#define PCR_NONE				( UINT32_MAX )
#define PCR_CLOCK_EXT			( 27000000 )

#define TS_SYNC_BYTE			( 0x47 )

#define PID_NULL				( 0x1FFF )
#define PID_TOT					( 0x0014 )

#define TABLE_ID_TOT			( 0x73 )

#define GET_PCR( pcr_bin, pcr )		{														\
										pcr = 0;											\
										pcr  = ( pcr_bin[ 0 ] << ( 3 * 8 ) ) & 0xFF000000;	\
										pcr |= ( pcr_bin[ 1 ] << ( 2 * 8 ) ) & 0x00FF0000;	\
										pcr |= ( pcr_bin[ 2 ] << ( 1 * 8 ) ) & 0x0000FF00;	\
										pcr |= ( pcr_bin[ 3 ] << ( 0 * 8 ) ) & 0x000000FF;	\
										pcr <<= 1;											\
										pcr |= ( pcr_bin[ 4 ] >> ( 7 ) ) & 0x01;			\
									}	

#define SET_PCR( pcr_bin, pcr )		{														\
										pcr_bin[ 0 ] = ( pcr >> 25 ) & 0xFF;				\
										pcr_bin[ 1 ] = ( pcr >> 17 ) & 0xFF;				\
										pcr_bin[ 2 ] = ( pcr >>  9 ) & 0xFF;				\
										pcr_bin[ 3 ] = ( pcr >>  1 ) & 0xFF;				\
										if( pcr & 1 ){										\
											pcr_bin[ 4 ] |= 0x80;							\
										}else{												\
											pcr_bin[ 4 ] &= 0x7F;							\
										}													\
									}														\

#define GET_PCR_EXT( pcr_bin, pcr )		{																			\
										uint64_t	b, e;															\
										b  = ( ( ( uint8_t* )pcr_bin )[ 0 ] << ( 3 * 8 ) ) & 0x00000000FF000000;	\
										b |= ( ( ( uint8_t* )pcr_bin )[ 1 ] << ( 2 * 8 ) ) & 0x0000000000FF0000;	\
										b |= ( ( ( uint8_t* )pcr_bin )[ 2 ] << ( 1 * 8 ) ) & 0x000000000000FF00;	\
										b |= ( ( ( uint8_t* )pcr_bin )[ 3 ] << ( 0 * 8 ) ) & 0x00000000000000FF;	\
										b <<= 1;																	\
										b |= ( ( ( uint8_t* )pcr_bin )[ 4 ] >> ( 7 ) ) & 0x0000000000000001;		\
										e  = ( ( ( uint8_t* )pcr_bin )[ 4 ] << ( 8 ) ) & 0x0000000000000100;		\
										e |= ( ( ( uint8_t* )pcr_bin )[ 5 ] << ( 0 ) ) & 0x00000000000000FF;		\
										pcr = b * 300 + e;															\
									}																				\



/*------------------------------------------------------------------------------
 Enum
------------------------------------------------------------------------------*/
typedef enum {
	TS_SCRAMBLE_NONE = 0x00,
	TS_SCRAMBLE_EVEN = 0x02,
	TS_SCRAMBLE_ODD = 0x03,
} TS_SCRAMBLE;

typedef enum {
	TS_ADAPTATION_FIELD_CONTROL_NONE = 0x01,
	TS_ADAPTATION_FIELD_CONTROL_ONLY = 0x02,
	TS_ADAPTATION_FIELD_CONTROL_WITH_PAYLOAD = 0x03,
} TS_ADAPTATION_FIELD_CONTROL;

/*------------------------------------------------------------------------------
 Struct
------------------------------------------------------------------------------*/
typedef struct {
	uint8_t						SyncByte;
	bool						TransportErrorIndicator;
	bool						PayloadUnitStartIndicator;
	bool						TransportPriority;
	uint16_t					Pid;
	TS_SCRAMBLE					TransportScramblingControl;
	TS_ADAPTATION_FIELD_CONTROL	AdaptationFieldControl;
	uint8_t						ContinuityCounter;
	uint64_t					Pcr;
} TS_HEADER;
#endif


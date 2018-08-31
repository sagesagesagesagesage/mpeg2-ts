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
									}														\

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


#endif


/**
* @file base.c
* @brief MPEG-TS Basic.
* @author sage
* @date 2018/09/25
* @details 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>

#include "ts.h"

#define	DEBUG	0
#if _DEBUG
#define DEBUG_PRINT(...)	printf( "DEBUG >" __VA_ARGS__ )
#else
#define DEBUG_PRINT(...)
#endif

struct {
	bool	DumpTsHeader;			// Dump TS Header
} Options;


static	bool			ts_dump( const char* ts_file );
static	void			ts_dump_header( const uint8_t* ts_packet, const uint8_t ts_packet_length );
static	void			show_help( void );

/**
* @brief		Dump TS Packet
* @param[in]	in_filename		Input TS file path
* @return		bool			Result
* @details		Display data of TS packet in hexadecimal.
*/
static	bool			ts_dump( const char* ts_file )
{
	FILE*		ifp = NULL;
	
	uint8_t		i;
	uint8_t		ts_buffer[ TS_PACKET_SIZE ];
	
	bool		result = false;
	
	ifp = fopen( ts_file, "rb" );
	if( ifp ){
		while( TS_PACKET_SIZE == fread( ts_buffer, 1, sizeof( ts_buffer ), ifp ) ){
			if( TS_SYNC_BYTE != ts_buffer[ 0 ] ){
				continue;
			}
			
			if( Options.DumpTsHeader ){
				ts_dump_header( ts_buffer, TS_PACKET_SIZE );
			}
			
			for( i = 0 ; i < TS_PACKET_SIZE ; i++ ){
				printf( "%02X,", ts_buffer[ i ] );
			}
			printf( "\n" );
		}
		
		fclose( ifp );
		result = true;
	}else{
		perror( "Input file open." );
	}

	return result;
}

/**
* @brief		Dump TS Packet header
* @param[in]	ts_packet			TS packet
* @param[in]	ts_packet_length	length of ts_packet
* @return		void
* @details		Display data of TS packet header.
*/
static	void			ts_dump_header( const uint8_t* ts_packet, const uint8_t ts_packet_length )
{
	TS_HEADER		header;

	static	bool	show_header = true;
	
	assert( TS_PACKET_SIZE == ts_packet_length );
	
	header.SyncByte						 = ts_packet[ 0 ];
	header.TransportErrorIndicator		 = ( 0x80 & ts_packet[ 1 ] ) >> 7;
	header.PayloadUnitStartIndicator	 = ( 0x40 & ts_packet[ 1 ] ) >> 6;
	header.TransportPriority			 = ( 0x20 & ts_packet[ 1 ] ) >> 6;
	header.Pid							 = GET_PID( ts_packet[ 1 ], ts_packet[ 2 ]  );
	header.TransportScramblingControl	 = ( ts_packet[ 3 ] & 0xC0 ) >> 6;
	header.AdaptationFieldControl		 = ( ts_packet[ 3 ] & 0x30 ) >> 4;
	header.ContinuityCounter			 = ts_packet[ 3 ] & 0x0F;
	
	if(    (    ( TS_ADAPTATION_FIELD_CONTROL_ONLY == header.AdaptationFieldControl )
			 || ( TS_ADAPTATION_FIELD_CONTROL_WITH_PAYLOAD == header.AdaptationFieldControl ) )
		&& ( 0x10 & ts_packet[ 5 ] ) ){
		GET_PCR_EXT( &ts_packet[ 6 ], header.Pcr );
		
	}else{
		header.Pcr = PCR_NONE;
	}
	
	if( show_header ){
		printf( "Sync byte,Transport Error Indicator,Payload Unit Start Indicator,"\
				"Transport Priority,PID,Transport Scrambling Control,"\
				"Adaptation field control,Continuity counter,PCR,Adaptation field,"\
				"TS Packet raw data\n");
		show_header = false;
	}
	
	printf( "0x%02X,%s,%s,%s,0x%X,%s,%s,%d,", 
			header.SyncByte, 
			( header.TransportErrorIndicator ) ? "NG" : "OK",
			( header.PayloadUnitStartIndicator ) ? "ON" : "OFF",
			( header.TransportPriority ) ? "Higher" : "Normal",
			header.Pid,
			( ( TS_SCRAMBLE_NONE == header.TransportScramblingControl ) 
					? 
						"Not scrambled"
					:
						( ( TS_SCRAMBLE_EVEN == header.TransportScramblingControl ) 
							?
								"Scrambled with even key"
							:
								( ( TS_SCRAMBLE_EVEN == header.TransportScramblingControl ) 
									?
										"Scrambled with even key"
									:
										"Reserved for future use" )
						)
			),
			( ( TS_ADAPTATION_FIELD_CONTROL_NONE == header.AdaptationFieldControl ) 
					? 
						"Payload only"
					:
						( ( TS_ADAPTATION_FIELD_CONTROL_ONLY == header.AdaptationFieldControl ) 
							?
								"Adaptation field only"
							:
								( ( TS_ADAPTATION_FIELD_CONTROL_WITH_PAYLOAD == header.AdaptationFieldControl ) 
									?
										"Adaptation field followed by payload"
									:
										"Reserved for future use" )
						)
			),
			header.ContinuityCounter
	);
	if( PCR_NONE == header.Pcr ){
		printf( "-," );
	}else{
		printf( "%ld,", header.Pcr );
	}
	
	if(    ( PID_NULL == header.Pid )
		|| ( TS_ADAPTATION_FIELD_CONTROL_NONE == header.AdaptationFieldControl ) ){
		printf( "-," );
	}else{
		int i;
		for( i = 0 ; i < ts_packet[ 4 ] ; i++ ){
			printf( "%02X ", ts_packet[ 4 + i ] );
		}
		printf( "," );
	}
}

/**
* @brief		Show help
*/
static	void			show_help( void )
{
	printf( " -i\tInput TS file path.\n" );
	printf( " -H\tDump TS Header\n" );
	printf( " -h\tShow Help.\n" );
}

/**
* @brief		Main
*/
int						main( int args, char* argc[] )
{
	char*				in_filename = NULL;
	char				ch;
	
	memset( &Options, 0, sizeof( Options ) );
	
	while( (ch = getopt( args, argc, "i:Hh") ) != -1 ){
		if( ch == 255 ){
			break;
		}
		switch( ch ){
			case 'i':
				in_filename = optarg;
				break;
			case 'H':
				Options.DumpTsHeader = true;
				break;
			case 'h':
			default:
				show_help();
				return 0;
				break;
		}
	}
	
	if( NULL == in_filename ){
		printf( "Please input IN File. -i filepath \n" );
		return -1;
	}
	
	ts_dump( in_filename );
	
	return 0;
}


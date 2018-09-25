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

#include "ts.h"

#define	DEBUG	0
#if _DEBUG
#define DEBUG_PRINT(...)	printf( "DEBUG >" __VA_ARGS__ )
#else
#define DEBUG_PRINT(...)
#endif


static	bool			ts_dump( const char* ts_file );
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
* @brief		Show help
*/
static	void			show_help( void )
{
	printf( " -i\tInput TS file path.\n" );
	printf( " -h\tShow Help.\n" );
}

/**
* @brief		Main
*/
int						main( int args, char* argc[] )
{
	char*				in_filename = NULL;
	char				ch;
	
	while( (ch = getopt( args, argc, "i:h") ) != -1 ){
		if( ch == 255 ){
			break;
		}
		switch( ch ){
			case 'i':
				in_filename = optarg;
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
	
	printf( "IN File	 = %s\n", in_filename );
	ts_dump( in_filename );
	
	return 0;
}


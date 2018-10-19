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

#define	DEBUG	1
#if DEBUG
#define DEBUG_PRINT(...)	printf( "DEBUG >" __VA_ARGS__ )
#else
#define DEBUG_PRINT(...)
#endif

/**
* @def		BIT_RATE_COUNT_PCR
* @brief	Define the number of PCRs used for calculating the bit rate
*/
#define BIT_RATE_COUNT_PCR		( 1000 )

struct {
	bool		DumpTsHeader;			// Dump TS Header
	bool		CalcTsBitrate;			// Calculate bitrate 
	uint32_t	BitrateCountPcr;
} Options;


static	bool			ts_dump( const char* ts_file );
static	void			ts_dump_header( const uint8_t* ts_packet, const uint8_t ts_packet_length );
static	double			ts_calc_bitrate( const char* ts_file, const uint32_t use_pcr_count );
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
* @brief		Calculate bit rate of TS file
* @param[in]	ts_file			TS file path
* @param[in]	use_pcr_count	Sampling PCR count
* @return		double			bitrate. if error return 0.0
*/
static	double		ts_calc_bitrate( const char* ts_file, const uint32_t use_pcr_count )
{
	FILE*		ifp = NULL;
	
	uint8_t		ts_packet[ TS_PACKET_SIZE ];
	
	uint64_t	total_packet = 0;
	
	uint64_t	start_pcr = PCR_NONE;
	uint64_t	end_pcr = PCR_NONE;
	int			pcr_count = 0;
	double		bitrate = 0.0;
	uint16_t	pcr_pid = PID_NULL;
	
	ifp = fopen( ts_file, "rb" );
	total_packet = 0;
	if( ifp ){
		while( TS_PACKET_SIZE == fread( ts_packet, 1, sizeof( ts_packet ), ifp ) ){
			if( TS_SYNC_BYTE != ts_packet[ 0 ] ){
				break;
			}
			
			if( 0 < pcr_count ){
				total_packet++;
			}else{
				total_packet = 0;
			}
			
			if( ts_packet[ 3 ] & TS_ADAPTATION_FIELD ){			// Is adaptaion_file ON ?
				if( ts_packet[ 5 ] & ADAPTATION_FIELD_PCR ){	// PCR Flag ?
					if( PID_NULL == pcr_pid ){
						pcr_pid = GET_PID( ts_packet[ 1 ], ts_packet[ 2 ] );
					}
					
					if( pcr_pid != GET_PID( ts_packet[ 1 ], ts_packet[ 2 ] ) ){
						continue;
					}
					
					if( 0 == pcr_count ){
						GET_PCR_EXT( &ts_packet[ 6 ], start_pcr );
						DEBUG_PRINT( "Start PCR = %lu\n", start_pcr );
						pcr_count++;
					}else if( 0 < pcr_count ){
						GET_PCR_EXT( &ts_packet[ 6 ], end_pcr );
						
						if( start_pcr > end_pcr ){
							DEBUG_PRINT( "PCR RESET %lu => %lu\n", start_pcr, end_pcr );
							pcr_count = 0;
							total_packet = 0;
							continue;
						}
						
						pcr_count++;
						
						if( use_pcr_count < pcr_count ){
							break;
						}
					}
				}
			}
		}
		fclose( ifp );
		
		if(    ( PCR_NONE != start_pcr )
			&& ( PCR_NONE != end_pcr ) ){
			DEBUG_PRINT("End   PCR = %lu / Total = %lu\n", end_pcr, total_packet );
			bitrate = ( total_packet * TS_PACKET_SIZE * 8 ) / ( ( end_pcr - start_pcr ) / ( double )PCR_CLOCK_EXT );
			DEBUG_PRINT( "TS Bitrate = %lf\n", bitrate );
		}
	}
	
	return bitrate;
}


/**
* @brief		Show help
*/
static	void			show_help( void )
{
	printf( " -i\tInput TS file path.\n" );
	printf( " -H\tDump TS Header\n" );
	printf( " -b\tCalculate bit rate of TS file\n" );
	printf( " -c\tCalculate bit rate of TS file. Use packet number(32bit, default = %d).\n", BIT_RATE_COUNT_PCR );
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
	Options.BitrateCountPcr = BIT_RATE_COUNT_PCR;
	
	while( (ch = getopt( args, argc, "i:Hbc:h") ) != -1 ){
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
			case 'b':
				Options.CalcTsBitrate = true;
				break;
			case 'c':
				Options.BitrateCountPcr = atol( optarg );
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
	
	if( Options.CalcTsBitrate ){
		printf( "%s Bitrate = %f bps.\n", in_filename, ts_calc_bitrate( in_filename, Options.BitrateCountPcr ) );
	}else{
		ts_dump( in_filename );
	}
	
	return 0;
}


/**
* @file ts_tot_spliter.c
* @brief Divide TS file by TOT time.
* @author sage
* @date 2018/08/31
* @details This program splits the TS file using TOT time.
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



/**
* @def		BIT_RATE_COUNT_PCR
* @brief	Define the number of PCRs used for calculating the bit rate
*/
#define BIT_RATE_COUNT_PCR		( 100 )

typedef struct{
	uint64_t		DateTime;
	uint16_t		MJD;
	uint32_t		Time;
} ST_DATETIME;

inline	uint8_t			bcd_to_dec( uint8_t bcd );
static	bool			ts_calc_bitrate( const char* ts_file );
static	bool			ts_split( const char* in_filename, const char* out_filename, ST_DATETIME* start, ST_DATETIME* end );
static	bool			get_datetime( char* str_datetime, ST_DATETIME* st_datetime );
static	void			show_help( void );

/**
* @brief		BCD => DECIMAL converter
* @param[in]	bcd			BCD
* @return		uint8_t		Converted decimal
* @details		This function convert DCB to decimal.\n
* 				Example : 0x12 => 12
*/
inline	uint8_t		bcd_to_dec( uint8_t bcd )
{
	uint8_t result = bcd & 0x0F;
	
	result += ( ( bcd & 0xF0 ) >> 4 ) * 10;
	
	return result;
}

/**
* @brief		Split ts file.
* @param[in]	ts_file			TS file path
* @param[in]	out_filename	Output TS file path
* @return		double			bitrate. if error return 0.0
*/
static	bool		ts_calc_bitrate( const char* ts_file )
{
	FILE*		ifp = NULL;
	
	uint8_t		ts_buffer[ TS_PACKET_SIZE ];
	
	uint64_t	total_packet = 0;
	
	uint64_t	start_pcr;
	uint64_t	end_pcr;
	int			pcr_count = 0;
	double		bitrate = 0.0;
	uint16_t	pcr_pid = PID_NULL;
	
	ifp = fopen( ts_file, "rb" );
	total_packet = 0;
	if( ifp ){
		while( TS_PACKET_SIZE == fread( ts_buffer, 1, sizeof( ts_buffer ), ifp ) ){
			if( TS_SYNC_BYTE != ts_buffer[ 0 ] ){
				break;
			}
			
			if( 0 < pcr_count ){
				total_packet++;
			}else{
				total_packet = 0;
			}
			
			if( ts_buffer[ 3 ] & TS_ADAPTATION_FIELD ){			// Is adaptaion_file ON ?
				if( ts_buffer[ 5 ] & ADAPTATION_FIELD_PCR ){	// PCR Flag ?
					if( PID_NULL == pcr_pid ){
						pcr_pid = GET_PID( ts_buffer[ 1 ], ts_buffer[ 2 ] );
					}
					
					if( pcr_pid != GET_PID( ts_buffer[ 1 ], ts_buffer[ 2 ] ) ){
						continue;
					}
					
					if( 0 == pcr_count ){
						start_pcr  = ( ( ( uint64_t )ts_buffer[ 6 ] ) << 25 ) & 0x1FE000000;
						start_pcr |= ( ( ( uint64_t )ts_buffer[ 7 ] ) << 17 ) & 0x001FE0000;
						start_pcr |= ( ( ( uint64_t )ts_buffer[ 8 ] ) << 9  ) & 0x00001FE00;
						start_pcr |= ( ( ( uint64_t )ts_buffer[ 9 ] ) << 1 )  & 0x0000001FE;
						start_pcr |= ( ( ( ( uint64_t )ts_buffer[ 10 ] ) & 0x80 ) >> 7 ) & 0x00000001;
						DEBUG_PRINT( "Start PCR = %lu\n", start_pcr );
						pcr_count++;
					}else if( 0 < pcr_count ){
						end_pcr  = ( ( ( uint64_t )ts_buffer[ 6 ] ) << 25 ) & 0x1FE000000;
						end_pcr |= ( ( ( uint64_t )ts_buffer[ 7 ] ) << 17 ) & 0x001FE0000;
						end_pcr |= ( ( ( uint64_t )ts_buffer[ 8 ] ) << 9  ) & 0x00001FE00;
						end_pcr |= ( ( ( uint64_t )ts_buffer[ 9 ] ) << 1 )  & 0x0000001FE;
						end_pcr |= ( ( ( ( uint64_t )ts_buffer[ 10 ] ) & 0x80 ) >> 7 ) & 0x00000001;
						
						if( start_pcr > end_pcr ){
							DEBUG_PRINT( "PCR RESET %lu => %lu\n", start_pcr, end_pcr );
							pcr_count = 0;
							total_packet = 0;
							continue;
						}
						
						pcr_count++;
						
						if( BIT_RATE_COUNT_PCR < pcr_count ){
							DEBUG_PRINT("End   PCR = %lu / Total = %lu\n", end_pcr, total_packet );
							bitrate = ( total_packet * TS_PACKET_SIZE * 8 ) / ( ( end_pcr - start_pcr ) / 90000.0 );
							DEBUG_PRINT( "TS Bitrate = %lf\n", bitrate );
							break;
						}
					}
				}
			}
		}
		fclose( ifp );
	}
	
	return bitrate;
}

/**
* @brief		Split ts file.
* @param[in]	in_filename		Input TS file path
* @param[in]	out_filename	Output TS file path
* @param[in]	start			Start datetime of output file
* @param[in]	end				End datetime of output file
* @return		bool			Result
* @details		Divide the file according to the following procedure
*				1) Calculate the bit rate of the input file.\n
*				PCR is used for calculation.\n
*				2) Seek from the calculated bit rate to the approximate start position of the input file.\n
*				3) If the seek has succeeded, continue processing.\n
*				If it fails, the processing is terminated.\n
*				4) Write the TS packet from the input file to the output file, and terminate the process in the case of discovering the time of power of the TOT.\n
*				Also, even if it is not the end time, the process ends when the input file ends.\n
*/
static	bool		ts_split( const char* in_filename, const char* out_filename, ST_DATETIME* start, ST_DATETIME* end )
{
	FILE*		ifp = NULL;
	FILE*		ofp = NULL;
	
	uint8_t		ts_buffer[ TS_PACKET_SIZE ];
	
	uint64_t	total_packet = 0;
	
	bool		file_write_flag = false;
	bool		file_seeked = false;
	
	double		bitrate = 0;
	
	bool		find_tot = false;
	
	bool		result = true;
	
	bitrate = ts_calc_bitrate( in_filename );
	if( 0.0 >= bitrate ){
		perror( "Error calc bitrate.\n" );
		return false;
	}
	
	ifp = fopen( in_filename, "rb" );
	if( ifp ){
		ofp = fopen( out_filename, "wb" );
		
		if( ofp ){
			while( TS_PACKET_SIZE == fread( ts_buffer, 1, sizeof( ts_buffer ), ifp ) ){
				if( TS_SYNC_BYTE != ts_buffer[ 0 ] ){
					result = true;
					break;
				}
				
				if( PID_TOT == GET_PID( ts_buffer[ 1 ], ts_buffer[ 2 ] ) ){
					int		payload_start_pos;
					
					if( ts_buffer[ 3 ] & TS_ADAPTATION_FIELD ){
						payload_start_pos = 4 + 1 + ts_buffer[ 4 ];
					}else{
						payload_start_pos = 5;
					}
					
					if( TABLE_ID_TOT == ts_buffer[ payload_start_pos ] ){
						uint16_t	tot_mjd;
						uint32_t	tot_time;
						uint64_t	tot_datetime;
						uint32_t	hour, min, sec;
						
						tot_mjd = ( ( ( uint16_t )ts_buffer[ payload_start_pos + 3 ] << 8 ) & 0xFF00 ) | ( ( ( uint16_t )ts_buffer[ payload_start_pos + 4 ] ) & 0x00FF );
						
						hour = bcd_to_dec( ts_buffer[ payload_start_pos + 5 ] );
						min  = bcd_to_dec( ts_buffer[ payload_start_pos + 6 ] );
						sec  = bcd_to_dec( ts_buffer[ payload_start_pos + 7 ] );
						tot_time = hour * 3600 + min * 60 + sec;
						
						tot_datetime = ( ( uint64_t )tot_mjd ) << 32 | ( uint64_t )tot_time;
						if( file_seeked ){
							DEBUG_PRINT("MJD = %d  Time = %u Datatime = %ld Time = %02X:%02X:%02X\n", tot_mjd, tot_time, tot_datetime, ts_buffer[ payload_start_pos + 5 ], ts_buffer[ payload_start_pos + 6 ], ts_buffer[ payload_start_pos + 7 ]);
							file_seeked = false;
						}
						if( start->DateTime <= tot_datetime && tot_datetime <= end->DateTime ){
							if( !file_write_flag ){
								DEBUG_PRINT( "Split start MJD %u  Time %u Datetime = %lu\n", tot_mjd, tot_time, tot_datetime );
							}
							file_write_flag = true;
							find_tot = true;
						}else{
							if( file_write_flag ){
								DEBUG_PRINT( "Split end MJD %u  Time %u Datetime = %lu\n", tot_mjd, tot_time, tot_datetime );
								break;
							}
							file_write_flag = false;
							
							if( !find_tot ){
								DEBUG_PRINT( "First TOT Packet\n" );
								DEBUG_PRINT("MJD = %d  Time = %u Datatime = %ld Time = %02X:%02X:%02X\n", tot_mjd, tot_time, tot_datetime, ts_buffer[ payload_start_pos + 5 ], ts_buffer[ payload_start_pos + 6 ], ts_buffer[ payload_start_pos + 7 ]);
								if( tot_datetime < start->DateTime ){
									uint64_t	diff_second;
									uint64_t	seek_byte;
									if( tot_time < start->Time ){
										diff_second = start->Time - tot_time;
										diff_second += ( start->MJD - tot_mjd ) * 24 * 3600;
									}else{
										diff_second = 24 * 3600 - ( tot_time - start->Time );
										diff_second += ( ( start->MJD - 1 ) - tot_mjd ) * 24 * 3600;
									}
									seek_byte = ( ( uint64_t )( ( bitrate / 8 * diff_second * 0.999 ) / TS_PACKET_SIZE ) ) * TS_PACKET_SIZE;
									
									DEBUG_PRINT( "Diff Second = %lu / Seek_Byte = %lu\n", diff_second, seek_byte );
									fseeko( ifp, seek_byte, SEEK_SET );
									file_seeked = true;
								}
								find_tot = true;
							}
						}
					}
				}
				
				if( file_write_flag ){
					fwrite( ts_buffer, 1, sizeof( ts_buffer ), ofp );
					total_packet++;
				}
			}
			
			fclose( ofp );
		}else{
			printf( "%s()[%d] IN File open error. [%s]", __func__, __LINE__, out_filename );
			result = false;
		}
		
		fclose( ifp );
	}else{
		printf( "%s()[%d] OUT File open error. [%s]", __func__, __LINE__, in_filename );
	}
	
	printf( "Total read TS packet = %ld\n", total_packet );
	
	return result;
}

/**
* @brief		Convert DateTime   String => ST_DATETIME
* @param[in]	str_datetime	String datetime
* @param[out]	st_datetime		Struct datetime
* @return		bool			Result
*/
static	bool			get_datetime( char* str_datetime, ST_DATETIME* st_datetime )
{
	int		year, month, day;
	int		hour, min, sec;
	
	if( EOF == sscanf( str_datetime, "%d/%d/%d-%d:%d:%d", &year, &month, &day, &hour, &min, &sec ) ){
		return false;
	}
	
	// Calc MJD
	if( ( month == 1) || ( month == 2 ) ){
		year = year - 1;
		month = month + 12;
	}
	st_datetime->MJD = floor( 365.25 * year ) + ( year / 400 ) - ( year / 100 ) + floor( 30.59 * ( month - 2 ) ) + day - 678912;
	st_datetime->Time = hour * 3600 + min * 60 + sec;
	
	st_datetime->DateTime = ( ( uint64_t )st_datetime->MJD ) << 32 | ( uint64_t )st_datetime->Time;
	
	DEBUG_PRINT( "\n\t-----------\n" );
	DEBUG_PRINT( "\tMJD  %u <= %04d/%02d/%02d\n", st_datetime->MJD, year, month, day );
	DEBUG_PRINT( "\tTIME %u <= %02d:%02d:%02d\n", st_datetime->Time, hour, min, sec );
	DEBUG_PRINT( "\tDate TIME %lu\n", st_datetime->DateTime );
	DEBUG_PRINT( "\t-----------\n" );
	
	return true;
}

/**
* @brief		Show help
*/
static	void			show_help( void )
{
	printf( " -i\tInput TS file path.\n" );
	printf( " -o\tOutput TS file path.\n" );
	printf( " -s\tStart Date time.(exp 2018/01/02-09:00:00)\n" );
	printf( " -e\tEnd Date time.(exp 2018/01/02-09:15:00)\n" );
	printf( " -h\tShow Help.\n" );
}
/**
* @brief		Main
*/
int						main( int args, char* argc[] )
{
	char*				in_filename = NULL;
	char*				out_filename = NULL;
	
	char*				start_datetime = NULL;
	char*				end_datetime = NULL;
	
	ST_DATETIME			st_start;
	ST_DATETIME			st_end;
	
	char				ch;
	
	while( (ch = getopt( args, argc, "i:o:s:e:h") ) != -1 ){
		if( ch == 255 ){
			break;
		}
		switch( ch ){
			case 'i':
				in_filename = optarg;
				break;
			case 'o':
				out_filename = optarg;
				break;
			case 's':
				start_datetime = optarg;
				break;
			case 'e':
				end_datetime = optarg;
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
	}
	if( NULL == out_filename ){
		printf( "Please input Out File. -o filepath \n" );
	}
	if( NULL == start_datetime && NULL == start_datetime ){
		printf( "Please input Start Datetime. -s starttime \n" );
		printf( "Please input End Datetime. -s starttime \n" );
	}
	if( 	NULL == in_filename 
		||	NULL == out_filename
		||	( NULL == start_datetime && NULL == start_datetime ) ){
		return -1;
	}
	
	printf( "IN File	 = %s\n", in_filename );
	printf( "OUT File	 = %s\n", out_filename );
	if( NULL == start_datetime ){
		DEBUG_PRINT( "Start		 = Head\n" );
		st_start.MJD = 0;
		st_start.Time = 0;
		st_start.DateTime = 0xFFFFFFFFFFFFFFFF;
	}else{
		DEBUG_PRINT( "Start		 = %s\n", start_datetime );
		if( false == get_datetime( start_datetime, &st_start ) ){
			printf( "Start datetime format error.\n" );
			return -1;
		}
	}
	if( NULL == end_datetime ){
		DEBUG_PRINT( "End		 = Tail\n" );
		st_end.MJD = 0xFFFF;
		st_end.Time = 0xFFFFFFFF;
		st_end.DateTime = 0xFFFFFFFFFFFFFFFF;
	}else{
		DEBUG_PRINT( "End		 = %s\n", end_datetime );
		if( false == get_datetime( end_datetime, &st_end ) ){
			printf( "End datetime format error.\n" );
			return -1;
		}
	}
	
	if( !ts_split( in_filename, out_filename, &st_start, &st_end ) ){
		perror( "Split is error.\n" );
	}
	
	return 0;
}


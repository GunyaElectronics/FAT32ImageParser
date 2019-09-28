/*
 * fat32_parser.h
 *
 *  Created on: 06.07.2019
 *      Author: Maxim Hunko
 */
#ifndef _FAT32_PARSER_H_
#define _FAT32_PARSER_H_

/*
	Used links:
	http://blog.hakzone.info/posts-and-articles/bios/analysing-the-master-boot-record-mbr-with-a-hex-editor-hex-workshop/
	https://en.wikipedia.org/wiki/Master_boot_record
	http://elm-chan.org/docs/fat_e.html
	http://asf.atmel.com/docs/latest/samg/html/group__thirdparty__fatfs__group.html
	http://www.cs.fsu.edu/~cop4610t/lectures/project3/Week11/Slides_week11.pdf
	https://habr.com/ru/post/353024/
*/

#define BS_jmpBoot			0
#define BS_OEMName			3
#define BPB_BytsPerSec		11
#define BPB_SecPerClus		13
#define BPB_RsvdSecCnt		14
#define BPB_NumFATs			16
#define BPB_RootEntCnt		17
#define BPB_TotSec16		19
#define BPB_Media			21
#define BPB_FATSz16			22
#define BPB_SecPerTrk		24
#define BPB_NumHeads		26
#define BPB_HiddSec			28
#define BPB_TotSec32		32
#define BS_55AA				510

#define BS_DrvNum			36
#define BS_BootSig			38
#define BS_VolID			39
#define BS_VolLab			43
#define BS_FilSysType		54

#define BPB_FATSz32			36
#define BPB_ExtFlags		40
#define BPB_FSVer			42
#define BPB_RootClus		44
#define BPB_FSInfo			48
#define BPB_BkBootSec		50
#define BS_DrvNum32			64
#define BS_BootSig32		66
#define BS_VolID32			67
#define BS_VolLab32			71
#define BS_FilSysType32		82


#define DEBUG_MBR	(1)
#if DEBUG_MBR > 0
#define DEBUG__MBR(...)    do{ printf("DEBUG: "); printf(__VA_ARGS__); printf("\r\n"); }while(0)
#else
#define DEBUG__MBR(...)
#endif

#define DEBUG_INFO	(1)
#if DEBUG_INFO > 0
#define DEBUG_PRINTF(...)    do{ printf("DEBUG: "); printf(__VA_ARGS__); printf("\r\n"); }while(0)
#else
#define DEBUG_PRINTF(...)
#endif

#define LOG_IN_FILE  (1)

#define MASTER_BOOT_RECORD_SIZE 	((uint32_t)512)

uint16_t LD_WORD(uint8_t* ptr);
uint32_t LD_DWORD(uint8_t* ptr);

#define RECURSION_MAX		10
#define	DIR_Name			0
#define	DIR_Attr			11
#define	DIR_NTres			12
#define	DIR_CrtTime			14
#define	DIR_CrtDate			16
#define	DIR_FstClusHI		20
#define	DIR_WrtTime			22
#define	DIR_WrtDate			24
#define	DIR_FstClusLO		26
#define	DIR_FileSize		28
#define	LDIR_Ord			0
#define	LDIR_Attr			11
#define	LDIR_Type			12
#define	LDIR_Chksum			13
#define	LDIR_FstClusLO		26

/* File attribute bits for directory entry */

#define	AM_RDO	0x01	/* Read only */
#define	AM_HID	0x02	/* Hidden */
#define	AM_SYS	0x04	/* System */
#define	AM_VOL	0x08	/* Volume label */
#define AM_LFN	0x0F	/* LFN entry */
#define AM_DIR	0x10	/* Directory */
#define AM_ARC	0x20	/* Archive */
#define AM_MASK	0x3F	/* Mask of defined bits */

typedef struct{
	uint8_t jmp_boot[3];
	char oem_name[9];
	uint16_t bytes_per_sec;
	uint8_t sec_per_clus;
	uint16_t rsvd_sec_cnt;
	uint8_t num_fats;
	uint16_t root_ent_cnt;
	uint32_t tot_sec;
	uint8_t media;
	uint32_t fat_sz;
	uint16_t sec_per_trk;
	uint16_t num_heads;
	uint32_t hidd_sec;
	uint32_t root_clus;
	uint32_t root_dir_sectors;
	uint32_t first_data_sector;
	uint32_t first_fat_sector;
	uint32_t data_sectors;
	uint32_t total_clusters;
}MBR_Record_t;

typedef struct{
	uint8_t name[11];
	uint8_t atr;
	uint8_t ntres;
	uint8_t crt_tenth;
	uint16_t crt_time;
	uint16_t crt_date;
	uint16_t last_act_date;
	uint16_t fst_clust_hi;
	uint16_t write_time;
	uint16_t write_date;
	uint16_t fst_clus_lo;
	uint32_t file_size;
}DIR_Record_t;

typedef struct{
	uint8_t ord;
	uint8_t name1[10];
	uint8_t atr;
	uint8_t type;
	uint8_t crc;
	uint8_t name2[12];
	uint16_t fst_clust_lo;
	uint8_t name3[4];
} __attribute((packed)) LFN_Record_t;

int  search_mbr(FILE *fp, MBR_Record_t *p_mbr);
void parse_folder_and_print(FILE *fp, MBR_Record_t *p_mbr, uint32_t lba, char *name);
extern uint8_t *cluster_data;
void set_cluster_sz(uint32_t size);

#endif //_FAT32_PARSER_H_

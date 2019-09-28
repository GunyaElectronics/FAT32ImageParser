/*
 * fat32_parser.c
 *
 *  Created on: 06.07.2019
 *      Author: Maxim Hunko
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "fat32_parser.h"

uint8_t *cluster_data = NULL; //pointer to cluster data
uint32_t cluster_sz = 0;	  //cluster size
uint32_t fat_offset = 0;	  //FAT table offset in bytes
uint32_t current_cluster = 0; //current cluster number
FILE *logfp;				  //log file pointer

void set_cluster_sz(uint32_t size) { cluster_sz = size; }

void read_cluster(FILE *fp, uint32_t lba, MBR_Record_t *p_mbr, uint8_t *p_data)
{
	fseek(fp, (p_mbr->first_data_sector * p_mbr->bytes_per_sec) + ((lba - 2)*cluster_sz), SEEK_SET);
	fread(p_data, 1, p_mbr->bytes_per_sec * p_mbr->sec_per_clus, fp);
}

static uint8_t create_sum (const DIR_Record_t *entry)
{
    int i;
    uint8_t sum;

    for (i = sum = 0; i < 11; i++) { /* Calculate sum of DIR_Name[] field */
        sum = (sum >> 1) + (sum << 7) + entry->name[i];
    }
    return sum;
}

static void print_item_name(char *name, int folder, uint8_t level)
{
	char *format_str = malloc(sizeof(char) * ((level * 3) + 1));
	
	if(format_str)
	{
		memset(format_str, '\0', (3*level)+1);
		memset(format_str, ' ', (3*level));
					
		if(folder)
		{
			printf("|%s|\r\n", format_str);
			#if LOG_IN_FILE
			fprintf(logfp,"|%s|\r\n", format_str);
			#endif
		}
				
		memset(&format_str[3*(level-1)], ' ', (3));
		
		printf("|%s%s\r\n", format_str, name);
		#if LOG_IN_FILE
		fprintf(logfp,"|%s%s\r\n", format_str, name);
		#endif
		free(format_str);
	}
}

#define  FOLDER_CNT 1
#define  FILE_CNT	0

uint32_t get_item_cnt(uint8_t type, FILE *fp, MBR_Record_t *p_mbr)
{
	uint8_t *next_cluster_data = NULL;
	uint8_t *p_items = cluster_data;
	uint8_t *p_end_cluster = cluster_data + cluster_sz;
	uint32_t cnt = 0;
	uint32_t next_clust_num = current_cluster;
	uint32_t cluster_num_changed = 0;
	
	do
	{		
		if(p_items[DIR_Attr] & AM_VOL || p_items[DIR_Attr] & AM_LFN)
		{
			p_items += DIR_FileSize + sizeof(uint32_t); //switch to next item			
			continue;
		}
		
		if(type == FOLDER_CNT && (p_items[DIR_Attr] & AM_DIR) != 0)
		{
			if(p_items[0] == 0x2E)
			{
				p_items += DIR_FileSize + sizeof(uint32_t); //switch to next item				
				continue;
			}
			
			cnt++;
		}
		else if(type == FILE_CNT && (p_items[DIR_Attr] & AM_DIR) == 0 && ( isupper(p_items[DIR_Name]) || isdigit(p_items[DIR_Name])) )
			cnt++;
			
		p_items += DIR_FileSize + sizeof(uint32_t); //switch to next item
		
		if(p_items >= p_end_cluster)
		{
			if(next_cluster_data == NULL)
				next_cluster_data = malloc(cluster_sz);
			
			fseek(fp, fat_offset + (next_clust_num * sizeof(uint32_t)), SEEK_SET);
			
			cluster_num_changed = next_clust_num;
			
			fread(&next_clust_num, 1, sizeof(uint32_t), fp);
			
			if(next_clust_num == cluster_num_changed)
				break; //end of folder
			//read this cluster
			read_cluster(fp, next_clust_num, p_mbr, next_cluster_data);
			
			p_items = next_cluster_data;
			p_end_cluster = next_cluster_data + cluster_sz;
		}
		
	}while(p_items[DIR_Attr] != 0 && p_items[DIR_Attr] != 0xFF);
	
	if(next_cluster_data)
		free(next_cluster_data);
		
	return cnt;
}

uint32_t get_count_of_folder(FILE *fp, MBR_Record_t *p_mbr)
{	
	return get_item_cnt(FOLDER_CNT, fp, p_mbr);
}

uint32_t get_count_of_file(FILE *fp, MBR_Record_t *p_mbr)
{	
	return get_item_cnt(FILE_CNT, fp, p_mbr);
}

uint32_t get_item_lba(uint8_t* item)
{
	return ((uint32_t)LD_WORD(item+DIR_FstClusHI) << 16) | LD_WORD(item+DIR_FstClusLO);
}

char folder_name[256];
char file_name[256];

void get_item_name(char *name, uint8_t type, uint8_t *item)
{
	char short_name[13];
	char* pn = short_name;
	
	memset(short_name, '\0', sizeof(short_name));
	
	int i;
	int ln = type ? 11 : 9;
	for(i = 0; i<ln && item[i] != ' '; i++)
	{
		*pn = item[i];
		*pn++;
	}
	
	if(!type)
	{
		*pn++ = '.';
		if(item[8] != ' ') *pn++ = item[8];
		if(item[9] != ' ') *pn++ = item[9];
		if(item[10] != ' ') *pn++ = item[10];
	}
	
	int lfn_cnt = 0;
	//search all LFN items	
	do
	{
		item -= 32;
		lfn_cnt++;
	}while((item[LDIR_Ord] & 0x40) == 0);
	
	pn = name;
	
	item += 32 * (lfn_cnt - 1);
	
	memset(name, '\0', 256);
	
	do
	{
		int i;
		for(i=0;i<10;i+=2)
		{
			if(item[1+i] != 0xFF)
				*pn++ = item[1+i];
			else
				return;
		}
		for(i=0;i<12;i+=2)
		{
			if(item[14+i] != 0xFF)
				*pn++ = item[14+i];
			else
				return;
		}
		for(i=0;i<4;i+=2)
		{
			if(item[28+i] != 0xFF)
				*pn++ = item[28+i];
			else
				return;
		}
		
		item -= 32;
	}while(lfn_cnt--);
}

uint8_t* get_item_by_index(int index, uint8_t type, uint8_t* cl_pointer, FILE *fp, MBR_Record_t *p_mbr)
{
	uint8_t* result = NULL;
	uint8_t *p_items = cl_pointer;
	uint8_t *p_end_cluster = cl_pointer + cluster_sz;
	uint32_t next_clust_num = current_cluster;
	uint32_t cnt = 0;
	
	do
	{		
		if(p_items[DIR_Attr] & AM_VOL || p_items[DIR_Attr] & AM_LFN)
		{
			p_items += DIR_FileSize + sizeof(uint32_t); //switch to next item
			
			continue;
		}
		
		if(type == FOLDER_CNT && (p_items[DIR_Attr] & AM_DIR) != 0)
		{
			if(p_items[0] == 0x2E)
			{
				p_items += DIR_FileSize + sizeof(uint32_t); //switch to next item
				
				continue;
			}
			
			if(cnt == index)
			{
				result = p_items;
				break;
			}
			
			cnt++;
		}
		else if(type == FILE_CNT && (p_items[DIR_Attr] & AM_DIR) == 0 && ( isupper(p_items[DIR_Name]) || isdigit(p_items[DIR_Name])) )
		{
			if(cnt == index)
			{
				result = p_items;
				break;
			}
			
			cnt++;
		}
			
		p_items += DIR_FileSize + sizeof(uint32_t); //switch to next item
		
		if(p_items >= p_end_cluster)
		{							
			fseek(fp, fat_offset + (next_clust_num * sizeof(uint32_t)), SEEK_SET);
			fread(&next_clust_num, 1, sizeof(uint32_t), fp);
			
			//read this cluster
			read_cluster(fp, next_clust_num, p_mbr, cl_pointer);
			
			p_items = cl_pointer;
			p_end_cluster = cl_pointer + cluster_sz;
		}
	}while(p_items[DIR_Attr] != 0 && p_items[DIR_Attr] != 0xFF);
	
	return result;
}

void parse_folder_and_print(FILE *fp, MBR_Record_t *p_mbr, uint32_t lba, char *name)
{
	static uint8_t level = 0;
	current_cluster = lba;
	
	if(name)
		print_item_name(name, 1, level);
		
	if(level >= RECURSION_MAX)
		return;
	
	if(cluster_data == NULL)
		return;
		
	level++;
	
	read_cluster(fp, lba, p_mbr, cluster_data);
	
	uint32_t file_cnt = get_count_of_file(fp, p_mbr);
		
	uint8_t* cluster_tmp = malloc(cluster_sz);
	
	if(file_cnt)
	{
		int i;
		for(i = 0; file_cnt; file_cnt--, i++)
		{
			//read the cluster again after recursion
			read_cluster(fp, lba, p_mbr, cluster_tmp);
			
			uint8_t* item = get_item_by_index(i, 0, cluster_tmp, fp, p_mbr);
			
			get_item_name(file_name, 0, item);
			
			print_item_name(file_name, 0, level);
		}
	}
	
	uint32_t folder_cnt = get_count_of_folder(fp, p_mbr);
	
	if(folder_cnt)
	{
		int i;
		for(i = 0; folder_cnt; folder_cnt--, i++)
		{
			//read the cluster again after recursion
			read_cluster(fp, lba, p_mbr, cluster_tmp);
			
			uint8_t* item = get_item_by_index(i, 1, cluster_tmp, fp, p_mbr);
			
			uint32_t folder_lba = get_item_lba(item);
			get_item_name(folder_name, 1, item);
			
			parse_folder_and_print(fp, p_mbr, folder_lba, folder_name);
		}		
	}
			
	if(cluster_tmp)
		free(cluster_tmp);
			
	level--;
#if LOG_IN_FILE
	if(level == 0)
		fclose(logfp);
#endif	
}

int search_mbr(FILE *fp, MBR_Record_t *p_mbr)
{
	int result = 1;

#if LOG_IN_FILE
	logfp = fopen("log.txt", "a+t");
#endif

	uint8_t *buffer = malloc(MASTER_BOOT_RECORD_SIZE);
		
	if(buffer != NULL)
	{			
		int res = fread(buffer, 1, MASTER_BOOT_RECORD_SIZE, fp);
			
		if(res == MASTER_BOOT_RECORD_SIZE)
		{
			p_mbr->fat_sz = LD_WORD(buffer+BPB_FATSz16);
			if (!p_mbr->fat_sz) p_mbr->fat_sz = LD_DWORD(buffer+BPB_FATSz32);				
			p_mbr->num_fats = buffer[BPB_NumFATs];	
				
			p_mbr->tot_sec = LD_WORD(buffer+BPB_TotSec16);				/* Number of sectors on the file system */
			if (!p_mbr->tot_sec) p_mbr->tot_sec = LD_DWORD(buffer+BPB_TotSec32);
			
			p_mbr->root_ent_cnt = LD_WORD(buffer+BPB_RootEntCnt);
			p_mbr->bytes_per_sec = LD_WORD(buffer+BPB_BytsPerSec);
			p_mbr->sec_per_clus = buffer[BPB_SecPerClus];		
			p_mbr->rsvd_sec_cnt = LD_WORD(buffer+BPB_RsvdSecCnt);
			p_mbr->root_clus = LD_DWORD(buffer+BPB_RootClus);
			
			DEBUG__MBR("FAT size           %i", p_mbr->fat_sz);
			DEBUG__MBR("Num FATs           %i", p_mbr->num_fats);
			DEBUG__MBR("Total sectors      %i", p_mbr->tot_sec);
			DEBUG__MBR("Root ent counter   %i", p_mbr->root_ent_cnt);
			DEBUG__MBR("Bytes per sector   %i", p_mbr->bytes_per_sec);
			DEBUG__MBR("Sector per clust   %i", p_mbr->sec_per_clus);
			DEBUG__MBR("Reserved sec count %i", p_mbr->rsvd_sec_cnt);
			DEBUG__MBR("Root cluster       %i\r\n", p_mbr->root_clus);
			
			//This calculation will round up. 32 is the size of a FAT directory in bytes. 
			p_mbr->root_dir_sectors = ((p_mbr->root_ent_cnt * 32) + (p_mbr->bytes_per_sec - 1)) / p_mbr->bytes_per_sec;
			
			//The first data sector (that is, the first sector in which directories and files may be stored):
			p_mbr->first_data_sector = p_mbr->rsvd_sec_cnt + (p_mbr->num_fats * p_mbr->fat_sz) + p_mbr->root_dir_sectors;
			
			p_mbr->first_fat_sector = p_mbr->rsvd_sec_cnt;
			
			fat_offset = p_mbr->first_fat_sector * p_mbr->bytes_per_sec;
			
			p_mbr->data_sectors = p_mbr->tot_sec - (p_mbr->rsvd_sec_cnt + (p_mbr->num_fats * p_mbr->fat_sz) + p_mbr->root_dir_sectors);
			
			p_mbr->total_clusters = p_mbr->data_sectors / p_mbr->sec_per_clus;	
			
			DEBUG__MBR("Root dir sectors   %i", p_mbr->root_dir_sectors);
			DEBUG__MBR("First data sector  %i", p_mbr->first_data_sector);
			DEBUG__MBR("First FAT sector   %i", p_mbr->first_fat_sector);
			DEBUG__MBR("Data sectors       %i", p_mbr->data_sectors);
			DEBUG__MBR("Total clusters     %i\r\n", p_mbr->total_clusters);
			
			result = 0;			
		}
		else
		{
			DEBUG__MBR("ERROR can`t read file");
		}
		
		free(buffer);
	}
	
	return result;
}

uint16_t LD_WORD(uint8_t* ptr)	{ uint16_t rv = ptr[1]; rv = rv << 8 | ptr[0];  return rv; }
uint32_t LD_DWORD(uint8_t* ptr)	{ uint32_t rv = ptr[3]; rv = rv << 8 | ptr[2]; rv = rv << 8 | ptr[1]; rv = rv << 8 | ptr[0];  return rv; }

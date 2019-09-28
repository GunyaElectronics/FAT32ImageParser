/*
 * main.c
 *
 *  Created on: 06.07.2019
 *      Author: Maxim Hunko
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "fat32_parser.h"

int main(int argc, char *argv[]) 
{
	MBR_Record_t mbr;
	
	FILE *fp;
	char *p_param;
	
#if DEBUG_INFO	
	DEBUG_PRINTF("FAT32 image parser V1.0 build %s %s", __DATE__, __TIME__);
	p_param = "disk.img";
#else
	p_param = argv[1];
#endif

   	fp = fopen(p_param, "r");
   	
   	if(fp != NULL)
   	{
   		printf("%s\r\n", p_param);
   		
		if(search_mbr(fp, &mbr) == 0)
		{			
			cluster_data = malloc(mbr.bytes_per_sec * mbr.sec_per_clus);
			
			if(cluster_data)
			{
				set_cluster_sz(mbr.bytes_per_sec * mbr.sec_per_clus);
				
				parse_folder_and_print(fp, &mbr, 2, NULL); //start from NULL(root) folder, recursively!!!
				
				free(cluster_data);
			}
		}
			
		fclose(fp);
	}
   	else
   		printf("Can`t open file\r\n");
   		
	return 0;
}

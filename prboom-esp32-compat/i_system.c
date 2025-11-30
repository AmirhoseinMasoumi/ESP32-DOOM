/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *  Misc system stuff needed by Doom, implemented for Linux.
 *  Mainly timer handling, and ENDOOM/ENDBOOM.
 *
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>

#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#ifdef _MSC_VER
#define    F_OK    0    /* Check for file existence */
#define    W_OK    2    /* Check for write permission */
#define    R_OK    4    /* Check for read permission */
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <sys/stat.h>



#include "config.h"
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "m_argv.h"
#include "lprintf.h"
#include "doomtype.h"
#include "doomdef.h"
#include "lprintf.h"
#include "m_fixed.h"
#include "r_fps.h"
#include "i_system.h"
#include "i_joy.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"

#include "esp_partition.h"
#include "spi_flash_mmap.h"
#include "esp_flash.h"

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "dma.h"
#include "esp_timer.h"

#ifdef __GNUG__
#pragma implementation "i_system.h"
#endif
#include "i_system.h"

#include <sys/time.h>

#define MODE_SPI 1
#define PIN_NUM_MISO 2 //4
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   15

/*
SDMMC pin configuration
#define MODE_SPI 0
MOSI = 15
MISO = 2
CLK = 14
*/


int realtime=0;
//SemaphoreHandle_t dmaChannel2Sem;

void I_uSleep(unsigned long usecs)
{
	vTaskDelay(usecs/1000);
}

static unsigned long getMsTicks() {
  //struct timeval tv;
  //struct timezone tz;
  unsigned long thistimereply;

  //gettimeofday(&tv, &tz);
  unsigned long now = esp_timer_get_time() / 1000;
  //convert to ms
  //unsigned long now = tv.tv_usec/1000+tv.tv_sec*1000;
  return now;
}

int I_GetTime_RealTime (void)
{
  unsigned long thistimereply;
  thistimereply = ((esp_timer_get_time() * TICRATE) / 1000000);
  return thistimereply;

}

const int displaytime=0;

fixed_t I_GetTimeFrac (void)
{
  unsigned long now;
  fixed_t frac;


  now = getMsTicks();

  if (tic_vars.step == 0)
    return FRACUNIT;
  else
  {
    frac = (fixed_t)((now - tic_vars.start + displaytime) * FRACUNIT / tic_vars.step);
    if (frac < 0)
      frac = 0;
    if (frac > FRACUNIT)
      frac = FRACUNIT;
    return frac;
  }
}


void I_GetTime_SaveMS(void)
{
  if (!movement_smooth)
    return;

  tic_vars.start = getMsTicks();
  tic_vars.next = (unsigned int) ((tic_vars.start * tic_vars.msec + 1.0f) / tic_vars.msec);
  tic_vars.step = tic_vars.next - tic_vars.start;
}

unsigned long I_GetRandomTimeSeed(void)
{
	return 4; //per https://xkcd.com/221/
}

const char* I_GetVersionString(char* buf, size_t sz)
{
  sprintf(buf,"%s v%s (http://prboom.sourceforge.net/)",PACKAGE,VERSION);
  return buf;
}

const char* I_SigString(char* buf, size_t sz, int signum)
{
  return buf;
}

extern unsigned char *doom1waddata;
static bool init_flash = false;

// Flash partition for WAD file
static const esp_partition_t *wad_partition = NULL;
#define WAD_PARTITION_OFFSET 0x210000  // Where WAD is stored in flash

void Init_Flash()
{
	lprintf(LO_INFO, "Init_Flash: Initializing flash partition for WAD...\n");
	
	// Find the WAD partition or use raw flash at known offset
	wad_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "doom_wad");
	if (!wad_partition) {
		// Try to find any data partition at our offset, or just use raw flash reading
		lprintf(LO_INFO, "Init_Flash: No doom_wad partition found, using raw flash at 0x%lx\n", (unsigned long)WAD_PARTITION_OFFSET);
	} else {
		lprintf(LO_INFO, "Init_Flash: Found WAD partition at 0x%lx, size %lu\n", 
			(unsigned long)wad_partition->address, (unsigned long)wad_partition->size);
	}
	init_flash = true;
}

typedef struct {
	int offset;      // Current read position
	int size;        // Total size of WAD
	int base_offset; // Base offset in flash
	char name[12];
	int is_open;
} FileDesc;

static FileDesc fds[32];
static const char fileName[] = "DOOM.WAD";

int I_Open(const char *wad, int flags) {
	char fname[12];
	strcpy(fname, wad);
	memcpy(fname+(strlen(fname)-4), ".WAD", 4);
	lprintf(LO_INFO, "I_Open: Opening File: %s (as %s)\n", wad, fname);

	if(init_flash == false)
		Init_Flash();
	
	int x=0;
	while (fds[x].is_open && strcmp(fds[x].name, fname)!=0 && x < 32) 
		x++;
	lprintf(LO_INFO, "I_Open: Got handle: %d\n", x);

	if(x == 31 && fds[x].is_open)
	{
		lprintf(LO_INFO, "I_Open: Too many handles open\n");
		return -1;
	}

	if(strcmp(fds[x].name, fname) == 0 && fds[x].is_open)
	{
		lprintf(LO_INFO, "I_Open: File already open\n");
		fds[x].offset = 0;  // Rewind
		return x;
	}

	// For WAD files, read from flash partition
	if (strcmp(fname, fileName)==0 || strcmp(fname, "DOOM1.WAD")==0) {
		// Read WAD header to get file size
		uint8_t header[12];
		esp_err_t err;
		
		if (wad_partition) {
			err = esp_partition_read(wad_partition, 0, header, 12);
		} else {
			// Use direct flash read (no alignment requirements)
			err = esp_flash_read(NULL, header, WAD_PARTITION_OFFSET, 12);  // NULL uses main flash chip
		}
		
		if (err != ESP_OK) {
			lprintf(LO_ERROR, "I_Open: Failed to read WAD header from flash: %d\n", err);
			return -1;
		}
		
		// Check WAD signature
		if (memcmp(header, "IWAD", 4) != 0 && memcmp(header, "PWAD", 4) != 0) {
			lprintf(LO_ERROR, "I_Open: Invalid WAD signature at flash offset 0x%x: %.4s\n", 
				WAD_PARTITION_OFFSET, header);
			return -1;
		}
		
		// Get number of lumps and directory offset to calculate approximate size
		int numlumps = header[4] | (header[5] << 8) | (header[6] << 16) | (header[7] << 24);
		int diroffset = header[8] | (header[9] << 8) | (header[10] << 16) | (header[11] << 24);
		
		// WAD size is directory offset + directory size (16 bytes per lump entry)
		fds[x].size = diroffset + (numlumps * 16);
		fds[x].offset = 0;
		fds[x].base_offset = WAD_PARTITION_OFFSET;
		fds[x].is_open = 1;
		strcpy(fds[x].name, fname);
		
		lprintf(LO_INFO, "I_Open: Opened WAD from flash - lumps: %d, size: %d bytes\n", 
			numlumps, fds[x].size);
	} else if(strcmp("prboom.WAD", fname)==0) {
		// prboom.wad is not needed for basic operation, return error
		lprintf(LO_INFO, "I_Open: prboom.wad not available on flash\n");
		return -1;
	} else {
		lprintf(LO_INFO, "I_Open: Unknown file %s\n", fname);
		return -1;
	}
	
	return x;
}

int I_Lseek(int ifd, off_t offset, int whence) {
//	lprintf(LO_INFO, "I_Lseek: Seeking %d.\n", (int)offset);
	if (whence==SEEK_SET) {
		fds[ifd].offset=offset;
	} else if (whence==SEEK_CUR) {
		fds[ifd].offset+=offset;
	} else if (whence==SEEK_END) {
		fds[ifd].offset = fds[ifd].size + offset;
	}
	return fds[ifd].offset;
}

int I_Filelength(int ifd)
{
	return fds[ifd].size;
}

void I_Close(int fd) {
	lprintf(LO_INFO, "I_Close: Closing File: %s\n", fds[fd].name);
	fds[fd].name[0] = '\0';
	fds[fd].is_open = 0;
}


typedef struct {
//	spi_flash_mmap_handle_t handle;
	int ifd;
	void *addr;
	int offset;
	size_t len;
	int used;
} MmapHandle;

#define NO_MMAP_HANDLES 128
static MmapHandle mmapHandle[NO_MMAP_HANDLES];

static int nextHandle=0;

static int getFreeHandle() {
//	lprintf(LO_INFO, "getFreeHandle: Get free handle... ");
	int n=NO_MMAP_HANDLES;
	while (mmapHandle[nextHandle].used!=0 && n!=0) {
		nextHandle++;
		if (nextHandle==NO_MMAP_HANDLES) nextHandle=0;
		n--;
	}
	if (n==0) {
		lprintf(LO_ERROR, "getFreeHandle: More mmaps than NO_MMAP_HANDLES!\n");
		exit(0);
	}
	
	if (mmapHandle[nextHandle].addr) {
		//spi_flash_munmap(mmapHandle[nextHandle].handle);
		free(mmapHandle[nextHandle].addr);
		mmapHandle[nextHandle].addr=NULL;
//		printf("mmap: freeing handle %d\n", nextHandle);
	}
	int r=nextHandle;
	nextHandle++;
	if (nextHandle==NO_MMAP_HANDLES) nextHandle=0;
//	lprintf(LO_INFO, "Got: %d\n", r);
	return r;
}

void freeUnusedMmaps(void) {
	lprintf(LO_INFO, "freeUnusedMmaps...\n");
	for (int i=0; i<NO_MMAP_HANDLES; i++) {
		//Check if handle is not in use but is mapped.
		if (mmapHandle[i].used==0 && mmapHandle[i].addr!=NULL) {
			//spi_flash_munmap(mmapHandle[i].handle);
			free(mmapHandle[i].addr);
			mmapHandle[i].addr=NULL;
			mmapHandle[i].ifd=0;
			//printf("Freeing handle %d\n", i);
		}
	}
}


void *I_Mmap(void *addr, size_t length, int prot, int flags, int ifd, off_t offset) {
//	lprintf(LO_INFO, "I_Mmap: ifd %d, length: %d, offset: %d\n", ifd, (int)length, (int)offset);
	
	int i;
	esp_err_t err;
	void *retaddr=NULL;

	for (i=0; i<NO_MMAP_HANDLES; i++) {
		if (mmapHandle[i].offset==offset && mmapHandle[i].len==length && mmapHandle[i].ifd==ifd) {
			mmapHandle[i].used++;
			return mmapHandle[i].addr;
		}
	}

	i=getFreeHandle();

	retaddr = malloc(length);
	if(!retaddr)
	{
		lprintf(LO_ERROR, "I_Mmap: No free address space. Cleaning up unused cached mmaps...\n");
		freeUnusedMmaps();
		retaddr = malloc(length);
	}

	if(retaddr)
	{
		I_Lseek(ifd, offset, SEEK_SET);
		I_Read(ifd, retaddr, length);
		mmapHandle[i].addr=retaddr;
		mmapHandle[i].len=length;
		mmapHandle[i].used=1;
		mmapHandle[i].offset=offset;
		mmapHandle[i].ifd=ifd;
	} else {
		lprintf(LO_ERROR, "I_Mmap: Can't mmap offset: %d (len=%d)!\n", (int)offset, length);
		return NULL;
	}

	return retaddr;
}


int I_Munmap(void *addr, size_t length) {
	int i;
	for (i=0; i<NO_MMAP_HANDLES; i++) {
		if (mmapHandle[i].addr==addr && mmapHandle[i].len==length/* && mmapHandle[i].ifd==ifd*/) break;
	}
	if (i==NO_MMAP_HANDLES) {
		lprintf(LO_ERROR, "I_Mmap: Freeing non-mmapped address/len combo!\n");
		exit(0);
	}
//	lprintf(LO_INFO, "I_Mmap: freeing handle %d\n", i);
	mmapHandle[i].used--;
	return 0;
}



void I_Read(int ifd, void* vbuf, size_t sz)
{
	//lprintf(LO_INFO, "I_Read: Reading %d bytes from offset %d\n", (int)sz, fds[ifd].offset);
	
	esp_err_t err;
	
	if (wad_partition) {
		err = esp_partition_read(wad_partition, fds[ifd].offset, vbuf, sz);
	} else {
		// Use direct flash read for raw flash access (no alignment requirements)
		size_t flash_offset = fds[ifd].base_offset + fds[ifd].offset;
		err = esp_flash_read(NULL, vbuf, flash_offset, sz);  // NULL uses main flash chip
	}
	
	if (err != ESP_OK) {
		I_Error("I_Read: Error reading %d bytes from flash offset 0x%x: %d", 
			(int)sz, fds[ifd].offset, err);
		return;
	}
	
	fds[ifd].offset += sz;
}

const char *I_DoomExeDir(void)
{
  return "";
}



char* I_FindFile(const char* wfname, const char* ext)
{
  char *p;
  p = malloc(strlen(wfname)+4);
  sprintf(p, "%s.%s", wfname, ext);
  return NULL;
}

void I_SetAffinityMask(void)
{
}

/*
int access(const char *path, int atype) {
    return 1;
}
*/




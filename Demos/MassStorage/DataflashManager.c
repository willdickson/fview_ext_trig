/*
             LUFA Library
     Copyright (C) Dean Camera, 2009.
              
  dean [at] fourwalledcubicle [dot] com
      www.fourwalledcubicle.com
*/

/*
  Copyright 2009  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, and distribute this software
  and its documentation for any purpose and without fee is hereby
  granted, provided that the above copyright notice appear in all
  copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Functions to manage the physical dataflash media, including reading and writing of
 *  blocks of data. These functions are called by the SCSI layer when data must be stored
 *  or retrieved to/from the physical storage media. If a different media is used (such
 *  as a SD card or EEPROM), functions similar to these will need to be generated.
 */

#define  INCLUDE_FROM_DATAFLASHMANAGER_C
#include "DataflashManager.h"

/** Writes blocks (OS blocks, not Dataflash pages) to the storage medium, the board dataflash IC(s), from
 *  the pre-selected data OUT endpoint. This routine reads in OS sized blocks from the endpoint and writes
 *  them to the dataflash in Dataflash page sized blocks.
 *
 *  \param BlockAddress  Data block starting address for the write sequence
 *  \param TotalBlocks   Number of blocks of data to write
 */
void DataflashManager_WriteBlocks(const uint32_t BlockAddress, uint16_t TotalBlocks)
{
	uint16_t CurrDFPage          = ((BlockAddress * VIRTUAL_MEMORY_BLOCK_SIZE) / DATAFLASH_PAGE_SIZE);
	uint16_t CurrDFPageByte      = ((BlockAddress * VIRTUAL_MEMORY_BLOCK_SIZE) % DATAFLASH_PAGE_SIZE);
	uint8_t  CurrDFPageByteDiv16 = (CurrDFPageByte >> 4);
	bool     NeedPageWrite       = false;

	/* Select the dataflash IC based on the page number */
	Dataflash_SelectChipFromPage(CurrDFPage);
	Dataflash_WaitWhileBusy();
	
	/* Copy selected dataflash's current page contents to the dataflash buffer */
	Dataflash_ToggleSelectedChipCS();
	Dataflash_SendByte(DF_CMD_MAINMEMTOBUFF1);
	Dataflash_SendAddressBytes(CurrDFPage, 0);
	Dataflash_WaitWhileBusy();

	/* Send the dataflash buffer write command */
	Dataflash_ToggleSelectedChipCS();
	Dataflash_SendByte(DF_CMD_BUFF1WRITE);
	Dataflash_SendAddressBytes(0, CurrDFPageByte);

	while (TotalBlocks)
	{
		/* Determine how many endpoint packets in one disk block */
		for (uint8_t PacketsInBlock = 0; PacketsInBlock < (VIRTUAL_MEMORY_BLOCK_SIZE / MASS_STORAGE_IO_EPSIZE); PacketsInBlock++)
		{
			/* Wait until the endpoint is ready to be written to */
			Endpoint_WaitUntilReady();
			
			/* Write an endpoint packet sized data block to the dataflash */
			for (uint8_t BufferByteDiv16 = 0; BufferByteDiv16 < (MASS_STORAGE_IO_EPSIZE >> 4); BufferByteDiv16++)
			{
				/* Indicate that the dataflash buffer contains unwritten data */
				NeedPageWrite = true;

				/* Write one 16-byte chunk of data to the dataflash */
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				Dataflash_SendByte(Endpoint_Read_Byte());
				
				/* Increment the dataflash page 16 byte block counter */
				CurrDFPageByteDiv16++;		

				/* Check if end of dataflash page reached */
				if (CurrDFPageByteDiv16 == (DATAFLASH_PAGE_SIZE >> 4))
				{
					/* Write the dataflash buffer contents back to the dataflash page */
					Dataflash_ToggleSelectedChipCS();
					Dataflash_SendByte(DF_CMD_BUFF1TOMAINMEMWITHERASE);
					Dataflash_SendAddressBytes(CurrDFPage, 0);

					/* Reset the dataflash buffer counter, increment the page counter */
					CurrDFPageByteDiv16 = 0;
					CurrDFPage++;

					/* Indicate current buffer data has been written to the non-volatile memory */
					NeedPageWrite = false;

					/* Select the next dataflash chip based on the new dataflash page index */
					Dataflash_SelectChipFromPage(CurrDFPage);
					Dataflash_WaitWhileBusy();

#if (DATAFLASH_PAGE_SIZE > VIRTUAL_MEMORY_BLOCK_SIZE)
					/* If less than one dataflash page remaining, copy over the existing page to preserve trailing data */
					if ((TotalBlocks * (VIRTUAL_MEMORY_BLOCK_SIZE >> 4)) < (DATAFLASH_PAGE_SIZE >> 4))
					{
						/* Copy selected dataflash's current page contents to the dataflash buffer */
						Dataflash_ToggleSelectedChipCS();
						Dataflash_SendByte(DF_CMD_MAINMEMTOBUFF1);
						Dataflash_SendAddressBytes(CurrDFPage, 0);
						Dataflash_WaitWhileBusy();
					}
#endif

					/* Send the dataflash buffer write command */
					Dataflash_ToggleSelectedChipCS();
					Dataflash_SendByte(DF_CMD_BUFF1WRITE);
					Dataflash_SendAddressBytes(0, 0);
				}
			}

			Endpoint_ClearCurrentBank();

			/* Check if the current command is being aborted by the host */
			if (IsMassStoreReset)
			  return;
		}
			
		/* Decrement the blocks remaining counter and reset the sub block counter */
		TotalBlocks--;
	}

	/* Check if still need to commit the current dataflash buffer to non-volatile memory */
	if (NeedPageWrite)
	{
		/* Write the dataflash buffer contents back to the dataflash page */
		Dataflash_ToggleSelectedChipCS();
		Dataflash_SendByte(DF_CMD_BUFF1TOMAINMEMWITHERASE);
		Dataflash_SendAddressBytes(CurrDFPage, 0x00);
		Dataflash_WaitWhileBusy();
	}

	/* Deselect all dataflash chips */
	Dataflash_DeselectChip();
}

/** Reads blocks (OS blocks, not Dataflash pages) from the storage medium, the board dataflash IC(s), into
 *  the pre-selected data IN endpoint. This routine reads in Dataflash page sized blocks from the Dataflash
 *  and writes them in OS sized blocks to the endpoint.
 *
 *  \param BlockAddress  Data block starting address for the read sequence
 *  \param TotalBlocks   Number of blocks of data to read
 */
void DataflashManager_ReadBlocks(const uint32_t BlockAddress, uint16_t TotalBlocks)
{
	uint16_t CurrDFPage          = ((BlockAddress * VIRTUAL_MEMORY_BLOCK_SIZE) / DATAFLASH_PAGE_SIZE);
	uint16_t CurrDFPageByte      = ((BlockAddress * VIRTUAL_MEMORY_BLOCK_SIZE) % DATAFLASH_PAGE_SIZE);
	uint8_t  CurrDFPageByteDiv16 = (CurrDFPageByte >> 4);

	/* Select the dataflash IC based on the page number */
	Dataflash_SelectChipFromPage(CurrDFPage);
	Dataflash_WaitWhileBusy();
	
	/* Send the dataflash continuous page read command */
	Dataflash_ToggleSelectedChipCS();
	Dataflash_SendByte(DF_CMD_CONTARRAYREAD_LF);
	Dataflash_SendAddressBytes(CurrDFPage, CurrDFPageByte);
	
	while (TotalBlocks)
	{
		/* Determine how many endpoint packets in one disk block */
		for (uint8_t PacketsInBlock = 0; PacketsInBlock < (VIRTUAL_MEMORY_BLOCK_SIZE / MASS_STORAGE_IO_EPSIZE); PacketsInBlock++)
		{
			/* Wait until the endpoint is ready to be written to */
			Endpoint_WaitUntilReady();

			/* Read in an endpoint packet sized data block from the dataflash */
			for (uint8_t BufferByteDiv16 = 0; BufferByteDiv16 < (MASS_STORAGE_IO_EPSIZE >> 4); BufferByteDiv16++)
			{				
				/* Read one 16-byte chunk of data from the dataflash */
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				Endpoint_Write_Byte(Dataflash_ReceiveByte());
				
				/* Increment the dataflash page 16 byte block counter */
				CurrDFPageByteDiv16++;
				
				/* Check if end of dataflash page reached */
				if (CurrDFPageByteDiv16 == (DATAFLASH_PAGE_SIZE >> 4))
				{
					/* Reset the dataflash buffer counter, increment the page counter */
					CurrDFPageByteDiv16 = 0;
					CurrDFPage++;

					/* Select the next dataflash chip based on the new dataflash page index */
					Dataflash_SelectChipFromPage(CurrDFPage);
					Dataflash_SendByte(DF_CMD_CONTARRAYREAD_LF);
					Dataflash_SendAddressBytes(CurrDFPage, 0);
				}	
			}

			/* Check if the current command is being aborted by the host */
			if (IsMassStoreReset)
			  return;
			  
			Endpoint_ClearCurrentBank();
		}
		
		/* Decrement the blocks remaining counter */
		TotalBlocks--;
	}

	/* Deselect all dataflash chips */
	Dataflash_DeselectChip();
}

/** Disables the dataflash memory write protection bits on the board Dataflash ICs, if enabled. */
void DataflashManager_ResetDataflashProtections(void)
{
	/* Select first dataflash chip, send the read status register command */
	Dataflash_SelectChip(DATAFLASH_CHIP1);
	Dataflash_SendByte(DF_CMD_GETSTATUS);
	
	/* Check if sector protection is enabled */
	if (Dataflash_ReceiveByte() & DF_STATUS_SECTORPROTECTION_ON)
	{
		Dataflash_ToggleSelectedChipCS();

		/* Send the commands to disable sector protection */
		Dataflash_SendByte(DF_CMD_SECTORPROTECTIONOFF[0]);
		Dataflash_SendByte(DF_CMD_SECTORPROTECTIONOFF[1]);
		Dataflash_SendByte(DF_CMD_SECTORPROTECTIONOFF[2]);
		Dataflash_SendByte(DF_CMD_SECTORPROTECTIONOFF[3]);
	}
	
	/* Select second dataflash chip (if present on selected board), send read status register command */
	#if (DATAFLASH_TOTALCHIPS == 2)
	Dataflash_SelectChip(DATAFLASH_CHIP2);
	Dataflash_SendByte(DF_CMD_GETSTATUS);
	
	/* Check if sector protection is enabled */
	if (Dataflash_ReceiveByte() & DF_STATUS_SECTORPROTECTION_ON)
	{
		Dataflash_ToggleSelectedChipCS();

		/* Send the commands to disable sector protection */
		Dataflash_SendByte(DF_CMD_SECTORPROTECTIONOFF[0]);
		Dataflash_SendByte(DF_CMD_SECTORPROTECTIONOFF[1]);
		Dataflash_SendByte(DF_CMD_SECTORPROTECTIONOFF[2]);
		Dataflash_SendByte(DF_CMD_SECTORPROTECTIONOFF[3]);
	}
	#endif
	
	/* Deselect current dataflash chip */
	Dataflash_DeselectChip();
}

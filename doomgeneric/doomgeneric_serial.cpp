// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

// For JPEG methods
#include "../JPEGENC/src/JPEGENC.h"

// For COM methods
#include <time.h>
#include <fileapi.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "doomgeneric.h"
#include "doomgeneric_serial.h"

HANDLE getCOM(char *arg);
void clearCOM(HANDLE hComDev);
void releaseCOM(HANDLE hComDev);
void prepareFrame();
int compress();
int sendData(uint8_t *dataToSend, int bytesToSend);
void saveFrame();

HANDLE com;
uint8_t *resultBuffer;
uint8_t *inputFrame;

// Size of resulting jpeg (in bytes)
int iDataSize = 0;
// Image size (estimation)
const int iSize = (DOOMGENERIC_RESX * DOOMGENERIC_RESY * 2) / 3;

void openCOM() {
	// 'jpg' will fill this buffer with the resulting image
	resultBuffer = (uint8_t *)malloc(iSize);
	// This buffer is used to turn the contents of DG_ScreenBuffer from RGBA to RGB to then be fed into JPEGENC
	inputFrame = (uint8_t *)malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 3);
	com = getCOM("\\\\.\\COM19");
	clearCOM(com);
	printf("INIT COMPLETE");
}

void closeCOM() {
	free(resultBuffer);
	free(inputFrame);
	releaseCOM(com);
}

void sendSerialFrame() {
	prepareFrame();
	if(compress())
		printf("FAILED TO COMPRESS");
	if (sendData(resultBuffer, iDataSize))
		printf("FAILED TO SEND DATA");
}

// The JPEG compressor is expecting a byte array in a RGB arrangment
// So here we take each pixel (uint32) from the screenbuffer and copy it into our array, ommiting the alpha channel (px[3])
void prepareFrame()
{
	for (int i = 0; i < (DOOMGENERIC_RESX * DOOMGENERIC_RESY); i++)
	{
		unsigned char *px = (unsigned char *)&DG_ScreenBuffer[i];
		//fprintf(fdlog, "Accessing: %d\n", i);
		inputFrame[i*3] = px[0];
		inputFrame[i*3+1] = px[1];
		inputFrame[i*3+2] = px[2];
	}
}

int compress()
{
    // Static copy of JPEG encoder class
    JPEGENC jpg;
    // Struct that stores JPEGENC state
    JPEGENCODE state;
    // Stores return code
	int rc;
	// Specify input pixel format (24-bit RGB)
	uint8_t ucPixelType = JPEGE_PIXEL_RGB888;
    // Bits per pixel (24-bit)
	int iBytePP = 3; 
    // Image stride (number of bytes in a row of pixels)
    int iPitch = iBytePP * DOOMGENERIC_RESX;
	rc = jpg.open(resultBuffer, iSize);
	if (rc != JPEGE_SUCCESS)
	{
        //free(resultBuffer);
        //free(pBitmap);
        return 1;
    }
    rc = jpg.encodeBegin(&state, DOOMGENERIC_RESX, DOOMGENERIC_RESY, ucPixelType, JPEGE_SUBSAMPLE_420, JPEGE_Q_HIGH);
    if (rc != JPEGE_SUCCESS)
    {
        //free(resultBuffer);
        //free(pBitmap);
        return 1;
    }
    rc = jpg.addFrame(&state, inputFrame, iPitch);
    iDataSize = jpg.close();
	
	return 0;
}

int sendData(uint8_t *dataToSend, int bytesToSend)
{
	//HANDLE com = getCOM("\\\\.\\COM19");
	//clearCOM(com);

	// const char *dataToSend = "cd";
	// DWORD bytesToSend = (DWORD)strlen((char*)dataToSend);
	DWORD bytesWritten = 0;
	// string r = "";
	//printf("Attempting to write %lu bytes to COM19: \"%x\"\n", bytesToSend, *dataToSend);--------
	for (int j = 0; j < 1; j++)
	{
		BOOL writeResult = WriteFile(com, dataToSend, bytesToSend, &bytesWritten, NULL);
		if (!writeResult)
		{
			fprintf(stderr, "Error: Failed to write to COM19. Error Code: %lu\n", GetLastError());
			//closeCOM();
			return 1;
		}
		if (bytesWritten == bytesToSend)
		{
			//printf("Successfully wrote %lu bytes to COM19.\n", bytesWritten); -----------------
		}
		else
		{
			// This might happen if timeouts occur during a synchronous write,
			// though less common for typical short serial writes.
			fprintf(stderr, "Warning: Wrote %lu bytes, but expected to write %lu bytes to COM19.\n", bytesWritten, bytesToSend);
		}
	}

	//closeCOM(com);
	return 0;
}

// COM helper functions

HANDLE getCOM(char *arg)
{
	HANDLE hComDev;
	if ((hComDev = CreateFile(arg,
							  GENERIC_READ | GENERIC_WRITE,
							  0,
							  NULL,
							  OPEN_EXISTING,
							  FILE_ATTRIBUTE_NORMAL,
							  NULL)) == INVALID_HANDLE_VALUE)
	{
		printf("Failed to open port");
		exit(1);
	}
	printf("Opened port, getting & setting port state");
	DCB Dcb;
	GetCommState(hComDev, &Dcb);
	Dcb.BaudRate = 3000000;
	Dcb.StopBits = ONESTOPBIT;
	Dcb.ByteSize = 8;
	Dcb.Parity = NOPARITY;
	Dcb.fParity = 0;
	Dcb.fOutxCtsFlow = 0;
	Dcb.fOutxDsrFlow = 0;
	Dcb.fDsrSensitivity = 0;
	Dcb.fTXContinueOnXoff = TRUE;
	Dcb.fOutX = 0;
	Dcb.fInX = 0;
	Dcb.fNull = 0;
	Dcb.fErrorChar = 0;
	Dcb.fAbortOnError = 0;
	Dcb.fRtsControl = RTS_CONTROL_DISABLE;
	Dcb.fDtrControl = DTR_CONTROL_DISABLE;
	SetCommState(hComDev, &Dcb);
	printf("Port state set");
	return hComDev;
}

void clearCOM(HANDLE hComDev)
{
	PurgeComm(hComDev, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
	COMSTAT comStat;
	DWORD dwErrorFlags;
	ClearCommError(hComDev, &dwErrorFlags, &comStat);
	return;
}

void releaseCOM(HANDLE hComDev)
{
	if (hComDev != NULL && hComDev != INVALID_HANDLE_VALUE)
	{
		if (!CloseHandle(hComDev))
		{
			fprintf(stderr, "Error: Failed to close COM handle %p. Error Code: %lu\n", (void *)hComDev, GetLastError());
		}
		else
		{
			printf("Successfully closed handle %p\n", (void *)hComDev);
		}
	}
	else
	{
		printf("Warning: Attempted to close an invalid or NULL handle.\n");
	}
}

void saveFrame() {
	FILE *oHandle = fopen("doomgtest.jpg", "w+b");
	if (oHandle)
	{
		fwrite(resultBuffer, 1, iDataSize, oHandle);
		fclose(oHandle);
	}
	closeCOM();
	exit(1);
}
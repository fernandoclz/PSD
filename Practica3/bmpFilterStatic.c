#include "bmpBlackWhite.h"
#include "mpi.h"

/** Show log messages */
#define SHOW_LOG_MESSAGES 1

/** Enable output for filtering information */
#define DEBUG_FILTERING 0

/** Show information of input and output bitmap headers */
#define SHOW_BMP_HEADERS 0

int main(int argc, char **argv)
{

	tBitmapFileHeader imgFileHeaderInput;  /** BMP file header for input image */
	tBitmapInfoHeader imgInfoHeaderInput;  /** BMP info header for input image */
	tBitmapFileHeader imgFileHeaderOutput; /** BMP file header for output image */
	tBitmapInfoHeader imgInfoHeaderOutput; /** BMP info header for output image */
	char *sourceFileName;				   /** Name of input image file */
	char *destinationFileName;			   /** Name of output image file */
	int inputFile, outputFile;			   /** File descriptors */
	unsigned char *outputBuffer;		   /** Output buffer for filtered pixels */
	unsigned char *inputBuffer;			   /** Input buffer to allocate original pixels */
	unsigned char *auxPtr;				   /** Auxiliary pointer */
	unsigned int rowSize;				   /** Number of pixels per row */
	unsigned int rowsPerProcess;		   /** Number of rows to be processed (at most) by each worker */
	unsigned int rowsSentToWorker;		   /** Number of rows to be sent to a worker process */
	unsigned int threshold;				   /** Threshold */
	unsigned int currentRow;			   /** Current row being processed */
	unsigned int currentPixel;			   /** Current pixel being processed */
	unsigned int outputPixel;			   /** Output pixel */
	unsigned int readBytes;				   /** Number of bytes read from input file */
	unsigned int writeBytes;			   /** Number of bytes written to output file */
	unsigned int totalBytes;			   /** Total number of bytes to send/receive a message */
	unsigned int numPixels;				   /** Number of neighbour pixels (including current pixel) */
	unsigned int currentWorker;			   /** Current worker process */
	tPixelVector vector;				   /** Vector of neighbour pixels */
	int imageDimensions[2];				   /** Dimensions of input image */
	double timeStart, timeEnd;			   /** Time stamps to calculate the filtering time */
	int size, rank, tag;				   /** Number of process, rank and tag */
	MPI_Status status;					   /** Status information for received messages */

	// Init
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	tag = 1;
	srand(time(NULL));

	// Check the number of processes
	if (size <= 2)
	{

		if (rank == 0)
			printf("This program must be launched with (at least) 3 processes\n");

		MPI_Finalize();
		exit(0);
	}

	// Check arguments
	if (argc != 4)
	{

		if (rank == 0)
			printf("Usage: ./bmpFilterStatic sourceFile destinationFile threshold\n");

		MPI_Finalize();
		exit(0);
	}

	// Get input arguments...
	sourceFileName = argv[1];
	destinationFileName = argv[2];
	threshold = atoi(argv[3]);

	// Master process
	if (rank == 0)
	{

		// Process starts
		timeStart = MPI_Wtime();

		// Read headers from input file
		readHeaders(sourceFileName, &imgFileHeaderInput, &imgInfoHeaderInput);
		readHeaders(sourceFileName, &imgFileHeaderOutput, &imgInfoHeaderOutput);

		// Write header to the output file
		writeHeaders(destinationFileName, &imgFileHeaderOutput, &imgInfoHeaderOutput);

		// Calculate row size for input and output images
		rowSize = (((imgInfoHeaderInput.biBitCount * imgInfoHeaderInput.biWidth) + 31) / 32) * 4;

		// Show headers...
		if (SHOW_BMP_HEADERS)
		{
			printf("Source BMP headers:\n");
			printBitmapHeaders(&imgFileHeaderInput, &imgInfoHeaderInput);
			printf("Destination BMP headers:\n");
			printBitmapHeaders(&imgFileHeaderOutput, &imgInfoHeaderOutput);
		}

		// Open source image
		if ((inputFile = open(sourceFileName, O_RDONLY)) < 0)
		{
			printf("ERROR: Source file cannot be opened: %s\n", sourceFileName);
			exit(1);
		}

		// Open target image
		if ((outputFile = open(destinationFileName, O_WRONLY, 0777)) < 0)
		{
			printf("ERROR: Target file cannot be open to append data: %s\n", destinationFileName);
			exit(1);
		}

		// Allocate memory to copy the bytes between the header and the image data
		outputBuffer = (unsigned char *)malloc((imgFileHeaderInput.bfOffBits - BIMAP_HEADERS_SIZE) * sizeof(unsigned char));

		// Copy bytes between headers and pixels
		lseek(inputFile, BIMAP_HEADERS_SIZE, SEEK_SET);
		read(inputFile, outputBuffer, imgFileHeaderInput.bfOffBits - BIMAP_HEADERS_SIZE);
		write(outputFile, outputBuffer, imgFileHeaderInput.bfOffBits - BIMAP_HEADERS_SIZE);

		unsigned int height = abs(imgInfoHeaderInput.biHeight);
		unsigned int width = imgInfoHeaderInput.biWidth;
		unsigned int pixelesTotalesEnBytes = height * rowSize;

		inputBuffer = (unsigned char *)malloc(pixelesTotalesEnBytes * sizeof(unsigned char));
		read(inputFile, inputBuffer, pixelesTotalesEnBytes);

		imageDimensions[0] = height;
		imageDimensions[1] = width;

		unsigned int info_buffer[3];
		info_buffer[0] = imageDimensions[0];
		info_buffer[1] = imageDimensions[1];
		info_buffer[2] = rowSize;
		MPI_Bcast(info_buffer, 3, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

		rowsPerProcess = height / (size - 1);
		unsigned int ultimas_filas = height % (size - 1);
		currentRow = 0;

		for (currentWorker = 1; currentWorker < size; currentWorker++)
		{
			rowsSentToWorker = rowsPerProcess;
			if (currentWorker == size - 1)
			{
				rowsSentToWorker += ultimas_filas;
			}

			totalBytes = rowsSentToWorker * rowSize;
			unsigned int buffer_size = 2 * sizeof(unsigned int) + totalBytes;

			auxPtr = (unsigned char *)malloc(buffer_size);

			memcpy(auxPtr, &rowsSentToWorker, sizeof(unsigned int));
			memcpy(auxPtr + sizeof(unsigned int), &currentRow, sizeof(unsigned int));
			memcpy(auxPtr + sizeof(unsigned int) * 2, inputBuffer + currentRow * rowSize, totalBytes);

			MPI_Send(auxPtr, buffer_size, MPI_BYTE, currentWorker, tag, MPI_COMM_WORLD);

			free(auxPtr);
			currentRow += rowsSentToWorker;
		}
		// Recibir de ANY_SOURCE
		unsigned int a_recibir = size - 1;
		for (unsigned int i = 0; i < a_recibir; i++)
		{
			MPI_Probe(MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
			int count;
			MPI_Get_count(&status, MPI_BYTE, &count);
			unsigned char *recvBuffer = (unsigned char *)malloc(count);
			MPI_Recv(recvBuffer, count, MPI_BYTE, status.MPI_SOURCE, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			unsigned int rowsReceived = 0;
			unsigned int startRow = 0;
			memcpy(&rowsReceived, recvBuffer, sizeof(unsigned int));
			memcpy(&startRow, recvBuffer + sizeof(unsigned int), sizeof(unsigned int));

			unsigned int pixelsByte = count - sizeof(unsigned int) * 2;

			printf("Master: received %u bytes for %u rows starting at row %u from worker %d\n", pixelsByte, rowsReceived, startRow, status.MPI_SOURCE);

			off_t writePos = (off_t)imgFileHeaderOutput.bfOffBits + ((off_t)startRow * rowSize);
			lseek(outputFile, writePos, SEEK_SET);

			unsigned char *dataToWrite = recvBuffer + 2 * sizeof(unsigned int);
			writeBytes = write(outputFile, dataToWrite, pixelsByte);

			free(recvBuffer);
		}
		unsigned int rowsToSend = 0; // 0 filas indica fin
		for (int worker = 1; worker < size; worker++)
		{
			MPI_Send(&rowsToSend, 1, MPI_UNSIGNED, worker, tag, MPI_COMM_WORLD);
		}

		// Close files
		close(inputFile);
		close(outputFile);

		// Process ends
		timeEnd = MPI_Wtime();

		// Show processing time
		printf("Filtering time: %f\n", timeEnd - timeStart);
	}

	// Worker process
	else
	{
		unsigned int info_buffer[3];
		MPI_Bcast(info_buffer, 3, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

		unsigned int height = info_buffer[0];
		unsigned int width = info_buffer[1];
		rowSize = info_buffer[2];
		unsigned int rowsReceived = 0;
		do
		{
			MPI_Status status;
			MPI_Probe(0, tag, MPI_COMM_WORLD, &status);
			int count;
			MPI_Get_count(&status, MPI_BYTE, &count);

			unsigned char *recvBuffer = (unsigned char *)malloc(count);
			MPI_Recv(recvBuffer, count, MPI_BYTE, 0, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			unsigned int startRow;
			memcpy(&rowsReceived, recvBuffer, sizeof(unsigned int));

			if (rowsReceived != 0)
			{
				memcpy(&startRow, recvBuffer + sizeof(unsigned int), sizeof(unsigned int));
				unsigned char *data = recvBuffer + sizeof(unsigned int) * 2;
				unsigned int pixelsByte = count - sizeof(unsigned int) * 2;

				printf("Worker %d: received %u rows starting at %u (bytes=%u)\n", rank, rowsReceived, startRow, pixelsByte);

				unsigned int out_buffer_size = 2 * sizeof(unsigned int) + pixelsByte;
				auxPtr = (unsigned char *)malloc(out_buffer_size);

				memcpy(auxPtr, &rowsReceived, sizeof(unsigned int));
				memcpy(auxPtr + sizeof(unsigned int), &startRow, sizeof(unsigned int));

				for (unsigned int r = 0; r < rowsReceived; ++r)
				{
					unsigned char *rowIn = data + ((size_t)r * rowSize);
					unsigned char *rowOut = auxPtr + 2 * sizeof(unsigned int) + ((size_t)r * rowSize);

					for (unsigned int col = 0; col < width; col++)
					{
						unsigned char left = (col > 0) ? rowIn[3 * (col - 1)] : rowIn[3 * col];
						unsigned char center = rowIn[3 * col];
						unsigned char right = (col + 1 < width) ? rowIn[3 * (col + 1)] : rowIn[3 * col];

						tPixelVector vec = {left, center, right};

						unsigned char outValue = calculatePixelValue(vec, 3, threshold, 0);

						// Escribir en los 3 canales
						rowOut[3 * col + 0] = outValue;
						rowOut[3 * col + 1] = outValue;
						rowOut[3 * col + 2] = outValue;
					}
					if (rowSize > width * 3)
					{
						memcpy(rowOut + width * 3, rowIn + width * 3, rowSize - width * 3);
					}
				}
				MPI_Send(auxPtr, out_buffer_size, MPI_BYTE, 0, tag, MPI_COMM_WORLD);

				free(auxPtr);
			}
			else
			{
				printf("Worker %d ending\n", rank);
			}
			free(recvBuffer);
		} while (rowsReceived != 0);
	}

	// Finish MPI environment
	MPI_Finalize();
	return 0;
}

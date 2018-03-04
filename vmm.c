//Author: Chance Ball
//Date: 03/03/17
//Project 2: 4347

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


//Defined variables
#define FRAME_ENTRIES 256 //Number of frames in physical memory
#define FRAME_SIZE 256 //Frame size
#define MEM_SIZE 65536 //Physical memory size
#define PAGE_ENTRIES 256 //Page table entries
#define PAGE_NUM_BITS 8 //Page number size, bits
#define PAGE_SIZE 256 //Page size
#define TLB_ENTRIES 16 //Max TLB entries

//Global variables
int frameNum; //Frame number
int memIndex = 0; //Points to first empty frame
int offset; 
int pAddr; //Physical address
int pgNum; //Page number
char pMemory[MEM_SIZE]; //Physical memory. Each char = 1 byte
int pgTable[PAGE_ENTRIES];
int TLB[TLB_ENTRIES][2];
int TLBFront = -1; //TLB front index for a queue
int TLBBack = -1; //TLB back index for a queue
int vAddr; //Virtual address
int pAddrValue;
//Fault/Hit rate variables
int addressCount = 0; //Counts addresses read
int faultCount = 0; //Count page faults
float faults; //Rate of faults
int TLBCount = 0; //TLB hits
float TLBHits; //Rate of TLB Hits

//Function Declarations
int getOffset(int vAddr);
int getpAddr(int vAddr);
int getpgNum(int vAddr);
void pgTableInit(int n);
int pgTableLookup(int pgNum);
void TLBInit(int n);
int TLBLookup(int pgNum);
void updateTLB(int pgNum, int frameNum);

int main(int argc, char *argv[]) 
{
    //File names, pointers, data, etc
    FILE* inFile; //Input file pointer
    char* inName; //Filename for logical addresses. usually addresses.txt

    FILE* outFile; //Output file pointer
    char* outName; //Filename where physical addresses and values are stored
    
    char* RAFName; //Random Access File's name; usually BACKING_STORE.bin
    char* RAData; //Data retrieved from the Random Access File

    char line[8]; //Holds each line input
    int fd;
   

    //Initialize page table and set all to -1
    pgTableInit(-1);
    TLBInit(-1);

    if (argc != 4) 
    {
        printf("Usage: ./vmm [input file] [output file] [binary file]");

        exit(EXIT_FAILURE);
    }
    else 
    {
        inName = argv[1];
        outName = argv[2];
        RAFName = argv[3];

        if ((inFile = fopen(inName, "r")) == NULL) 
        {
            printf("Input file unable to open.\n");
            exit(EXIT_FAILURE);
        }
        if ((outFile = fopen(outName, "w")) == NULL) 
        {
            printf("Output file unable to open.\n");
            exit(EXIT_FAILURE);
        }

       
        //Map the Random Access File to memory, and open a file descriptor
        fd = open(RAFName, O_RDONLY);
        RAData = mmap(0, MEM_SIZE, PROT_READ, MAP_SHARED, fd, 0);
        //Check if mapping worked
        if (RAData == MAP_FAILED) 
        {
            close(fd);
            printf("Error mmapping BACKING_STORE.bin file");
            exit(EXIT_FAILURE);
        }

        //Loop through the input file
        while (fgets(line, sizeof(line), inFile)) 
        {
            vAddr = atoi(line);
            addressCount++;


            //Get offset from the virtual address
            offset = getOffset(vAddr);
            //Get page number from the virtual address
            pgNum = getpgNum(vAddr);
            
            //Use page number to find frame number in TLB, hopefully
            frameNum = TLBLookup(pgNum);

            //Check frame number returned by the lookup on the TLB
            if (frameNum != -1) 
            {
                //If frame number isn't -1, then the TLB lookup worked and we didn't have to change anything
                pAddr = frameNum + offset;

                //Fetch the value directly from memory
                pAddrValue = pMemory[pAddr];
            }
            else 
            {
                //TLB lookup failed
                //Look for frame number in page table
                frameNum = pgTableLookup(pgNum);

                //Check frame number gotten from page table lookup
                if (frameNum != -1) 
                {
                    //No page fault, thankfully
                    pAddr = frameNum + offset;

                    //Update our TLB
                    updateTLB(pgNum, frameNum);

                    //Fetch value stored at physical address
                    pAddrValue = pMemory[pAddr];
                }
                else 
                {
                    //Page fault, why God
                    //Read in a 256-byte from BACKING_STORE.bin and store it in availableframe in the physical memory
                    
                    //go to address needed that's in BACKING_STORE
                    int pgAddress = pgNum * PAGE_SIZE;

                    //Look for a free frame 
                    if (memIndex != -1) 
                    {
                        //There is a free frame, so we store the value from BACKING_STORE into the frame
                        memcpy((pMemory + memIndex), (RAData + pgAddress), PAGE_SIZE);

                        //Find the physical address of the specific byte
                        frameNum = memIndex;
                        pAddr = frameNum + offset;
                        pAddrValue = pMemory[pAddr];

                        //Update our page table with the now correct frame number, and the TLB as well
                        pgTable[pgNum] = memIndex;
                        updateTLB(pgNum, frameNum);

                        //Increment index in the memory
                        if (memIndex < MEM_SIZE - FRAME_SIZE)
                            memIndex += FRAME_SIZE;
                        
                        else //Set the memory index to -1 which means our memory is full
                            memIndex = -1;
                    
                    }
                    else 
                    {
                        //There's no free frame in memory, which means we've really messed up
                        printf("Uh oh, no free frames");
                    }
                }
            }
            //Storing our things into the output file.
            fprintf(outFile, "Virtual address: %d ", vAddr); 
            fprintf(outFile, "Physical address: %d ", pAddr);
            fprintf(outFile, "Value: %d\n", pAddrValue);
        }

        //Calculate rates for faults and TLB hits
        faults = (float) faultCount / (float) addressCount;
        TLBHits = (float) TLBCount / (float) addressCount;

        //Print the statistics at the end
        fprintf(outFile, "\n\n");
        fprintf(outFile, "--------------------------RESULTS--------------------------\n");
        fprintf(outFile, "Number of addresses translated : %d\n", addressCount); 
        fprintf(outFile, "Number of page faults : %d\n", faultCount);
        fprintf(outFile, "Rate of page faults : %.3f\n", faults);
        fprintf(outFile, "Number of TLB hits : %d\n", TLBCount);
        fprintf(outFile, "Rate of TLB hits : %.3f\n", TLBHits);

        //Close all files and the file descriptor
        fclose(inFile);
        fclose(outFile);
        close(fd);
    }

    return EXIT_SUCCESS;
}

//Functions
int getOffset(int vAddr) 
{
    //The mask is 2^8-1, or 1111 1111
    int mask = 255;

    return vAddr & mask;
}

int getpAddr(int vAddr) 
{
    pAddr = getpgNum(vAddr) + getOffset(vAddr);
    return pAddr;
}

int getpgNum(int vAddr) 
{
    //Shift virtual to the right by 8 bits
    return (vAddr >> PAGE_NUM_BITS);
}

void pgTableInit(int n) //Sets page table entries to -1 initially
{
    for (int i = 0; i < PAGE_ENTRIES; i++) 
        pgTable[i] = n;
}

int pgTableLookup(int pgNum) //Takes page number and checks for its corresponding frame
{
    if (pgTable[pgNum] == -1) 
        faultCount++;
    
    return pgTable[pgNum];
}

void TLBInit(int n) //Sets TLB entries to -1 intiially
{
    for (int i = 0; i < TLB_ENTRIES; i++) 
    {
        TLB[i][0] = -1;
        TLB[i][1] = -1;
    }
}

int TLBLookup(int pgNum) 
{
    //If page number's found return the corresponding frame
    for (int i = 0; i < TLB_ENTRIES; i++) 
    {
        if (TLB[i][0] == pgNum) 
        {
            TLBCount++;
            return TLB[i][1];
        }
    }

    //Page number doesn't exist in TLB, returns a -1
    return -1;
}

void updateTLB(int pgNum, int frameNum) 
{
    //Uses FIFO right now, should be Second Chance later
    //I'm just making sure to have vanilla project done first
    if (TLBFront == -1) 
    {
        //Set front and back indices for the queue
        TLBFront = 0;
        TLBBack = 0;

        //Update TLB
        TLB[TLBBack][0] = pgNum;
        TLB[TLBBack][1] = frameNum;
    }
    else 
    {
        //Circular array implements queue
        TLBFront = (TLBFront + 1) % TLB_ENTRIES;
        TLBBack = (TLBBack + 1) % TLB_ENTRIES;

        //Insert new TLB entry
        TLB[TLBBack][0] = pgNum;
        TLB[TLBBack][1] = frameNum;
    }
}
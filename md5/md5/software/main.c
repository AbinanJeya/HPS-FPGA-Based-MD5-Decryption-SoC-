#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include "hwlib.h"
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"
#include "hps_0.h"
#include <assert.h>
#include <pthread.h>

// Define a macro for printing messages
#define PRINT_ENABLED 0 // Set to 1 to enable printing, 0 to disable
#define print(fmt, ...) if (PRINT_ENABLED) printf(fmt, ##__VA_ARGS__)
#define CURRENT_TIME time(NULL)
#define NUMBER_OF_ENGINES 16
#define LW_SIZE 0x00200000
#define LWHPS2FPGA_BASE 0xff200000

const static size_t timeout = 10;
volatile uint32_t *md5ControlBase = NULL;
volatile uint32_t *md5DataBase = NULL;
int success, total;

const uint32_t testSeq[] =
{0x01680208,
 0x13ab80bb,
 0xcb8b2c30,
 0xb9657582,
 0xa3793c48,
 0x103f26be,
 0x0b78dac4,
 0x5c433348,
 0x4de99287,
 0xeff0be7c,
 0x00808533,0x00000000,
 0x00000000,
 0x00000000,
 0x00000150,
 0x00000000};

typedef struct {
    size_t engine;
    uint32_t message;
} ThreadArgs;

typedef struct {
    struct md5Control {
        volatile uint32_t* md5_start;
        volatile uint32_t* md5_reset;
        volatile uint32_t* md5_wr;
        volatile uint32_t* md5_done;
    } md5Control;
    struct md5Data {
        volatile uint32_t* writedata;
        volatile uint32_t* writeaddr;
        volatile uint32_t* readaddr;
        volatile uint32_t* readdata;
    } md5Data;
} s_md5Registers;

s_md5Registers md5Registers = {
    .md5Control = {0},
    .md5Data = {0}
};

void setRegister(volatile uint32_t* registerName, volatile uint32_t word, char* registerNameString)
{
    alt_write_word(registerName, word);
    print("Asserting: \t\tRegister(0x%08x, %s) = 0x%08x\n", registerName, registerNameString, alt_read_word(registerName));
}

void clearRegister(volatile uint32_t* registerName, char* registerNameString){
    alt_write_word(registerName, 0x0);
    print("De-Asserting: \t\tRegister(0x%08x, %s) = 0x%08x\n", registerName, registerNameString, alt_read_word(registerName));
}

void checkRegister(volatile uint32_t* registerName, uint32_t word, char* registerNameString)
{
    time_t startTime = time(NULL);
    print("Reading: \t\tRegister(0x%08x, %s) = 0x%08x\n", registerName, registerNameString, alt_read_word(registerName));
    while (1)
    {
        if (alt_read_word(registerName) == word)
            return ;
        assert(difftime(CURRENT_TIME, startTime) < timeout);
    }
}

void md5Reset(){
    print("===\t Resetting \t===\n");
    setRegister(md5Registers.md5Control.md5_reset, 0x1, "md5_reset");
    clearRegister(md5Registers.md5Control.md5_wr, "md5_wr");
    clearRegister(md5Registers.md5Control.md5_start, "md5_start");
    clearRegister(md5Registers.md5Data.writeaddr, "writeaddr");
    clearRegister(md5Registers.md5Data.readaddr, "readaddr");
    clearRegister(md5Registers.md5Control.md5_reset, "md5_reset");
}

void md5Start(){
    print("===\t Starting Engine \t===\n");
    setRegister(md5Registers.md5Control.md5_start, 0x1, "md5_start");
    clearRegister(md5Registers.md5Control.md5_start, "md5_start");
    checkRegister(md5Registers.md5Control.md5_done, 0xFFFFFFFF, "md5_done");
}

void md5PrintDigest(){
    print("===\t Reading Digest \t===\n");
    uint32_t totalDigest[] = {0, 0, 0, 0};
    int i;
    for (i = 0; i < 4; i++)
    {
        setRegister(md5Registers.md5Data.readaddr, i, "readaddr");
        totalDigest[i] = alt_read_word(md5Registers.md5Data.readdata);
    }
    printf("Digest: ");
    for (i = 0; i < 4; i++)
        printf("%08x", totalDigest[3 - i]);
}

void md5HardwareAlgorithm(){
    // Reset
    md5Reset();
    // Set data
    print("===\t Setting Data \t===\n");
    setRegister(md5Registers.md5Control.md5_wr, 0x1, "md5_wr");
    int i;
    for (i = 0; i < 16; i++)
    {
        print("Stage: %d\n", i);
        setRegister(md5Registers.md5Data.writeaddr, i, "writeaddr");
        setRegister(md5Registers.md5Data.writedata, testSeq[i], "writedata");
    }
    clearRegister(md5Registers.md5Control.md5_wr, "md5_wr");
    // Start engine
    md5Start();
    // Read digest
    md5PrintDigest();
}

void writeEngine(void* args){
    ThreadArgs* parameters = (ThreadArgs*)args;
    size_t engine = parameters->engine;
    uint32_t message = parameters->message;
    setRegister(md5Registers.md5Data.writeaddr, engine, "writeaddr");
    setRegister(md5Registers.md5Data.writedata, message, "writedata");
}

int main(int argc, char **argv){
    int fd;
    success = 0; total = 0;
    
    //map address space of fpga for software to access here
    if((fd = open("/dev/mem", ( O_RDWR | O_SYNC ) ) ) == -1 ) {
        printf( "ERROR: could not open \"/dev/mem\"...\n" );
        return( 1 );
    }
    
    void* virtual_base = mmap( NULL, LW_SIZE, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, LWHPS2FPGA_BASE);
    if( virtual_base == MAP_FAILED ) {
        printf( "ERROR: mmap() failed...\n" );
        close( fd );
        return(1);
    }
    
    //initialize the addresses
    md5ControlBase = virtual_base + ((uint32_t)( MD5_CONTROL_0_BASE) );
    md5DataBase = virtual_base + ((uint32_t)(MD5_DATA_0_BASE));
    
    uint32_t *md5Control = &md5Registers.md5Control.md5_start;
    uint32_t *md5Data = &md5Registers.md5Data.writedata;
    size_t i;
    
    for (i = 0; i < 4; i++)
    {
        printf("ControlBase offset = 0x%08x\n", md5ControlBase + i);
        md5Control[i] = md5ControlBase + i;
    }
    
    for (i = 0; i < 4; i++)
    {
        printf("ControlBase offset = 0x%08x\n", md5DataBase + i);
        md5Data[i] = md5DataBase + i;
    }
    
    printf("------>md5 HW in serial<-------\n");
    md5HardwareAlgorithm();
    printf( "\n=================================\n" );
    printf( "Compare software digest\n" );
    
    uint32_t message[] = {
        0x01680208, 0x13ab80bb, 0xcb8b2c30, 0xb9657582,
        0xa3793c48, 0x103f26be, 0x0b78dac4, 0x5c433348,
        0x4de99287, 0xeff0be7c, 0x00808533, 0x00000000,
        0x00000000, 0x00000000, 0x00000150, 0x00000000
    };
    
    size_t len = sizeof(message) / sizeof(message[0]);
    uint32_t digest[4];
    md5(message, digest);
    printf("MD5 Hash: %08x%08x%08x%08x\n", digest[3], digest[2], digest[1], digest[0]);
    printf("\n");
    printf( "\n=================================\n" );
    
    printf("------>md5 HW in parallel<-------\n");
    md5Reset();
    setRegister(md5Registers.md5Control.md5_wr, 0x1, "md5_wr");
    
    pthread_t threads[NUMBER_OF_ENGINES];
    ThreadArgs args[NUMBER_OF_ENGINES];
    
    for (i = 0; i < NUMBER_OF_ENGINES; i++)
    {
        args[i].engine = i;
        args[i].message = testSeq[i];
        pthread_create(&threads[i], NULL, (void *)writeEngine, &args[i]);
    }
    
    for (i = 0; i < NUMBER_OF_ENGINES; ++i)
        pthread_join(threads[i], NULL);
    
    clearRegister(md5Registers.md5Control.md5_wr, "md5_wr");
    md5Start();
    md5PrintDigest();
    
    printf( "\n=================================\n" );
    printf( "Compare software digest\n" );
    printf("MD5 Hash: %08x%08x%08x%08x\n", digest[3], digest[2], digest[1], digest[0]);
    printf("\n");
    
    // clean up our memory mapping and exit
    if( munmap( virtual_base, LW_SIZE) != 0 ) {
        print( "ERROR: munmap() failed...\n" );
        close( fd );
        return( 1 );
    }
    
    close( fd );
    return 0;
}

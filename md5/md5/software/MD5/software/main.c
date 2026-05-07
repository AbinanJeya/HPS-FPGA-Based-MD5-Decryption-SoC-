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
#define PRINT_ENABLED 1 // Set to 1 to enable printing, 0 to disable
#define print(fmt, ...) if (PRINT_ENABLED) printf(fmt, ##__VA_ARGS__)
#define CURRENT_TIME time(NULL)
#define NUMBER_OF_ENGINES 16
#define LW_SIZE 0x00200000
#define LWHPS2FPGA_BASE 0xff200000

const static size_t timeout = 60;
volatile uint32_t *md5_controlBase = NULL;
volatile uint32_t *md5_dataBase = NULL;
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
    struct md5_control {
        volatile uint32_t* md5_start;
        volatile uint32_t* md5_reset;
        volatile uint32_t* md5_wr;
        volatile uint32_t* md5_done;
    } md5_control;
    struct md5_data {
        volatile uint32_t* md5_writedata;
        volatile uint32_t* md5_writeaddr;
        volatile uint32_t* md5_readaddr;
        volatile uint32_t* md5_readdata;
    } md5_data;
} s_md5Registers;

s_md5Registers md5Registers = {
    .md5_control = {0},
    .md5_data = {0}
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
    setRegister(md5Registers.md5_control.md5_reset, 0x1, "md5_reset");
    clearRegister(md5Registers.md5_control.md5_wr, "md5_wr");
    clearRegister(md5Registers.md5_control.md5_start, "md5_start");
    clearRegister(md5Registers.md5_data.md5_writeaddr, "md5_writeaddr");
    clearRegister(md5Registers.md5_data.md5_readaddr, "md5_readaddr");
    clearRegister(md5Registers.md5_control.md5_reset, "md5_reset");
}

void md5Start(){
    print("===\t Starting Engine \t===\n");
    setRegister(md5Registers.md5_control.md5_start, 0x1, "md5_start");
    clearRegister(md5Registers.md5_control.md5_start, "md5_start");
    checkRegister(md5Registers.md5_control.md5_done, 0xFFFFFFFF, "md5_done");
}

void md5PrintDigest(){
    print("===\t Reading Digest \t===\n");
    uint32_t totalDigest[] = {0, 0, 0, 0};
    int i;
    for (i = 0; i < 4; i++)
    {
        setRegister(md5Registers.md5_data.md5_readaddr, i, "md5_readaddr");
        totalDigest[i] = alt_read_word(md5Registers.md5_data.md5_readdata);
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
    setRegister(md5Registers.md5_control.md5_wr, 0x1, "md5_wr");
    int i;
    for (i = 0; i < 16; i++)
    {
        print("Stage: %d\n", i);
        setRegister(md5Registers.md5_data.md5_writeaddr, i, "md5_writeaddr");
        setRegister(md5Registers.md5_data.md5_writedata, testSeq[i], "md5_writedata");
    }
    clearRegister(md5Registers.md5_control.md5_wr, "md5_wr");
    // Start engine
    md5Start();
    // Read digest
    md5PrintDigest();
}

void writeEngine(void* args){
    ThreadArgs* parameters = (ThreadArgs*)args;
    size_t engine = parameters->engine;
    uint32_t message = parameters->message;
    setRegister(md5Registers.md5_data.md5_writeaddr, engine, "md5_writeaddr");
    setRegister(md5Registers.md5_data.md5_writedata, message, "md5_writedata");
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
    md5_controlBase = virtual_base + ((uint32_t)( MD5_CONTROL_0_BASE) );
    md5_dataBase = virtual_base + ((uint32_t)(MD5_DATA_0_BASE));

    uint32_t *md5_control = &md5Registers.md5_control.md5_start;
    uint32_t *md5_data = &md5Registers.md5_data.md5_writedata;
    size_t i;

    for (i = 0; i < 4; i++)
    {
        printf("ControlBase offset = 0x%08x\n", md5_controlBase + i);
        md5_control[i] = md5_controlBase + i;
    }

    for (i = 0; i < 4; i++)
    {
        printf("ControlBase offset = 0x%08x\n", md5_dataBase + i);
        md5_data[i] = md5_dataBase + i;
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
    setRegister(md5Registers.md5_control.md5_wr, 0x1, "md5_wr");
    
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
    
    clearRegister(md5Registers.md5_control.md5_wr, "md5_wr");
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

/* ------------------------------------------------------------------------
 * phase4.c
 *
 * University of Arizona South
 * Computer Science 452
 *
 * Author: Cristian Rivera
 * Group: Cristian Rivera (Solo)
 *
 * ------------------------------------------------------------------------ */

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <usyscall.h>
#include <provided_prototypes.h>
#include "driver.h"

/* --------------------------- Globals ------------------------------------ */

/* semaphore to synchronize drivers and start3 */
static int running;
static int num_tracks[2];

/* Driver Process Table */
static struct driver_proc Driver_Table[MAXPROC];

/* PIDs for Disk Devices */
static int diskpids[DISK_UNITS];

/* Driver List(s) */
static driver_proc_ptr DriverList;

/* ------------------------- Prototypes ----------------------------------- */

//static int	ClockDriver(char *);
static int	DiskDriver(char *);
static void disk_size(sysargs *args_ptr);
static void DriverList_Insert(driver_proc_ptr dptr);
static void DriverList_Pop();
static int seek_proc_entry();

/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
     Name - start3
     Purpose - 
     Parameters - one, default arg
     Returns - one to indicate normal quit.
     Side Effects - none
     ----------------------------------------------------------------------- */
int start3(char *arg)
{
    char	name[128];
    //char  termbuf[10];
    char  buf[10];
    int		i;
    //int		clockPID;
    int		pid;
    int		status;
    /*
     * Check kernel mode here.
     */

    /* Assignment system call handlers */
    //sys_vec[SYS_SLEEP]    = sleep_first;
    //sys_vec[SYS_DISKREAD]   = disk_read_first;
    //sys_vec[SYS_DISKWRITE]  = disk_write_first;
    sys_vec[SYS_DISKSIZE]   = disk_size;

    //more for this phase's system call handlings

    /* Initialize the DriverList */
    DriverList = NULL;

    /* Initialize the phase 4 process table */
    for(i = 0; i < MAXPROC; i++)
    {
      Driver_Table[i].next_ptr      = NULL;
      Driver_Table[i].wake_time     = INIT_VAL;
      Driver_Table[i].been_zapped   = INIT_VAL;
      Driver_Table[i].operation     = DISK_NOT_IN_USE;
      Driver_Table[i].track_start   = INIT_VAL;
      Driver_Table[i].sector_start  = INIT_VAL;
      Driver_Table[i].num_sectors   = INIT_VAL;
      Driver_Table[i].disk_buf      = NULL;
    }

    /*
     * Create clock device driver 
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    //running = semcreate_real(0);
    //clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    /*if (clockPID < 0) 
    {
      console("start3(): Can't create clock driver\n");
	    halt(1);
    }*/
    
    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "running" once it is running.
     */

    //semp_real(running);

    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */

    for (i = 0; i < DISK_UNITS; i++) 
    {
        sprintf(buf, "%d", i);
        sprintf(name, "DiskDriver%d", i);
        diskpids[i] = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
        
        if (diskpids[i] < 0) 
        {
           console("start3(): Can't create disk driver %d\n", i);
           halt(1);
        }
    }
    //semp_real(running);
    //semp_real(running);


    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case names.
     */
    pid = spawn_real("start4", start4, NULL,  8 * USLOSS_MIN_STACK, 3);
    pid = wait_real(&status);

    /*
     * Zap the device drivers
     */
    //zap(clockPID);  // clock driver
    //join(&status); /* for the Clock Driver */
    return 0;
}/* start3 */

/* ------------------------------------------------------------------------
     Name - ClockDriver
     Purpose - none
     Parameters - none
     Returns - none
     Side Effects - none
     ----------------------------------------------------------------------- */
/*static int ClockDriver(char *arg)
{
    int result;
    int status;

    *
     * Let the parent know we are running and enable interrupts.
     *
    semv_real(running);
    psr_set(psr_get() | PSR_CURRENT_INT);
    while(! is_zapped()) 
    {
      result = waitdevice(CLOCK_DEV, 0, &status);
	
      if (result != 0)
      { 
        return 0;
      }
	
      *
	     * Compute the current time and wake up any processes
	     * whose time has come.
	     *
    }
    return 0;
}* ClockDriver */

static void disk_size(sysargs *args_ptr)
{

  printf("inside disk_size!\n");

  int unit = (int) args_ptr->arg1;
  int *sector = args_ptr->arg1;
  int *track = args_ptr->arg2;
  int *disk = args_ptr->arg3;

  disk_size_real(unit, sector, track, disk);

  //int index = seek_proc_entry();

  //if(index == -1)
  //{
    //printf("Driver queue full. terminating...\n");
    //Terminate(0);
  //}

  //Driver_Table[index].operation = DISK_TRACKS;
  //Driver_Table[index].unit = (int) args_ptr->arg1;
  //DriverList_Insert(&Driver_Table[index]);

  //DiskDriver((char *) Driver_Table[index].unit);

}/* disk_size */

int  disk_size_real(int unit, int *sector, int *track, int *disk)
{

  printf("Inside dis_size_real\n");

  int index = seek_proc_entry();

  if(index == -1)
    printf("Driver_Table full. should not see this\n");

  Driver_Table[index].operation = DISK_TRACKS;
  Driver_Table[index].unit = unit;
  //Driver_Table[index].sector_start = sector;
  //Driver_Table[index].track_start = track;
  //Driver_Table[index].disk_buf = disk;
  DriverList_Insert(&Driver_Table[index]);

  //switch to DiskDriver somehow
  
  
  *sector = Driver_Table[index].sector_start;
  *track  = Driver_Table[index].track_start;
  disk   = (int *) Driver_Table[index].disk_buf;

  //probably need to change later
  return 0;

}

/* ------------------------------------------------------------------------
     Name - DiskDriver
     Purpose - none
     Parameters - none
     Returns - none
     Side Effects - none
     ----------------------------------------------------------------------- */
static int DiskDriver(char *arg)
{
   int unit = atoi(arg);
   device_request my_request;
   int result;
   int status;

   printf("inside Disk Drivers\n");
   driver_proc_ptr current_req = NULL;

   //MboxCreate(0, sizeof(int));

   /*
   if (DEBUG4 && debugflag4)
      console("DiskDriver(%d): started\n", unit);
  */

   /* Get the number of tracks for this disk */
   my_request.opr  = DISK_TRACKS;
   my_request.reg1 = &num_tracks[unit];

   result = device_output(DISK_DEV, unit, &my_request);

   if (result != DEV_OK) 
   {
      console("DiskDriver %d: did not get DEV_OK on DISK_TRACKS call\n", unit);
      console("DiskDriver %d: is the file disk%d present???\n", unit, unit);
      halt(1);
   }

   waitdevice(DISK_DEV, unit, &status);
  

   printf("after wait device\n");


   while(DriverList != NULL)
   {
     current_req = DriverList;
     DriverList_Pop();
   }


   /*
   if (DEBUG4 && debugflag4)
      console("DiskDriver(%d): tracks = %d\n", unit, num_tracks[unit]);
  */

   //more code 
    return 0;
}/* DiskDriver */

/* ------------------------------------------------------------------------
     Name - DriverList_Insert
     Purpose - Helper function to insert a process into the top of the
               DriverList if empty, or the back of the DriverList if it is
               not.
     Parameters - one, a pointer to the Driver Process to be inserted into
                  the DriverList.
     Returns - none
     Side Effects - none
     ----------------------------------------------------------------------- */
static void DriverList_Insert(driver_proc_ptr dptr)
{

  printf("inside insert\n");

  driver_proc_ptr walker = NULL;

  if(DriverList == NULL)
    DriverList = dptr;
  else
  {
    walker = DriverList;

    while(walker->next_ptr != NULL)
      walker = walker->next_ptr;

    walker->next_ptr = dptr;
  }

}/* DriverList_Insert */

/* ------------------------------------------------------------------------
     Name - DriverList_Pop
     Purpose - Helper function that pops off the process from the top of the
               DriverList.
     Parameters - none
     Returns - none
     Side Effects - none
     ----------------------------------------------------------------------- */
static void DriverList_Pop()
{

  if(DriverList->next_ptr == NULL)
    DriverList = NULL;
  else
    DriverList = DriverList->next_ptr;
}/* DriverList_Pop */

/* ------------------------------------------------------------------------
     Name - seek_proc_entry
     Purpose - Helper function that finds the next empty entry in the
               Driver Process Table.
     Parameters - none
     Returns - one, the index of the next empty entry in the table, -1 if
               no empty entries exist.
     Side Effects - none
     ----------------------------------------------------------------------- */
static int seek_proc_entry()
{

  int i;

  for(i = 0; i < MAXPROC; i++)
  {
    if(Driver_Table[i].operation == DISK_NOT_IN_USE)
      return i;
  }

  return -1;
}/* seek_proc_entry */


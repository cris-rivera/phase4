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
static int running_u0;
static int running_u1;
static int num_tracks[NUM_DISK];
static int device_zapper;
static int size_disk;
static int size_sector;
static int size_track;

/* Driver Process Table */
static struct driver_proc Driver_Table[MAXPROC];

/* PIDs for Disk Devices */
static int diskpids[DISK_UNITS];

/* Driver List(s) */
static driver_proc_ptr DriverList_U0; //For Disk Unit 0
static driver_proc_ptr DriverList_U1; //For Disk Unit 1

/* ------------------------- Prototypes ----------------------------------- */

//static int	ClockDriver(char *);
static int	DiskDriver(char *);
static void disk_size(sysargs *args_ptr);
static void disk_write(sysargs *args_ptr);
static void disk_read(sysargs *args_ptr);
static void DriverList_Insert(driver_proc_ptr dptr, int unit_list);
static void DriverList_Pop(int unit);
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
    int   index;
    
    /*
     * Check kernel mode here.
     */

    /* Assignment system call handlers */
    //sys_vec[SYS_SLEEP]    = sleep_first;
    sys_vec[SYS_DISKREAD]   = disk_read;
    sys_vec[SYS_DISKWRITE]  = disk_write;
    sys_vec[SYS_DISKSIZE]   = disk_size;

    //more for this phase's system call handlings

    /* Initialize the DriverList */
    DriverList_U0 = NULL;
    DriverList_U1 = NULL;
    device_zapper = OFF;
    size_disk = 0;
    size_sector = 0;
    size_track = 0;

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
    running_u0 = semcreate_real(0);
    running_u1 = semcreate_real(0);
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

        //place disk device drivers on Driver_Table
        index = diskpids[i] % MAXPROC;
        Driver_Table[index].pid = diskpids[i];
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
    
    device_zapper = ON;
    join(&status);
    join(&status);

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

  int unit = (int) args_ptr->arg1;
  
  //temporary containers for int pointers
  int t_sec = 0;
  int t_track = 0;
  int t_disk = 0;

  int *sector = &t_sec;
  int *track = &t_track;
  int *disk = &t_disk;

  disk_size_real(unit, sector, track, disk);

  args_ptr->arg1 = (void *) *sector;
  args_ptr->arg2 = (void *) *track;
  args_ptr->arg3 = (void *) *disk;

}/* disk_size */

int  disk_size_real(int unit, int *sector, int *track, int *disk)
{
  int index = seek_proc_entry();
  int *running = NULL;
  
  if(unit == 0)
    running = &running_u0;
  else if(unit == 1)
    running = &running_u1;
  

  if(index == -1)
    printf("Driver_Table full. should not see this\n");

  Driver_Table[index].operation = DISK_TRACKS;
  Driver_Table[index].unit = unit;
  
  DriverList_Insert(&Driver_Table[index], Driver_Table[index].unit);
  semp_real(*running);

  *sector = size_sector;
  *track  = size_track;
  *disk   = size_disk;

  //probably need to change later
  return 0;

}

static void disk_write(sysargs *args_ptr)
{
  int unit      = (int) args_ptr->arg5;
  int track     = (int) args_ptr->arg3;
  int first     = (int) args_ptr->arg4;
  int sectors   = (int) args_ptr->arg2;
  void *buf     = args_ptr->arg1;
  int status    = 0;
  int result    = 0;

  if(first < 0 || first > 16)
    result = -1;

  status = disk_write_real(unit, track, first, sectors, buf);
  args_ptr->arg1 = (void *) status;
  args_ptr->arg4 = (void *) result;

}

int disk_write_real(int unit, int track, int first, int sectors, void *buffer)
{
  int index = seek_proc_entry();
  int *running = NULL;
 
  if(unit == 0)
    running = &running_u0;
  else if (unit == 1)
    running = &running_u1;
    

  if(index == -1)
    printf("Driver_table full. Should not see this...\n");

  Driver_Table[index].operation      = DISK_WRITE;
  Driver_Table[index].unit           = unit;
  Driver_Table[index].track_start    = track;
  Driver_Table[index].sector_start   = first;
  Driver_Table[index].num_sectors    = sectors;
  Driver_Table[index].disk_buf       = buffer;

  DriverList_Insert(&Driver_Table[index], Driver_Table[index].unit);
  
  semp_real(*running);
  
  if(is_zapped())
    quit(0);

  return 0;
}

static void disk_read(sysargs *args_ptr)
{
  int unit      = (int) args_ptr->arg5;
  int track     = (int) args_ptr->arg3;
  int first     = (int) args_ptr->arg4;
  int sectors   = (int) args_ptr->arg2;
  void *buf     = args_ptr->arg1;
  int status    = 0;

  status = disk_read_real(unit, track, first, sectors, buf);
  args_ptr->arg1 = (void *) status;

}

int disk_read_real(int unit, int track, int first, int sectors, void *buffer)
{
  int index = seek_proc_entry();
  int *running = NULL;

  if(unit == 0)
    running = &running_u0;
  else if (unit == 1)
    running = &running_u1;

  if(index == -1)
    printf("Driver_table full. Should not see this...\n");

  Driver_Table[index].operation     = DISK_READ;
  Driver_Table[index].unit          = unit;
  Driver_Table[index].track_start   = track;
  Driver_Table[index].sector_start  = first;
  Driver_Table[index].num_sectors   = sectors;
  Driver_Table[index].disk_buf      = buffer;

  DriverList_Insert(&Driver_Table[index], Driver_Table[index].unit);
  semp_real(*running);

  if(is_zapped())
    quit(0);

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
   int unit         = atoi(arg);
   device_request my_request;
   int result;
   int status;
   driver_proc_ptr  DriverList   = NULL;
   driver_proc_ptr  current_req  = NULL;
   int *running = NULL;
   int sectors = 0;
   int sector_mul = 0;
   int sector = 0;
   void * buffer = NULL;
   void *buffer2 = NULL;
   
   if(unit == 0)
     running = &running_u0;
   else if(unit == 1)
     running = &running_u1;
     
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

   size_sector = DISK_SECTOR_SIZE;
   size_track = DISK_TRACK_SIZE;
   size_disk = num_tracks[unit];

   while(device_zapper == OFF)
   {

     if(unit == 0)
       DriverList = DriverList_U0;
     else if(unit == 1)
       DriverList = DriverList_U1;
      
     if(DriverList != NULL)
     {
       
       current_req = DriverList;
       DriverList_Pop(unit);

       switch(current_req->operation)
       {
         case DISK_TRACKS: 
    
           semv_real(*running);
           break;
         
         case DISK_READ:
            
           sectors = current_req->num_sectors;
           sector_mul = 0;
           sector = 0;
           buffer = current_req->disk_buf;

           /* Disk Seek to move arm to the correct locaton */
           my_request.opr = DISK_SEEK;
           my_request.reg1 = (void *) current_req->track_start;

           device_output(DISK_DEV, unit, &my_request);
           waitdevice(DISK_DEV, unit, &status);

           while(sectors > 0)
           {
             sector = current_req->sector_start + sector_mul;

             if(sector == 16)
             {
               current_req->track_start  = current_req->track_start + 1;
               current_req->sector_start = 0;
               sector = 0;

               my_request.opr = DISK_SEEK;
               my_request.reg1 = (void *) current_req->track_start;

               device_output(DISK_DEV, unit, &my_request);
               waitdevice(DISK_DEV, unit, &status);
             }

             /* Once the location has been found, read from that location. */
             my_request.opr = DISK_READ;
             my_request.reg1 = (void *) sector;
             buffer2 = &buffer[sector_mul * 512];
             my_request.reg2 = buffer2;

             device_output(DISK_DEV, unit, &my_request);
             waitdevice(DISK_DEV, unit, &status);
            
             sector_mul++;
             sectors--;
           
           }

           current_req->status = status;
           semv_real(*running);

           break;

         case DISK_WRITE:

           sectors = current_req->num_sectors;
           sector_mul = 0;
           sector = 0;
           buffer = current_req->disk_buf;
 
           /* Disk Seek to move arm to correct location */
           my_request.opr = DISK_SEEK;
           my_request.reg1 = (void *) current_req->track_start;

           device_output(DISK_DEV, unit, &my_request);
           waitdevice(DISK_DEV, unit, &status);
          
           while(sectors > 0)
           {
             
             sector = current_req->sector_start + sector_mul;

             if(sector == 16)
             {
               current_req->track_start  = current_req->track_start + 1;
               current_req->sector_start = 0;
               sector = 0;

               my_request.opr = DISK_SEEK;
               my_request.reg1 = (void *) current_req->track_start;

               device_output(DISK_DEV, unit, &my_request);
               waitdevice(DISK_DEV, unit, &status);
             }

             /* Once location is found, write to that location. */
             my_request.opr = DISK_WRITE;
             my_request.reg1 = (void *) sector;
             buffer2 = &buffer[sector_mul * 512];
             my_request.reg2 =  buffer2;
        
             device_output(DISK_DEV, unit, &my_request);
             waitdevice(DISK_DEV, unit, &status);
            
             sector_mul++;
             sectors--;
           
           }
            
           current_req->status = status;
           semv_real(*running);

           break;
       }
     }
     else
       waitdevice(CLOCK_DEV, 0, &status);

   }
   
   if(device_zapper == ON)
     quit(0);

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
static void DriverList_Insert(driver_proc_ptr dptr, int unit_list)
{
  driver_proc_ptr walker = NULL;
  
  if(unit_list == 0)
  {
    if(DriverList_U0 == NULL)
      DriverList_U0 = dptr;
    else
    {
      walker = DriverList_U0;

      while(walker->next_ptr != NULL)
        walker = walker->next_ptr;

      walker->next_ptr = dptr;
    }
  }
  else if(unit_list == 1)
  {
    if(DriverList_U1 == NULL)
      DriverList_U1 = dptr;
    else
    {
      walker = DriverList_U1;

      while(walker->next_ptr != NULL)
        walker = walker->next_ptr;

      walker->next_ptr = dptr;
    }
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
static void DriverList_Pop(int unit)
{

  if(unit == 0)
  {
    if(DriverList_U0->next_ptr == NULL)
      DriverList_U0 = NULL;
    else
      DriverList_U0 = DriverList_U0->next_ptr;
  }
  else if(unit == 1)
  {
    if(DriverList_U1->next_ptr == NULL)
      DriverList_U1 = NULL;
    else
      DriverList_U1 = DriverList_U1->next_ptr;
  }

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


#define INIT_VAL 0
#define DISK_NOT_IN_USE 4
#define NUM_DISK 2 //Number of Disk Devices present
#define OFF 5
#define ON 6

typedef struct driver_proc * driver_proc_ptr;

struct driver_proc {
   driver_proc_ptr    next_ptr;
   int                wake_time;    /* for sleep syscall */
   int                been_zapped;

   /* Used for disk requests */
   int        operation;    /* DISK_READ, DISK_WRITE, DISK_SEEK, DISK_TRACKS */
   int        track_start;
   int        sector_start;
   int        num_sectors;
   void       *disk_buf;
   int        unit;
   int        pid;
   int        mbox_id;

   //more fields to add

};

/**************************************
	
	Authors: 
	1)	Grzegorz Kakareko
	email: gk15b@my.fsu.edu
	2)	Jordan Camejo
	email: 
	3)	Grace Bunch
	email: gib14@my.fsu.edu

**************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/delay.h> 
#include <linux/list.h>

#include <linux/kernel.h>
#include <linux/linkage.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Implementation of a scheduling algorithm for a hotel elevator");

/* Taken from animal */
#define ENTRY_NAME "elevator_list"
#define ENTRY_SIZE 1000
#define PERMS 0644
#define PARENT NULL

/* Four types of people */
#define ADULT 0
#define CHILD 1
#define ROOM_SERVICE 2
#define BELLHOP 3
#define MAX_EL_WEIGHT 15
#define MAX_PASSANGER_UNIT 10

/* Waiting time */
#define ELEVAITOR_WAIT 2	//	elevator must wait for 2.0 seconds when moving between floors
#define FLOOR_WAIT 1		// 	when moving between floors, and it must wait for 1.0
static struct file_operations fops;

static char *message;
static int read_p;

/*	System calls functions defined in the other file
	sys_call.c*/
extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int, int, int);
extern int (*STUB_elevator_run)(void);

/*	Definition of those functions	*/
int start_elevator(void);
int issue_request(int, int, int);
int stop_elevator(void);

/* The global structure elevator 
	that defines the elevator*/
struct {
	int status;				// 0- elevator is not moving, 1- elevator is moving
	int current_floor;		// Number of the current flor
	int next_floor;			// Number of the next floor
	int total_weight;		// The curent weight in the elevator, 0 at the begining
	int num_passangers;		// The current number of pasengers in the elevator
	int used;
	int loading;			// 1-is loading. 0 is not	
	struct list_head el_list;
} elevator;


/* This structure holds the characteristics 
	of the passanger */
typedef struct passenger{
	double weight;
	double pas_unit;
	int start;		// Where he entered the elevator
	int dest;		// Where is he coming
	const char *name;	// The name of the passanger probably no need to store
	struct list_head list;
} Passenger;

/********************************************************************/

/*	list_head floors- represents the people in the waiting lists */
struct list_head floors[10];

/*	init_floors()- this is initialize the flors by adding the empty lists  */
int init_floors()
{	
	int i;
	for(i=0;i<10,i++)
		INIT_LIST_HEAD(&flor[i]);
}

/*
add_person_to_waiting_list(int type, int start_floor, int dest_floor)
adds person to the waiting list, the input data are the type of the person,
where enters the floors and where is cooming
The types of the eprsons are as follow:
	• An adult counts as 1 passenger unit and 1 weight unit
	• A child counts as 1 passenger unit and 1⁄2 weight unit
	• Room service counts as 2 passenger units and 2 weight units
	• A bellhop counts as 2 passengers unit and 3 weight unit

/*
334- issue_request(int passenger_type, int start_floor, int destination_floor)
Description: Creates a passenger of type passenger_type at start_floor that wishes to go to
destination_floor. This function returns 1 if the request is not valid (one of the variables is out
of range), and 0 otherwise. A passenger type can be translated to an int as follows:
• Adult = 1
• Child = 2
• Room service = 3
• Bellhop = 4

*/
//int add_person_to_waiting_list(int type, int start_floor, int dest_floor) {
issue_request(int passenger_type, int start_floor, int destination_floor)
{
	
	/* Checking if th inputs are right*/

	if((type > 4) || (type < 0) || (start_floor > 10) || (start_floor < 1)\ 
		|| (dest_floor > 10) || (dest_floor < 1))
	{
		return 1;
	}

	double weight;
	double pas_unit;
	int start;
	int dest;
	char *name;
	Passenger *a; // this is ponter so elements need to be refered by ->

	switch (type) {
		case ADULT:
			weight = 1;
			pas_unit = 1; 
			name = "adult";
			break;
		case CHILD:
			weight = 0.5;
			pas_unit = 1;
			name = "child";
			break;
		case ROOM_SERVICE:
			weight = 2;
			pas_unit = 2;
			name = "room_service";
			break;
		case BELLHOP:
			weight = 3;
			pas_unit = 2;
			name = "bellhop";
			break;
		default:
			return -1;
	}
	start = start_floor;
	dest = dest_floor; 
	a = kmalloc(sizeof(passenger) * 1, __GFP_RECLAIM);
	if (a == NULL)
		return -ENOMEM;
	a->weight = weight;
	a->pas_unit = pas_unit;
	a->start = start_floor;
	a->dest = dest_floor;
	a->name = name;
	
	/* Adding the element to the list in the elevator*/
	//list_add(&a->list, &animals.list); /* insert at front of list */
	list_add_tail(&a->list, &floors[start_floor]); /* insert at back of list */
	return 0;
}

/*	void add_person_add_elevator(int floor_num)- delets from the waiting list 
	if satysfies the conditions and move the element to the list in the elevator
*/
void add_person_to_elevator(int floor_num)
{
	struct list_head *temp;
	struct list_head *dummy;
	Passenger *a;

	/*	Compared to the animal example the moving list was deleted and the elemtns
		are directly moved to the list in th elevator_*/
	/*	Sys function to move elements */
	list_for_each_safe(temp, dummy, &floors[floor_num].list) 
	{ 
		/*	Iterate through the list and if cond satysfied add to the lost*/
		a = list_entry(temp, Passenger, list);

		if (((a->weight + elevator.total_weight)< MAX_EL_WEIGHT) &&\ 
			((a->pas_unit + elevator.num_passangers)<MAX_PASSANGER_UNIT) )
		{
			list_move_tail(temp, &elevator.el_list); // Move element from waiting list to the list
			/*	Update the informations in the elevator */
			elevator.total_weight += a->weight;
			elevator.num_passangers += a->pas_unit;
		}
	}	
}


/*	void delete_passanger_from_elevator(floor_number)- delete the person from elevator list
	if the passanger is leaving the elevator */
void delete_passanger_from_elevator(int floor_number) 
{
	/*	Modyfication of the void delete_animals(int type) from random_animals.c*/
	struct list_head move_list;
	struct list_head *temp;
	struct list_head *dummy;
	int i;
	Passenger *a;

	INIT_LIST_HEAD(&move_list);

	/* move items to a temporary list to illustrate movement */
	//list_for_each_prev_safe(temp, dummy, &animals.list) { /* backwards */
	list_for_each_safe(temp, dummy, &elevator.el_list) { /* forwards */
		a = list_entry(temp, Passenger, list);
		/*	We iterate if satysfies we move from elevator and after that we delete */
		if (a->dest == floor_number) 
		{
			//list_move(temp, &move_list); /* move to front of list */
			list_move_tail(temp, &move_list); /* move to back of list */

			elevator.total_weight -= a->weight;
			elevator.num_passangers -= a->pas_unit;
			elevator.used += 1;
		}

	}	

	/* print stats of list to syslog, entry version just as example (not needed here) */
	i = 0;
	//list_for_each_entry_reverse(a, &move_list, list) { /* backwards */
	list_for_each_entry(a, &move_list, list) { /* forwards */
		/* can access a directly e.g. a->id */
		i++;
	}
	//printk(KERN_NOTICE "animal type %d had %d entries", type, i);
	printk(KERN_NOTICE "%d Passanger had left the elevator", i);
	/* free up memory allocation of Animals */
	//list_for_each_prev_safe(temp, dummy, &move_list) { /* backwards */
	list_for_each_safe(temp, dummy, &move_list) { /* forwards */
		a = list_entry(temp, Passanger, list);
		list_del(temp);	/* removes entry from list */
		kfree(a);
	}

}

/*
333 int start_elevator(void)-
Description: Activates the elevator for service. From that point onward, the elevator exists and will
begin to service requests. This system call will return 1 if the elevator is already active, 0 for a
successful elevator start, and -ERRORNUM if it could not initialize (e.g. -ENOMEM if it couldn’t
allocate memory). Initialize an elevator as follows:
• State: IDLE
• Current floor: 1•
Current load: 0 passenger units, 0 weight units
*/
int start_elevator(void)
{
	if(elevator.status==1)
	{ // Elevator is active

		return 1;
	}
	else
	{
	// Crate elevator
		elevator.status = 1;
		elevator.current_floor = 1;
		elevator.total_weight = 0;
		elevator.num_passangers = 0;
		elevator.used = 0;
		elevator.loading = 0;
		INIT_LIST_HEAD(&el_list);

		return 0;
	}
}

/*	---------The draft of the function that moves elevator --------------	*/
int elevator_run(void){
	Passenger *p;
	
	if ( list_empty(&elevator.el_list) )
	{
		if (elevator.current_floor != 1)
		{
			/*	Elevator is empty so is going to the first floor */
			ssleep((elevator.current_floor -1) * FLOOR_WAIT); // elevator is moving, wait
			elevator.current_floor = 1;
		}
		/*	Elevator is empty and on 1 floor*/
		int i;
		for (i = 0; i < 10; ++i){
			if (!list_empty(&floors[i])){
				if (i != 0)
					ssleep(i); // elevator is moving, wait
				/*	ELEVAITOR_WAIT 2	
					FLOOR_WAIT 1	*/
				elevator.current_floor = (i + 1);
				elevator.loading = 1;
				ssleep(ELEVAITOR_WAIT); // loading, wait

				add_person_to_elevator(i);

				p = list_first_entry(&elevator.el_list, Passenger, list);
				elevator.next_floor = p -> dest;
				if (elevator.next_floor > elevator.current_floor){
					// elevator.direction = UP;
					// elevator.status = UP;
				}	
				else{
					// elevator.direction = DOWN;
					// elevator.status = DOWN;
				}
				break; // break out of for loop
			}	
		}
	}
	else{
		//(elevator.next_floor-elevator.current_floor)
		if ((elevator.next_floor-elevator.current_floor)>0)
		{
			ssleep(elevator.next_floor-elevator.current_floor);
			elevator.current_floor = elevator.next_floor;
		}
		else{
			ssleep(elevator.current_floor - elevator.next_floor);
			elevator.current_floor = elevator.next_floor;
		}
		elevator.loading = 1;
		ssleep(ELEVAITOR_WAIT); // loading, wait
		delete_passanger_from_elevator(elevator.next_floor);
	}

	return 0;	
}
/*	----------------------------------------------	*/



/*
335- stop_elevator(void)
Description: Deactivates the elevator. At this point, this elevator will process no more new requests
(that is, passengers waiting on floors). However, before an elevator completely stops, it must offload all
of its current passengers. Only after the elevator is empty may it be deactivated (state = OFFLINE).
This function returns 1 if the elevator is already in the process of deactivating, and 0 otherwise.
*/

// int stop_elevator(void)
// {
	

// }



int print_elevator(void) 
{
	/*
	int status;				// 0- elevator is not moving, 1- elevator is moving
	int current_floor;		// Number of the current flor
	int next_floor;			// Number of the next floor
	int total_weight;		// The curent weight in the elevator, 0 at the begining
	int num_passangers;		// The current number of pasengers in the elevator
	int used;
	*/
	if (elevator.status == 1)
		sprintf(message, "used: %d\n status: %d\n", elevator.used, elevator.status);
	else
		sprintf(message, "OFFLINE");
	return 0;

}
/*
Here I check if I Delete the function from person 
list if so I add move it to the list in the 
elevaor
*/


/********************************************************************/
/*	int elevator_proc_open(struct inode *sp_inode, struct file *sp_file)- 
	This function works similar to the python logfiles, if there is
	an error prints the error to he message, if it works fine it prints 
	the number of people that were seviced	*/
int elevator_proc_open(struct inode *sp_inode, struct file *sp_file) {
	
	read_p = 1;
	message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL) 
	{
		// Print the warning
		printk(KERN_WARNING "elevator_proc_open");
		return -ENOMEM;
	}
	
	// add_animal(get_random_int() % NUM_ANIMAL_TYPES);
	return print_elevator();
}

ssize_t elevator_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset) {
	int len = strlen(message);
	
	read_p = !read_p;
	if (read_p)
		return 0;
		
	copy_to_user(buf, message, len);
	return len;
}

int elevator_proc_release(struct inode *sp_inode, struct file *sp_file) {
	kfree(message);
	return 0;
}

/********************************************************************/

static int elevator_init(void) {
	fops.open = elevator_proc_open;
	fops.read = elevator_proc_read;
	fops.release = elevator_proc_release;

	/* I am not sure why I need to assignt pointers for system calls*/
	STUB_start_elevator = start_elevator;
	STUB_issue_request = issue_request;
	STUB_elevator_run = elevator_run;
	
	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)) {
		printk(KERN_WARNING "elevator_init\n");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}

	// elevator.status = 0;
	// elevator.direction = 0;
	// elevator.total_weight = 0;
	// elevator.num_passangers = 0;
	// INIT_LIST_HEAD(&elevator.list);
	
	return 0;
}
module_init(elevator_init);

static void elevator_exit(void) 
{
	/* The pointers defined in init I am setting to Null*/
	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_elevator_run = NULL;

	// delete_elevator(ANIMAL_HAMSTER);
	// delete_elevator(ANIMAL_CAT); 
	// delete_elevator(ANIMAL_DOG);
	remove_proc_entry(ENTRY_NAME, NULL);

	/*	Debuging */
	printk(KERN_NOTICE "Removing /proc/%s\n", ENTRY_NAME);
}
module_exit(animal_exit);

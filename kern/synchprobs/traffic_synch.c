#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
void changeLights(Direction origin);
/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct lock *intersectionLock;
static struct cv *northCV;
static struct cv *eastCV;
static struct cv *southCV;
static struct cv *westCV;
volatile bool northFlag;
volatile bool eastFlag;
volatile bool southFlag;
volatile bool westFlag;
volatile bool firstRun;
volatile int intersectionCount;
volatile int northCount;
volatile int eastCount;
volatile int southCount;
volatile int westCount;
/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void intersection_sync_init(void)
{
	/* replace this default implementation with your own implementation */
	intersectionLock = lock_create("intersectionLock");
	if (intersectionLock == NULL) {
		panic("could not create intersection lock");
	}
	northCV = cv_create("North");
	if (northCV == NULL) {
		panic("could not create North CV");
	}	
	eastCV = cv_create("East");
	if (eastCV == NULL) {
		panic("could not create East CV");
	}
	southCV = cv_create("South");
	if (southCV == NULL) {
		panic("could not create South CV");
	}
	westCV = cv_create("West");
	if (westCV == NULL) {
		panic("could not create West CV");
	}
	northFlag = false;
	eastFlag = false;
	westFlag = false;
	southFlag = false;
	firstRun = true;
	intersectionCount = 0;
	northCount = 0;
	eastCount = 0;
	southCount = 0;
	westCount = 0;	
	return; 
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{

	/* replace this default implementation with your own implementation */
	KASSERT(intersectionLock != NULL);
	lock_destroy(intersectionLock);
	KASSERT(northCV != NULL);
	cv_destroy(northCV);
	KASSERT(eastCV != NULL);
	cv_destroy(eastCV);
	KASSERT(southCV != NULL);
	cv_destroy(southCV);
	KASSERT(westCV != NULL);
	cv_destroy(westCV);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
	(void)destination; /* avoid compiler complaint about unused parameter */
	KASSERT(intersectionLock != NULL);
	KASSERT(northCV != NULL);
	KASSERT(southCV != NULL);
	KASSERT(westCV != NULL);
	KASSERT(eastCV != NULL);
	lock_acquire(intersectionLock);
	if (firstRun) {
		firstRun = false;
		switch (origin) {
			case north: 
				northFlag = true;
				break;
			case east: 
				eastFlag = true; 
				break;
			case west:
				westFlag = true; 
				break;
			case south:
				southFlag = true;
				break;
		}
	}
	switch (origin) {
		case north:
			//kprintf("Incremented North Count: %d\n", northCount+1);
			++northCount;
			while (!northFlag){
				//kprintf("Going to sleep, have orgin North\n");			
				cv_wait(northCV, intersectionLock);
			}
			//kprintf("Passed through Intersection with origin North\n");
			break;

		case east:
			//kprintf("Incremented East Count: %d\n", eastCount+1);
			++eastCount;
			while (!eastFlag){
				//kprintf("Going to sleep, have orgin East\n");
				cv_wait(eastCV, intersectionLock);
			}
			//kprintf("Passed through Intersection with origin East\n");
			break;

		case south:
			//kprintf("Incremented South Count: %d\n", southCount+1);
			++southCount;
			while (!southFlag){
				//kprintf("Going to sleep, have orgin South\n");
				cv_wait(southCV, intersectionLock);
			}
			//kprintf("Passed through Intersection with origin South\n");
			break;

		case west:
			//kprintf("Incremented West Count: %d\n", westCount+1);
			++westCount;
			while (!westFlag){
				//kprintf("Going to sleep, have orgin West\n");
				cv_wait(westCV, intersectionLock);
			}
			//kprintf("Passed through Intersection with origin West\n");
			break;
	}
	//kprintf("Incremented Intersection Count: %d\n", intersectionCount+1);
	++intersectionCount;
	lock_release(intersectionLock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
changeLights(Direction origin) 
{
	switch (origin) {
		case north:
			if (northCount == 0) {
				northFlag = false;
			}
			else {
				panic("Northcount is %d when it should be 0", northCount);
			}
			if (eastCount != 0) {
				eastFlag = true;
				cv_broadcast(eastCV, intersectionLock);
			}
			else if (southCount != 0) {
				southFlag = true;
cv_broadcast(southCV, intersectionLock);
			}
			else if (westCount != 0) {
				westFlag = true;
cv_broadcast(westCV, intersectionLock);
			}
			else {
				//kprintf("Intersection is truly empty\n");
				firstRun = true;
			}
			break;

		case east:
			if (eastCount == 0) {
				eastFlag = false;
			}
			else {
				panic("eastcount is %d when it should be 0", eastCount);
			}
			if (southCount != 0) {
				southFlag = true;
cv_broadcast(southCV, intersectionLock);
			}
			else if (westCount != 0) {
				westFlag = true;
cv_broadcast(westCV, intersectionLock);
			}
			else if (northCount != 0) {
				northFlag = true;
cv_broadcast(northCV, intersectionLock);
			}
			else {
				//kprintf("Intersection is truly empty\n");
				firstRun = true;
			}
			break;

		case south:
			if (southCount == 0) {
				southFlag = false;
			}
			else {
				panic("Southcount is %d when it should be 0", southCount);
			}
			if (westCount != 0) {
				westFlag = true;
cv_broadcast(westCV, intersectionLock);
			}
			else if (northCount != 0) {
				northFlag = true;
cv_broadcast(northCV, intersectionLock);
			}
			else if (eastCount != 0) {
				eastFlag = true;
cv_broadcast(eastCV, intersectionLock);
			}
			else {
				//kprintf("Intersection is truly empty\n");
				firstRun = true;
			}
			break;

		case west:
			if (westCount == 0) {
				westFlag = false;
			}
			else {
				panic("Westcount is %d when it should be 0", westCount);
			}
			if (northCount != 0) {
				northFlag = true;
cv_broadcast(northCV, intersectionLock);
			}
			else if (eastCount != 0) {
				eastFlag = true;
cv_broadcast(eastCV, intersectionLock);
			}
			else if (southCount != 0) {
				southFlag = true;
cv_broadcast(southCV, intersectionLock);
			}
			else {
				//kprintf("Intersection is truly empty\n");
				firstRun = true;
			}
			break;
	}
}


void
intersection_after_exit(Direction origin, Direction destination) 
{
	KASSERT(intersectionLock != NULL);
	KASSERT(northCV != NULL);
	KASSERT(southCV != NULL);
	KASSERT(westCV != NULL);
	KASSERT(eastCV != NULL);
	(void)destination;
	lock_acquire(intersectionLock);
	//kprintf("Decremented Intersection Count: %d\n", intersectionCount-1);
	--intersectionCount;
	switch(origin) {
		case north:
			//kprintf("Decremented North Count: %d\n", northCount-1);
			--northCount;
			if (intersectionCount == 0) {
//kprintf("Calling Change of lights\n");
				changeLights(origin);
			}
			break;
		case east:
			//kprintf("Decremented East Count: %d\n", eastCount-1);
			--eastCount;
			if (intersectionCount == 0) {
//kprintf("Calling Change of lights\n");
				changeLights(origin);
			}
			break;
		case south:
			//kprintf("Decremented South Count: %d\n", southCount-1);
			--southCount;
			if (intersectionCount == 0) {
//kprintf("Calling Change of lights\n");
				changeLights(origin);
			}
			break;
		case west:
			//kprintf("Decremented West Count: %d\n", westCount-1);
			--westCount;
			if (intersectionCount == 0) {
//kprintf("Calling Change of lights\n");
				changeLights(origin);
			}
			break;
	}
	lock_release(intersectionLock);
}

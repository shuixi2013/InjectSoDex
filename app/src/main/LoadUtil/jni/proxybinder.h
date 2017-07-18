/*
 * proxybinder.h
 *
 *  Created on: 2014年6月24日
 *      Author: boyliang
 */


//#ifndef _PROXYBINDER_H_
//#define _PROXYBINDER_H_

#include <pthread.h>
#include <stddef.h>

void Main();


static void* _main(void*){
	Main();
	return NULL;
}

class EntryClass {
public:

	EntryClass() {
		pthread_t tid;
		pthread_create(&tid, NULL, _main, NULL);
		pthread_detach(tid);
	}

} boy;

//#endif //end of _PROXYBINDER_H_

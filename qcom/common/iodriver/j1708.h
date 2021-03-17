/* J1708 IOCONTROL driver
   Ruslan Sirota <ruslan.sirota@micronet-inc.com
   */


#ifndef __MIC__J1708_H__
#define __MIC__J1708_H__

struct j1708_thread_context {
	char name[PATH_MAX];
    int run;
};

void * j1708_proc(void * cntx);

#endif //__MIC_J1708_H__


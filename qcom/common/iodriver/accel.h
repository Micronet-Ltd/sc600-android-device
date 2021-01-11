
#ifndef __MIC_ACCELOROMETER_H__
#define __MIC_ACCELOROMETER_H__

struct accel_thread_context
{
	char name[PATH_MAX];
};

extern void * accel_proc(void * cntx);

#endif //__MIC_ACCELOROMETER_H__


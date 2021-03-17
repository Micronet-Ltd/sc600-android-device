
#ifndef __MIC_MISC_H__
#define __MIC_MISC_H__

#define err_abort(code, text) do { \
		fprintf(stderr, "%s at \"%s\":%d: %s\n", \
				text, __FILE__, __LINE__, strerror(code)); \
		abort(); \
	} while(0)
#define errno_abort(text) do { \
		fprintf(stderr, "%s at \"%s\":%d: %s\n", \
				text, __FILE__, __LINE__, strerror(errno)); \
		abort(); \
	} while(0)

#define mcu_ver_prop    "hw.build.version.mcu"
#define fpga_ver_prop   "hw.build.version.fpga"
#define prop_unknown    "unknown"

#endif //__MIC_MISC_H__


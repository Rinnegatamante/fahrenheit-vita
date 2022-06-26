/* main.c -- World of Goo .so loader
 *
 * Copyright (C) 2021 Andy Nguyen
 * Copyright (C) 2022 Rinnegatamante
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.	See the LICENSE file for details.
 */

#include <vitasdk.h>
#include <kubridge.h>
#include <vitashark.h>
#include <vitaGL.h>
#include <zlib.h>
#include <fnmatch.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h>
#include <SLES/OpenSLES.h>

#include <malloc.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <wchar.h>
#include <wctype.h>

#include <math.h>
#include <math_neon.h>

#include <errno.h>
#include <ctype.h>
#include <setjmp.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "main.h"
#include "config.h"
#include "dialog.h"
#include "so_util.h"
#include "ogg_patch.h"
#include "vorbis_patch.h"
#include "sha1.h"
#include "libc_bridge.h"

uint8_t ps2_mode = 1;

extern const char *BIONIC_ctype_;
extern const short *BIONIC_tolower_tab_;
extern const short *BIONIC_toupper_tab_;

static char fake_vm[0x1000];
static char fake_env[0x1000];

int file_exists(const char *path) {
	SceIoStat stat;
	return sceIoGetstat(path, &stat) >= 0;
}

int sceLibcHeapSize = MEMORY_SCELIBC_MB * 1024 * 1024;
int _newlib_heap_size_user = MEMORY_NEWLIB_MB * 1024 * 1024;

unsigned int _pthread_stack_default_user = 1 * 1024 * 1024;

so_module fahrenheit_mod, stdcpp_mod, iconv_mod, obbvfs_mod;

void *__wrap_memcpy(void *dest, const void *src, size_t n) {
	return sceClibMemcpy(dest, src, n);
}

void *__wrap_memmove(void *dest, const void *src, size_t n) {
	return sceClibMemmove(dest, src, n);
}

void *__wrap_memset(void *s, int c, size_t n) {
	return sceClibMemset(s, c, n);
}

char *getcwd_hook(char *buf, size_t size) {
	strcpy(buf, DATA_PATH);
	return buf;
}

int debugPrintf(char *text, ...) {
#ifdef DEBUG
	va_list list;
	static char string[0x8000];

	va_start(list, text);
	vsprintf(string, text, list);
	va_end(list);

	SceUID fd = sceIoOpen("ux0:data/goo_log.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
	if (fd >= 0) {
		sceIoWrite(fd, string, strlen(string));
		sceIoClose(fd);
	}
#endif
	return 0;
}

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
	va_list list;
	static char string[0x8000];

	va_start(list, fmt);
	vsprintf(string, fmt, list);
	va_end(list);

	printf("[LOG] %s: %s\n", tag, string);

	return 0;
}

int __android_log_write(int prio, const char *tag, const char *fmt, ...) {
	va_list list;
	static char string[0x8000];

	va_start(list, fmt);
	vsprintf(string, fmt, list);
	va_end(list);

	printf("[LOGW] %s: %s\n", tag, string);

	return 0;
}

int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list list) {
	static char string[0x8000];

	vsprintf(string, fmt, list);
	va_end(list);

	printf("[LOGV] %s: %s\n", tag, string);
	return 0;
}

int ret0(void) {
	return 0;
}

int ret1(void) {
	return 1;
}

int pthread_mutex_init_fake(pthread_mutex_t **uid, const pthread_mutexattr_t *mutexattr) {
	pthread_mutex_t *m = vglCalloc(1, sizeof(pthread_mutex_t));
	if (!m)
		return -1;

	const int recursive = (mutexattr && *(const int *)mutexattr == 1);
	*m = recursive ? PTHREAD_RECURSIVE_MUTEX_INITIALIZER : PTHREAD_MUTEX_INITIALIZER;

	int ret = pthread_mutex_init(m, mutexattr);
	if (ret < 0) {
		free(m);
		return -1;
	}

	*uid = m;

	return 0;
}

int pthread_mutex_destroy_fake(pthread_mutex_t **uid) {
	if (uid && *uid && (uintptr_t)*uid > 0x8000) {
		pthread_mutex_destroy(*uid);
		vglFree(*uid);
		*uid = NULL;
	}
	return 0;
}

int pthread_mutex_lock_fake(pthread_mutex_t **uid) {
	int ret = 0;
	if (!*uid) {
		ret = pthread_mutex_init_fake(uid, NULL);
	} else if ((uintptr_t)*uid == 0x4000) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		ret = pthread_mutex_init_fake(uid, &attr);
		pthread_mutexattr_destroy(&attr);
	} else if ((uintptr_t)*uid == 0x8000) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
		ret = pthread_mutex_init_fake(uid, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	if (ret < 0)
		return ret;
	return pthread_mutex_lock(*uid);
}

int pthread_mutex_trylock_fake(pthread_mutex_t **uid) {
	int ret = 0;
	if (!*uid) {
		ret = pthread_mutex_init_fake(uid, NULL);
	} else if ((uintptr_t)*uid == 0x4000) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		ret = pthread_mutex_init_fake(uid, &attr);
		pthread_mutexattr_destroy(&attr);
	} else if ((uintptr_t)*uid == 0x8000) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
		ret = pthread_mutex_init_fake(uid, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	if (ret < 0)
		return ret;
	return pthread_mutex_trylock(*uid);
}

int pthread_mutex_unlock_fake(pthread_mutex_t **uid) {
	int ret = 0;
	if (!*uid) {
		ret = pthread_mutex_init_fake(uid, NULL);
	} else if ((uintptr_t)*uid == 0x4000) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		ret = pthread_mutex_init_fake(uid, &attr);
		pthread_mutexattr_destroy(&attr);
	} else if ((uintptr_t)*uid == 0x8000) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
		ret = pthread_mutex_init_fake(uid, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	if (ret < 0)
		return ret;
	return pthread_mutex_unlock(*uid);
}

int pthread_cond_init_fake(pthread_cond_t **cnd, const int *condattr) {
	pthread_cond_t *c = calloc(1, sizeof(pthread_cond_t));
	if (!c)
		return -1;

	*c = PTHREAD_COND_INITIALIZER;

	int ret = pthread_cond_init(c, NULL);
	if (ret < 0) {
		free(c);
		return -1;
	}

	*cnd = c;

	return 0;
}

int pthread_cond_broadcast_fake(pthread_cond_t **cnd) {
	if (!*cnd) {
		if (pthread_cond_init_fake(cnd, NULL) < 0)
			return -1;
	}
	return pthread_cond_broadcast(*cnd);
}

int pthread_cond_signal_fake(pthread_cond_t **cnd) {
	if (!*cnd) {
		if (pthread_cond_init_fake(cnd, NULL) < 0)
			return -1;
	}
	return pthread_cond_signal(*cnd);
}

int pthread_cond_destroy_fake(pthread_cond_t **cnd) {
	if (cnd && *cnd) {
		pthread_cond_destroy(*cnd);
		vglFree(*cnd);
		*cnd = NULL;
	}
	return 0;
}

int pthread_cond_wait_fake(pthread_cond_t **cnd, pthread_mutex_t **mtx) {
	if (!*cnd) {
		if (pthread_cond_init_fake(cnd, NULL) < 0)
			return -1;
	}
	return pthread_cond_wait(*cnd, *mtx);
}

int pthread_cond_timedwait_fake(pthread_cond_t **cnd, pthread_mutex_t **mtx, const struct timespec *t) {
	if (!*cnd) {
		if (pthread_cond_init_fake(cnd, NULL) < 0)
			return -1;
	}
	sceKernelDelayThread(1000);
	return 0;
}

int clock_gettime_hook(int clk_id, struct timespec *t) {
	struct timeval now;
	int rv = gettimeofday(&now, NULL);
	if (rv)
		return rv;
	t->tv_sec = now.tv_sec;
	t->tv_nsec = now.tv_usec * 1000;

	return 0;
}

int pthread_cond_timedwait_relative_np_fake(pthread_cond_t **cnd, pthread_mutex_t **mtx, struct timespec *ts) {
	if (!*cnd) {
		if (pthread_cond_init_fake(cnd, NULL) < 0)
			return -1;
	}
	
	if (ts != NULL) {
		struct timespec ct;
		clock_gettime_hook(0, &ct);
		ts->tv_sec += ct.tv_sec;
		ts->tv_nsec += ct.tv_nsec;
	}
	
	pthread_cond_timedwait(*cnd, *mtx, ts); // FIXME
	return 0;
}

int pthread_create_fake(pthread_t *thread, const void *unused, void *entry, void *arg) {
	return pthread_create(thread, NULL, entry, arg);
}

int pthread_once_fake(volatile int *once_control, void (*init_routine)(void)) {
	if (!once_control || !init_routine)
		return -1;
	if (__sync_lock_test_and_set(once_control, 1) == 0)
		(*init_routine)();
	return 0;
}

int GetCurrentThreadId(void) {
	return sceKernelGetThreadId();
}

extern void *__aeabi_ldiv0;

int GetEnv(void *vm, void **env, int r2) {
	*env = fake_env;
	return 0;
}

void QDT__KRN__I_OUTPUT__Push(char *msg) {
	printf("%s\n", msg);
}

int rrSemaphoreCreate(int *uid, int value) {
	*uid = sceKernelCreateSema("sema", 0, value, 0x7fffffff, NULL);
	if (*uid < 0)
		return 0;
	return 1;
}

void rrSemaphoreDestroy(int *uid) {
	sceKernelDeleteSema(*uid);
}

int rrSemaphoreDecrementOrWait(int *uid, int ms) {
	if (ms == -1) {
		sceKernelWaitSema(*uid, 1, NULL);
	} else {
		SceUInt timeout = ms * 1000;
		if (sceKernelWaitSema(*uid, 1, &timeout) < 0)
			return 0;
	}
	return 1;
}

void rrSemaphoreIncrement(int *uid, int value) {
	sceKernelSignalSema(*uid, value);
}

int rrMutexCreate(SceKernelLwMutexWork **work, int unk) {
	*work = (SceKernelLwMutexWork *)memalign(8, sizeof(SceKernelLwMutexWork));
	if (sceKernelCreateLwMutex(*work, "mutex", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL) < 0)
		return 0;
	return 1;
}

void rrMutexDestroy(SceKernelLwMutexWork **work) {
	sceKernelDeleteLwMutex(*work);
	free(*work);
}

void rrMutexLock(SceKernelLwMutexWork **work) {
	sceKernelLockLwMutex(*work, 1, NULL);
}

int rrMutexLockTimeout(SceKernelLwMutexWork **work, int ms) {
	if (ms == -1) {
		sceKernelLockLwMutex(*work, 1, NULL);
	} else if (ms == 0) {
		if (sceKernelTryLockLwMutex(*work, 1) < 0)
			return 0;
	} else {
		SceUInt timeout = ms * 1000;
		if (sceKernelLockLwMutex(*work, 1, &timeout) < 0)
			return 0;
	}
	return 1;
}

void rrMutexUnlock(SceKernelLwMutexWork **work) {
	sceKernelUnlockLwMutex(*work, 1);
}

so_hook display2d_hook;
void Display2D(void *this) {
	glDisable(GL_SCISSOR_TEST);
	SO_CONTINUE(int, display2d_hook, this);
}

void patch_game(void) {
	if (ps2_mode)
		hook_addr(so_symbol(&fahrenheit_mod, "ktxLoadTextureM"), ret0);

	hook_addr(so_symbol(&fahrenheit_mod, "_ZN3QDT3KRN8I_OUTPUT4PushEPKcb"), QDT__KRN__I_OUTPUT__Push);

	hook_addr(so_symbol(&fahrenheit_mod, "rrmemset16"), sceClibMemset);
	hook_addr(so_symbol(&fahrenheit_mod, "rrmemset32"), sceClibMemset);

	hook_addr(so_symbol(&fahrenheit_mod, "rrSemaphoreCreate"), rrSemaphoreCreate);
	hook_addr(so_symbol(&fahrenheit_mod, "rrSemaphoreDestroy"), rrSemaphoreDestroy);
	hook_addr(so_symbol(&fahrenheit_mod, "rrSemaphoreDecrementOrWait"), rrSemaphoreDecrementOrWait);
	hook_addr(so_symbol(&fahrenheit_mod, "rrSemaphoreIncrement"), rrSemaphoreIncrement);

	hook_addr(so_symbol(&fahrenheit_mod, "rrMutexCreate"), rrMutexCreate);
	hook_addr(so_symbol(&fahrenheit_mod, "rrMutexDestroy"), rrMutexDestroy);
	hook_addr(so_symbol(&fahrenheit_mod, "rrMutexLock"), rrMutexLock);
	hook_addr(so_symbol(&fahrenheit_mod, "rrMutexLockTimeout"), rrMutexLockTimeout);
	hook_addr(so_symbol(&fahrenheit_mod, "rrMutexUnlock"), rrMutexUnlock);
	
	display2d_hook = hook_addr(so_symbol(&fahrenheit_mod, "_ZN3QDT3M3D15DISPLAY_MANAGER9Display2DEv"), Display2D);
}

extern void *__aeabi_atexit;
extern void *__aeabi_idiv;
extern void *__aeabi_idivmod;
extern void *__aeabi_ldivmod;
extern void *__aeabi_uidiv;
extern void *__aeabi_uidivmod;
extern void *__aeabi_uldivmod;
extern void *__cxa_atexit;
extern void *__cxa_finalize;
extern void *__cxa_call_unexpected;
extern void *__gnu_unwind_frame;
extern void *__stack_chk_fail;
int open(const char *pathname, int flags);

static int __stack_chk_guard_fake = 0x42424242;

static FILE __sF_fake[0x1000][3];

int stat_hook(const char *pathname, void *statbuf) {
	//printf("stat(%s)\n", pathname);
	struct stat st;
	int res;
	if (!strstr(pathname, "ux0:")) {
		char real_fname[256];
		sprintf(real_fname, "ux0:data/fahrenheit/%s", pathname);
		res = stat(real_fname, &st);
	} else
		res = stat(pathname, &st);
	if (res == 0)
		*(uint64_t *)(statbuf + 0x30) = st.st_size;
	return res;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
	return vglMemalign(length, 0x1000);
}

int munmap(void *addr, size_t length) {
	vglFree(addr);
	return 0;
}

int fstat_hook(int fd, void *statbuf) {
	struct stat st;
	int res = fstat(fd, &st);
	if (res == 0)
		*(uint64_t *)(statbuf + 0x30) = st.st_size;
	return res;
}

FILE *fopen_hook(char *fname, char *mode) {
	char real_fname[256];
	//printf("opening %s with mode %s (len: %d)\n", fname, mode, strlen(fname));
	char *s = strstr(fname, ".ktx");
	if (s) {
		fname[s - fname] = 0;
		sprintf(real_fname, "ux0:data/fahrenheit/%s.dxt", fname);
		fname[s - fname] = '.';
		return sceLibcBridge_fopen(real_fname, mode);
	}
	if (!strstr(fname, "ux0:")) {
		sprintf(real_fname, "ux0:data/fahrenheit/%s", fname);
		return sceLibcBridge_fopen(real_fname, mode);
	}
	return sceLibcBridge_fopen(fname, mode);
}

int mkdir_hook(const char *pathname, mode_t mode) {
	//printf("mkdir(%s)\n", pathname);
	if (!strstr(pathname, "ux0:")) {
		char real_fname[256];
		sprintf(real_fname, "ux0:data/fahrenheit/%s", pathname);
		return mkdir(real_fname, mode);
	}
	return mkdir(pathname, mode);
}

extern void *__cxa_guard_acquire;
extern void *__cxa_guard_release;

char *basename(char *path) {
	char *p = path;
	if (strlen(path) == 1)
		return path;
	char *slash = strstr(p, "/");
	while (slash) {
		p = slash + 1;
		slash = strstr(p, "/");
	}
	return p;
}

void *sceClibMemclr(void *dst, SceSize len) {
	return sceClibMemset(dst, 0, len);
}

void *sceClibMemset2(void *dst, SceSize len, int ch) {
	return sceClibMemset(dst, ch, len);
}

void *dlsym_hook( void *handle, const char *symbol);

void *Android_JNI_GetEnv() {
	return fake_env;
}

char *SDL_AndroidGetExternalStoragePath() {
	return DATA_PATH;
}

char *SDL_AndroidGetInternalStoragePath() {
	return DATA_PATH;
}

int g_SDL_BufferGeometry_w;
int g_SDL_BufferGeometry_h;

void abort_hook() {
	//printf("ABORT CALLED!!!\n");
	uint8_t *p = NULL;
	p[0] = 1;
}

int ret99() {
	return 99;
}

int chdir_hook(const char *path) {
	return 0;
}

int access_hook(const char *pathname, int mode) {
	//printf("access %s\n", pathname);
	int r;
	if (!strstr(pathname, "ux0:")) {
		char real_fname[256];
		sprintf(real_fname, "ux0:data/fahrenheit/%s", pathname);
		r = !file_exists(real_fname);
	} else
		r = !file_exists(pathname);
	return r ? -1 : 0;
}

void glShaderSource_fake(GLuint shader, GLsizei count, const GLchar **string, const GLint *length) {
	//printf("Shader with count %d\n", count);
	
	uint32_t sha1[5];
	SHA1_CTX ctx;
	
	sha1_init(&ctx);
	if (count > 1) {
		sha1_update(&ctx, string[1], length ? length[1] : strlen(string[1]));
	} else {
		sha1_update(&ctx, string[0], length ? length[0] : strlen(string[0]));
	}
	sha1_final(&ctx, (uint8_t *)sha1);
	
	char sha_name[64];
	snprintf(sha_name, sizeof(sha_name), "%08x%08x%08x%08x%08x", sha1[0], sha1[1], sha1[2], sha1[3], sha1[4]);
	char gxp_path[128], glsl_path[128];
	snprintf(gxp_path, sizeof(gxp_path), "%s/%s.gxp", "ux0:data/fahrenheit/gxp", sha_name);

	FILE *file = sceLibcBridge_fopen(gxp_path, "rb");
	if (!file) {
		char *tmp = string[1];
		/*snprintf(glsl_path, sizeof(glsl_path), "%s/%s.glsl", "ux0:data/fahrenheit/glsl", sha_name);
		file = sceLibcBridge_fopen(glsl_path, "w");
		sceLibcBridge_fwrite(tmp, 1, strlen(tmp), file);
		sceLibcBridge_fclose(file);*/
		
		//printf("Auto translation attempt...\n");
		char *tmp2 = vglMalloc(1 * 1024 * 1024);
		//if (!tmp2)
		//	printf("OUT OF MEM (2)!!!!\n");
		char *s = strstr(tmp, "#if defined GL_ES");
		if (!s) printf("wtf\n");
		size_t shaderSize;
		if (strstr(tmp, "u_modelViewProjectionMatrix")) { // Vertex shader
			//printf("Vertex shader detected\n");
			sceClibMemcpy(tmp2, tmp, s - tmp);
			char *p = tmp2 + (s - tmp);
			sprintf(glsl_path, "ux0:data/fahrenheit/vert.cg");
			file = sceLibcBridge_fopen(glsl_path, "r");
			sceLibcBridge_fseek(file, 0, SEEK_END);
			shaderSize = sceLibcBridge_ftell(file);
			sceLibcBridge_fseek(file, 0, SEEK_SET);
			sceLibcBridge_fread(p, 1, shaderSize, file);
			sceLibcBridge_fclose(file);
			p[shaderSize] = 0;
		} else { // Fragment Shader
			//printf("Fragment shader detected\n");
			sceClibMemcpy(tmp2, tmp, s - tmp);
			char *p = tmp2 + (s - tmp);
			sprintf(glsl_path, "ux0:data/fahrenheit/frag.cg");
			file = sceLibcBridge_fopen(glsl_path, "r");
			sceLibcBridge_fseek(file, 0, SEEK_END);
			shaderSize = sceLibcBridge_ftell(file);
			sceLibcBridge_fseek(file, 0, SEEK_SET);
			sceLibcBridge_fread(p, 1, shaderSize, file);
			sceLibcBridge_fclose(file);
			p[shaderSize] = 0;
		}
		/*
		sprintf(glsl_path, "ux0:data/fahrenheit/glsl/%s.cg", sha_name);
		file = fopen(glsl_path, "w+");
		fwrite(tmp2, 1, strlen(tmp2), file);
		fclose(file);
		*/
		//printf("Compiling resulting shader\n");
		glShaderSource(shader, 1, &tmp2, NULL);
		glCompileShader(shader);
		vglGetShaderBinary(shader, 0x8000, &shaderSize, tmp2);
		file = sceLibcBridge_fopen(gxp_path, "w+");
		sceLibcBridge_fwrite(tmp2, 1, shaderSize, file);
		sceLibcBridge_fclose(file);
		vglFree(tmp2);
		//printf("Auto translation completed!\n");
	} else {
		size_t shaderSize;
		void *shaderBuf;

		sceLibcBridge_fseek(file, 0, SEEK_END);
		shaderSize = sceLibcBridge_ftell(file);
		sceLibcBridge_fseek(file, 0, SEEK_SET);

		shaderBuf = vglMalloc(shaderSize);
		sceLibcBridge_fread(shaderBuf, 1, shaderSize, file);
		sceLibcBridge_fclose(file);

		glShaderBinary(1, &shader, 0, shaderBuf, shaderSize);

		vglFree(shaderBuf);
	}
}

void glDeleteProgram_fake(GLuint prog) {
	GLuint shaders[2];
	GLsizei count;
	glGetAttachedShaders(prog, 2, &count, shaders);
	for (int i = 0; i < count; i++) {
		glDeleteShader(shaders[i]);
	}
	glDeleteProgram(prog);
}

void glScissor_fake(GLint x, GLint y, GLsizei width, GLsizei height) {
	if (height == 544)
		return;
	glScissor(x, y, width, height);
}

void SDL_GL_SwapWindow_fake(SDL_Window * window) {
	SDL_GL_SwapWindow(window);
	glScissor(0, 0, SCREEN_W, SCREEN_H);
}

void glDisable_fake(GLenum cap) {
	if (cap == GL_SCISSOR_TEST)
		return;
	glDisable(cap);
}

static so_default_dynlib gl_hook[] = {
	{"glShaderSource", (uintptr_t)&glShaderSource_fake},
	{"glCompileShader", (uintptr_t)&ret0},
	{"glPixelStorei", (uintptr_t)&ret0},
	{"glBlendColor", (uintptr_t)&ret0},
	{"glDeleteShader", (uintptr_t)&ret0},
	{"glDeleteProgram", (uintptr_t)&glDeleteProgram_fake},
	{"glReleaseShaderCompiler", (uintptr_t)&ret0},
	{"glScissor", (uintptr_t)&glScissor_fake},
	{"glDisable", (uintptr_t)&glDisable_fake},
};
static size_t gl_numhook = sizeof(gl_hook) / sizeof(*gl_hook);

void *SDL_GL_GetProcAddress_fake(const char *symbol) {
	printf("looking for symbol %s\n", symbol);
	for (size_t i = 0; i < gl_numhook; ++i) {
		if (!strcmp(symbol, gl_hook[i].symbol)) {
			return (void *)gl_hook[i].func;
		}
	}
	void *r = vglGetProcAddress(symbol);
	if (!r)
		printf("Cannot find symbol %s\n", symbol);
	return r;
}

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {
	//printf("readlink(%s)\n", pathname);
	strncpy(buf, "ux0:data/fahrenheit/libFahrenheit.so", bufsiz - 1);
	return strlen(buf);
}

#define SCE_ERRNO_MASK 0xFF

#define DT_DIR 4
#define DT_REG 8

struct android_dirent {
	char pad[18];
	unsigned char d_type;
	char d_name[256];
};

typedef struct {
	SceUID uid;
	struct android_dirent dir;
} android_DIR;

int closedir_fake(android_DIR *dirp) {
	if (!dirp || dirp->uid < 0) {
		errno = EBADF;
		return -1;
	}

	int res = sceIoDclose(dirp->uid);
	dirp->uid = -1;

	free(dirp);

	if (res < 0) {
		errno = res & SCE_ERRNO_MASK;
		return -1;
	}

	errno = 0;
	return 0;
}

android_DIR *opendir_fake(const char *dirname) {
	//printf("opendir(%s)\n", dirname);
	SceUID uid;
	if (!strstr(dirname, "ux0:")) {
		char real_fname[256];
		sprintf(real_fname, "ux0:data/fahrenheit/%s", dirname);
		uid = sceIoDopen(real_fname);
	} else
		uid = sceIoDopen(dirname);

	if (uid < 0) {
		errno = uid & SCE_ERRNO_MASK;
		return NULL;
	}

	android_DIR *dirp = calloc(1, sizeof(android_DIR));

	if (!dirp) {
		sceIoDclose(uid);
		errno = ENOMEM;
		return NULL;
	}

	dirp->uid = uid;

	errno = 0;
	return dirp;
}

struct android_dirent *readdir_fake(android_DIR *dirp) {
	if (!dirp) {
		errno = EBADF;
		return NULL;
	}

	SceIoDirent sce_dir;
	int res = sceIoDread(dirp->uid, &sce_dir);

	if (res < 0) {
		errno = res & SCE_ERRNO_MASK;
		return NULL;
	}

	if (res == 0) {
		errno = 0;
		return NULL;
	}

	dirp->dir.d_type = SCE_S_ISDIR(sce_dir.d_stat.st_mode) ? DT_DIR : DT_REG;
	strcpy(dirp->dir.d_name, sce_dir.d_name);
	return &dirp->dir;
}

off_t sceLibcBridge_ftello(FILE *stream) {
	return (off_t)sceLibcBridge_ftell(stream);
}

static so_default_dynlib default_dynlib[] = {
	{ "opendir", (uintptr_t)&opendir_fake },
	{ "readdir", (uintptr_t)&readdir_fake },
	{ "closedir", (uintptr_t)&closedir_fake },
	{ "readlink", (uintptr_t)&readlink },
	{ "g_SDL_BufferGeometry_w", (uintptr_t)&g_SDL_BufferGeometry_w },
	{ "g_SDL_BufferGeometry_h", (uintptr_t)&g_SDL_BufferGeometry_h },
	{ "SL_IID_BUFFERQUEUE", (uintptr_t)&SL_IID_BUFFERQUEUE },
	{ "SL_IID_ENGINE", (uintptr_t)&SL_IID_ENGINE },
	{ "SL_IID_ENVIRONMENTALREVERB", (uintptr_t)&SL_IID_ENVIRONMENTALREVERB },
	{ "SL_IID_PLAY", (uintptr_t)&SL_IID_PLAY },
	{ "SL_IID_PLAYBACKRATE", (uintptr_t)&SL_IID_PLAYBACKRATE },
	{ "SL_IID_SEEK", (uintptr_t)&SL_IID_SEEK },
	{ "SL_IID_VOLUME", (uintptr_t)&SL_IID_VOLUME },
	{ "__aeabi_memclr", (uintptr_t)&sceClibMemclr },
	{ "__aeabi_memclr4", (uintptr_t)&sceClibMemclr },
	{ "__aeabi_memclr8", (uintptr_t)&sceClibMemclr },
	{ "__aeabi_memcpy4", (uintptr_t)&sceClibMemcpy },
	{ "__aeabi_memcpy8", (uintptr_t)&sceClibMemcpy },
	{ "__aeabi_memmove4", (uintptr_t)&sceClibMemmove },
	{ "__aeabi_memmove8", (uintptr_t)&sceClibMemmove },
	{ "__aeabi_memcpy", (uintptr_t)&sceClibMemcpy },
	{ "__aeabi_memmove", (uintptr_t)&sceClibMemmove },
	{ "__aeabi_memset", (uintptr_t)&sceClibMemset2 },
	{ "__aeabi_memset4", (uintptr_t)&sceClibMemset2 },
	{ "__aeabi_memset8", (uintptr_t)&sceClibMemset2 },
	{ "__aeabi_atexit", (uintptr_t)&__aeabi_atexit },
	{ "__android_log_print", (uintptr_t)&__android_log_print },
	{ "__android_log_vprint", (uintptr_t)&__android_log_vprint },
	{ "__android_log_write", (uintptr_t)&__android_log_write },
	{ "__cxa_atexit", (uintptr_t)&__cxa_atexit },
	{ "__cxa_call_unexpected", (uintptr_t)&__cxa_call_unexpected },
	{ "__cxa_guard_acquire", (uintptr_t)&__cxa_guard_acquire },
	{ "__cxa_guard_release", (uintptr_t)&__cxa_guard_release },
	{ "__cxa_finalize", (uintptr_t)&__cxa_finalize },
	{ "__errno", (uintptr_t)&__errno },
	{ "__gnu_unwind_frame", (uintptr_t)&__gnu_unwind_frame },
	{ "__gnu_Unwind_Find_exidx", (uintptr_t)&ret0 },
	{ "dl_unwind_find_exidx", (uintptr_t)&ret0 },
	// { "__google_potentially_blocking_region_begin", (uintptr_t)&__google_potentially_blocking_region_begin },
	// { "__google_potentially_blocking_region_end", (uintptr_t)&__google_potentially_blocking_region_end },
	{ "__sF", (uintptr_t)&__sF_fake },
	{ "__stack_chk_fail", (uintptr_t)&__stack_chk_fail },
	{ "__stack_chk_guard", (uintptr_t)&__stack_chk_guard_fake },
	{ "_ctype_", (uintptr_t)&BIONIC_ctype_},
	{ "_tolower_tab_", (uintptr_t)&BIONIC_tolower_tab_},
	{ "_toupper_tab_", (uintptr_t)&BIONIC_toupper_tab_},
	{ "abort", (uintptr_t)&abort_hook },
	{ "access", (uintptr_t)&access_hook },
	{ "acos", (uintptr_t)&acos },
	{ "acosh", (uintptr_t)&acosh },
	{ "acosf", (uintptr_t)&acosf },
	{ "asin", (uintptr_t)&asin },
	{ "asinh", (uintptr_t)&asinh },
	{ "asinf", (uintptr_t)&asinf },
	{ "atan", (uintptr_t)&atan },
	{ "atanh", (uintptr_t)&atanh },
	{ "atan2", (uintptr_t)&atan2 },
	{ "atan2f", (uintptr_t)&atan2f },
	{ "atanf", (uintptr_t)&atanf },
	{ "atoi", (uintptr_t)&atoi },
	{ "atol", (uintptr_t)&atol },
	{ "atoll", (uintptr_t)&atoll },
	{ "basename", (uintptr_t)&basename },
	// { "bind", (uintptr_t)&bind },
	{ "bsearch", (uintptr_t)&bsearch },
	{ "btowc", (uintptr_t)&btowc },
	{ "calloc", (uintptr_t)&vglCalloc },
	{ "ceil", (uintptr_t)&ceil },
	{ "ceilf", (uintptr_t)&ceilf },
	{ "chdir", (uintptr_t)&chdir_hook },
	{ "clearerr", (uintptr_t)&clearerr },
	{ "clock_gettime", (uintptr_t)&clock_gettime_hook },
	{ "close", (uintptr_t)&close },
	{ "cos", (uintptr_t)&cos },
	{ "cosf", (uintptr_t)&cosf },
	{ "cosh", (uintptr_t)&cosh },
	{ "crc32", (uintptr_t)&crc32 },
	{ "deflate", (uintptr_t)&deflate },
	{ "deflateEnd", (uintptr_t)&deflateEnd },
	{ "deflateInit_", (uintptr_t)&deflateInit_ },
	{ "deflateInit2_", (uintptr_t)&deflateInit2_ },
	{ "deflateReset", (uintptr_t)&deflateReset },
	{ "dlopen", (uintptr_t)&ret0 },
	{ "dlsym", (uintptr_t)&dlsym_hook },
	{ "exit", (uintptr_t)&exit },
	{ "exp", (uintptr_t)&exp },
	{ "exp2", (uintptr_t)&exp2 },
	{ "expf", (uintptr_t)&expf },
	{ "fabsf", (uintptr_t)&fabsf },
	{ "fclose", (uintptr_t)&sceLibcBridge_fclose },
	{ "fcntl", (uintptr_t)&ret0 },
	// { "fdopen", (uintptr_t)&fdopen },
	// { "ferror", (uintptr_t)&ferror },
	// { "fflush", (uintptr_t)&fflush },
	// { "fgets", (uintptr_t)&fgets },
	{ "floor", (uintptr_t)&floor },
	{ "floorf", (uintptr_t)&floorf },
	{ "fmod", (uintptr_t)&fmod },
	{ "fmodf", (uintptr_t)&fmodf },
	{ "fnmatch", (uintptr_t)&fnmatch },
	{ "fopen", (uintptr_t)&fopen_hook },
	{ "fprintf", (uintptr_t)&sceLibcBridge_fprintf },
	// { "fputc", (uintptr_t)&fputc },
	// { "fputwc", (uintptr_t)&fputwc },
	// { "fputs", (uintptr_t)&fputs },
	{ "fread", (uintptr_t)&sceLibcBridge_fread },
	{ "free", (uintptr_t)&vglFree },
	{ "frexp", (uintptr_t)&frexp },
	{ "frexpf", (uintptr_t)&frexpf },
	// { "fscanf", (uintptr_t)&fscanf },
	{ "fseek", (uintptr_t)&sceLibcBridge_fseek },
	// { "fstat", (uintptr_t)&fstat_hook },
	{ "ftell", (uintptr_t)&sceLibcBridge_ftell },
	{ "ftello", (uintptr_t)&sceLibcBridge_ftello },
	// { "ftruncate", (uintptr_t)&ftruncate },
	{ "fwrite", (uintptr_t)&sceLibcBridge_fwrite },
	{ "getc", (uintptr_t)&getc },
	{ "getpid", (uintptr_t)&ret0 },
	{ "getcwd", (uintptr_t)&getcwd_hook },
	{ "getenv", (uintptr_t)&ret0 },
	{ "getwc", (uintptr_t)&getwc },
	{ "gettimeofday", (uintptr_t)&gettimeofday },
	{ "gzopen", (uintptr_t)&gzopen },
	{ "inflate", (uintptr_t)&inflate },
	{ "inflateEnd", (uintptr_t)&inflateEnd },
	{ "inflateInit_", (uintptr_t)&inflateInit_ },
	{ "inflateReset", (uintptr_t)&inflateReset },
	{ "isalnum", (uintptr_t)&isalnum },
	{ "isalpha", (uintptr_t)&isalpha },
	{ "iscntrl", (uintptr_t)&iscntrl },
	{ "islower", (uintptr_t)&islower },
	{ "ispunct", (uintptr_t)&ispunct },
	{ "isprint", (uintptr_t)&isprint },
	{ "isspace", (uintptr_t)&isspace },
	{ "isupper", (uintptr_t)&isupper },
	{ "iswalpha", (uintptr_t)&iswalpha },
	{ "iswcntrl", (uintptr_t)&iswcntrl },
	{ "iswctype", (uintptr_t)&iswctype },
	{ "iswdigit", (uintptr_t)&iswdigit },
	{ "iswdigit", (uintptr_t)&iswdigit },
	{ "iswlower", (uintptr_t)&iswlower },
	{ "iswprint", (uintptr_t)&iswprint },
	{ "iswpunct", (uintptr_t)&iswpunct },
	{ "iswspace", (uintptr_t)&iswspace },
	{ "iswupper", (uintptr_t)&iswupper },
	{ "iswxdigit", (uintptr_t)&iswxdigit },
	{ "isxdigit", (uintptr_t)&isxdigit },
	{ "ldexp", (uintptr_t)&ldexp },
	{ "ldexpf", (uintptr_t)&ldexpf },
	// { "listen", (uintptr_t)&listen },
	{ "localtime_r", (uintptr_t)&localtime_r },
	{ "log", (uintptr_t)&log },
	{ "logf", (uintptr_t)&logf },
	{ "log10", (uintptr_t)&log10 },
	{ "log10f", (uintptr_t)&log10f },
	{ "longjmp", (uintptr_t)&longjmp },
	{ "lrand48", (uintptr_t)&lrand48 },
	{ "lrint", (uintptr_t)&lrint },
	{ "lrintf", (uintptr_t)&lrintf },
	{ "lseek", (uintptr_t)&lseek },
	{ "malloc", (uintptr_t)&vglMalloc },
	{ "mbrtowc", (uintptr_t)&mbrtowc },
	{ "memalign", (uintptr_t)&vglMemalign },
	{ "memchr", (uintptr_t)&sceClibMemchr },
	{ "memcmp", (uintptr_t)&sceClibMemcmp },
	{ "memcpy", (uintptr_t)&sceClibMemcpy },
	{ "memmove", (uintptr_t)&sceClibMemmove },
	{ "memset", (uintptr_t)&sceClibMemset },
	{ "mkdir", (uintptr_t)&mkdir_hook },
	// { "mmap", (uintptr_t)&mmap},
	// { "munmap", (uintptr_t)&munmap},
	{ "modf", (uintptr_t)&modf },
	{ "modff", (uintptr_t)&modff },
	// { "poll", (uintptr_t)&poll },
	{ "open", (uintptr_t)&open },
	{ "pow", (uintptr_t)&pow },
	{ "powf", (uintptr_t)&powf },
	{ "printf", (uintptr_t)&printf },
	{ "pthread_attr_destroy", (uintptr_t)&ret0 },
	{ "pthread_attr_init", (uintptr_t)&ret0 },
	{ "pthread_attr_setdetachstate", (uintptr_t)&ret0 },
	{ "pthread_attr_setstacksize", (uintptr_t)&ret0 },
	{ "pthread_cond_init", (uintptr_t)&pthread_cond_init_fake},
	{ "pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_fake},
	{ "pthread_cond_wait", (uintptr_t)&pthread_cond_wait_fake},
	{ "pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy_fake},
	{ "pthread_cond_timedwait", (uintptr_t)&pthread_cond_timedwait_fake},
	{ "pthread_cond_timedwait_relative_np", (uintptr_t)&pthread_cond_timedwait_relative_np_fake}, // FIXME
	{ "pthread_create", (uintptr_t)&pthread_create_fake },
	{ "pthread_getschedparam", (uintptr_t)&pthread_getschedparam },
	{ "pthread_getspecific", (uintptr_t)&pthread_getspecific },
	{ "pthread_key_create", (uintptr_t)&pthread_key_create },
	{ "pthread_key_delete", (uintptr_t)&pthread_key_delete },
	{ "pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_fake },
	{ "pthread_mutex_init", (uintptr_t)&pthread_mutex_init_fake },
	{ "pthread_mutex_trylock", (uintptr_t)&pthread_mutex_trylock_fake },
	{ "pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_fake },
	{ "pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_fake },
	{ "pthread_mutexattr_destroy", (uintptr_t)&pthread_mutexattr_destroy},
	{ "pthread_mutexattr_init", (uintptr_t)&pthread_mutexattr_init},
	{ "pthread_mutexattr_settype", (uintptr_t)&pthread_mutexattr_settype},
	{ "pthread_once", (uintptr_t)&pthread_once_fake },
	{ "pthread_self", (uintptr_t)&pthread_self },
	{ "pthread_setname_np", (uintptr_t)&ret0 },
	{ "pthread_getschedparam", (uintptr_t)&pthread_getschedparam },
	{ "pthread_setschedparam", (uintptr_t)&pthread_setschedparam },
	{ "pthread_setspecific", (uintptr_t)&pthread_setspecific },
	{ "sched_get_priority_min", (uintptr_t)&ret0 },
	{ "sched_get_priority_max", (uintptr_t)&ret99 },
	{ "putc", (uintptr_t)&putc },
	{ "puts", (uintptr_t)&puts },
	{ "putwc", (uintptr_t)&putwc },
	{ "qsort", (uintptr_t)&qsort },
	{ "read", (uintptr_t)&read },
	{ "realpath", (uintptr_t)&realpath },
	{ "realloc", (uintptr_t)&vglRealloc },
	// { "recv", (uintptr_t)&recv },
	{ "rint", (uintptr_t)&rint },
	// { "send", (uintptr_t)&send },
	// { "sendto", (uintptr_t)&sendto },
	{ "setenv", (uintptr_t)&ret0 },
	{ "setjmp", (uintptr_t)&setjmp },
	{ "setlocale", (uintptr_t)&ret0 },
	// { "setsockopt", (uintptr_t)&setsockopt },
	{ "setvbuf", (uintptr_t)&setvbuf },
	{ "sin", (uintptr_t)&sin },
	{ "sinf", (uintptr_t)&sinf },
	{ "sinh", (uintptr_t)&sinh },
	{ "slCreateEngine", (uintptr_t)&slCreateEngine },
	{ "snprintf", (uintptr_t)&snprintf },
	// { "socket", (uintptr_t)&socket },
	{ "sprintf", (uintptr_t)&sprintf },
	{ "sqrt", (uintptr_t)&sqrt },
	{ "sqrtf", (uintptr_t)&sqrtf },
	{ "srand48", (uintptr_t)&srand48 },
	{ "sscanf", (uintptr_t)&sscanf },
	{ "stat", (uintptr_t)&stat_hook },
	{ "strcasecmp", (uintptr_t)&strcasecmp },
	{ "strcasestr", (uintptr_t)&strstr },
	{ "strcat", (uintptr_t)&strcat },
	{ "strchr", (uintptr_t)&strchr },
	{ "strcmp", (uintptr_t)&sceClibStrcmp },
	{ "strcoll", (uintptr_t)&strcoll },
	{ "strcpy", (uintptr_t)&strcpy },
	{ "strcspn", (uintptr_t)&strcspn },
	{ "strdup", (uintptr_t)&strdup },
	{ "strerror", (uintptr_t)&strerror },
	{ "strftime", (uintptr_t)&strftime },
	{ "strlcpy", (uintptr_t)&strlcpy },
	{ "strlen", (uintptr_t)&strlen },
	{ "strncasecmp", (uintptr_t)&sceClibStrncasecmp },
	{ "strncat", (uintptr_t)&sceClibStrncat },
	{ "strncmp", (uintptr_t)&sceClibStrncmp },
	{ "strncpy", (uintptr_t)&sceClibStrncpy },
	{ "strpbrk", (uintptr_t)&strpbrk },
	{ "strrchr", (uintptr_t)&sceClibStrrchr },
	{ "strstr", (uintptr_t)&sceClibStrstr },
	{ "strtod", (uintptr_t)&strtod },
	{ "strtol", (uintptr_t)&strtol },
	{ "strtoul", (uintptr_t)&strtoul },
	{ "strtoll", (uintptr_t)&strtoll },
	{ "strtoull", (uintptr_t)&strtoull },
	{ "strxfrm", (uintptr_t)&strxfrm },
	{ "sysconf", (uintptr_t)&ret0 },
	{ "tan", (uintptr_t)&tan },
	{ "tanf", (uintptr_t)&tanf },
	{ "tanh", (uintptr_t)&tanh },
	{ "time", (uintptr_t)&time },
	{ "tolower", (uintptr_t)&tolower },
	{ "toupper", (uintptr_t)&toupper },
	{ "towlower", (uintptr_t)&towlower },
	{ "towupper", (uintptr_t)&towupper },
	{ "ungetc", (uintptr_t)&ungetc },
	{ "ungetwc", (uintptr_t)&ungetwc },
	{ "usleep", (uintptr_t)&usleep },
	{ "vfprintf", (uintptr_t)&vfprintf },
	{ "vprintf", (uintptr_t)&vprintf },
	{ "vsnprintf", (uintptr_t)&vsnprintf },
	{ "vsprintf", (uintptr_t)&vsprintf },
	{ "vswprintf", (uintptr_t)&vswprintf },
	{ "wcrtomb", (uintptr_t)&wcrtomb },
	{ "wcscoll", (uintptr_t)&wcscoll },
	{ "wcscmp", (uintptr_t)&wcscmp },
	{ "wcsncpy", (uintptr_t)&wcsncpy },
	{ "wcsftime", (uintptr_t)&wcsftime },
	{ "wcslen", (uintptr_t)&wcslen },
	{ "wcsxfrm", (uintptr_t)&wcsxfrm },
	{ "wctob", (uintptr_t)&wctob },
	{ "wctype", (uintptr_t)&wctype },
	{ "wmemchr", (uintptr_t)&wmemchr },
	{ "wmemcmp", (uintptr_t)&wmemcmp },
	{ "wmemcpy", (uintptr_t)&wmemcpy },
	{ "wmemmove", (uintptr_t)&wmemmove },
	{ "wmemset", (uintptr_t)&wmemset },
	{ "write", (uintptr_t)&write },
	// { "writev", (uintptr_t)&writev },
	{ "SDL_AndroidGetExternalStoragePath", (uintptr_t)&SDL_AndroidGetExternalStoragePath },
	{ "SDL_AndroidGetInternalStoragePath", (uintptr_t)&SDL_AndroidGetInternalStoragePath },
	{ "SDL_Android_Init", (uintptr_t)&ret1 },
	{ "SDL_AddTimer", (uintptr_t)&SDL_AddTimer },
	{ "SDL_CondSignal", (uintptr_t)&SDL_CondSignal },
	{ "SDL_CondWait", (uintptr_t)&SDL_CondWait },
	{ "SDL_ConvertSurfaceFormat", (uintptr_t)&SDL_ConvertSurfaceFormat },
	{ "SDL_CreateCond", (uintptr_t)&SDL_CreateCond },
	{ "SDL_CreateMutex", (uintptr_t)&SDL_CreateMutex },
	{ "SDL_CreateRenderer", (uintptr_t)&SDL_CreateRenderer },
	{ "SDL_CreateRGBSurface", (uintptr_t)&SDL_CreateRGBSurface },
	{ "SDL_CreateTexture", (uintptr_t)&SDL_CreateTexture },
	{ "SDL_CreateTextureFromSurface", (uintptr_t)&SDL_CreateTextureFromSurface },
	{ "SDL_CreateThread", (uintptr_t)&SDL_CreateThread },
	{ "SDL_CreateWindow", (uintptr_t)&SDL_CreateWindow },
	{ "SDL_Delay", (uintptr_t)&SDL_Delay },
	{ "SDL_DestroyMutex", (uintptr_t)&SDL_DestroyMutex },
	{ "SDL_DestroyRenderer", (uintptr_t)&SDL_DestroyRenderer },
	{ "SDL_DestroyTexture", (uintptr_t)&SDL_DestroyTexture },
	{ "SDL_DestroyWindow", (uintptr_t)&SDL_DestroyWindow },
	{ "SDL_FillRect", (uintptr_t)&SDL_FillRect },
	{ "SDL_FreeSurface", (uintptr_t)&SDL_FreeSurface },
	{ "SDL_GetCurrentDisplayMode", (uintptr_t)&SDL_GetCurrentDisplayMode },
	{ "SDL_GetDisplayMode", (uintptr_t)&SDL_GetDisplayMode },
	{ "SDL_GetError", (uintptr_t)&SDL_GetError },
	{ "SDL_GetModState", (uintptr_t)&SDL_GetModState },
	{ "SDL_GetMouseState", (uintptr_t)&SDL_GetMouseState },
	{ "SDL_GetRendererInfo", (uintptr_t)&SDL_GetRendererInfo },
	{ "SDL_GetTextureBlendMode", (uintptr_t)&SDL_GetTextureBlendMode },
	{ "SDL_GetTextureColorMod", (uintptr_t)&SDL_GetTextureColorMod },
	{ "SDL_GetTicks", (uintptr_t)&SDL_GetTicks },
	{ "SDL_GL_BindTexture", (uintptr_t)&SDL_GL_BindTexture },
	{ "SDL_GL_GetCurrentContext", (uintptr_t)&SDL_GL_GetCurrentContext },
	{ "SDL_GL_MakeCurrent", (uintptr_t)&SDL_GL_MakeCurrent },
	{ "SDL_GL_SetAttribute", (uintptr_t)&SDL_GL_SetAttribute },
	{ "SDL_Init", (uintptr_t)&SDL_Init },
	{ "SDL_InitSubSystem", (uintptr_t)&SDL_InitSubSystem },
	{ "SDL_IntersectRect", (uintptr_t)&SDL_IntersectRect },
	{ "SDL_LockMutex", (uintptr_t)&SDL_LockMutex },
	{ "SDL_LockSurface", (uintptr_t)&SDL_LockSurface },
	{ "SDL_Log", (uintptr_t)&SDL_Log },
	{ "SDL_LogError", (uintptr_t)&SDL_LogError },
	{ "SDL_LogSetPriority", (uintptr_t)&SDL_LogSetPriority },
	{ "SDL_MapRGB", (uintptr_t)&SDL_MapRGB },
	{ "SDL_MinimizeWindow", (uintptr_t)&SDL_MinimizeWindow },
	{ "SDL_PeepEvents", (uintptr_t)&SDL_PeepEvents },
	{ "SDL_PumpEvents", (uintptr_t)&SDL_PumpEvents },
	{ "SDL_PushEvent", (uintptr_t)&SDL_PushEvent },
	{ "SDL_PollEvent", (uintptr_t)&SDL_PollEvent },
	{ "SDL_QueryTexture", (uintptr_t)&SDL_QueryTexture },
	{ "SDL_Quit", (uintptr_t)&SDL_Quit },
	{ "SDL_RemoveTimer", (uintptr_t)&SDL_RemoveTimer },
	{ "SDL_RenderClear", (uintptr_t)&SDL_RenderClear },
	{ "SDL_RenderCopy", (uintptr_t)&SDL_RenderCopy },
	{ "SDL_RenderFillRect", (uintptr_t)&SDL_RenderFillRect },
	{ "SDL_RenderPresent", (uintptr_t)&SDL_RenderPresent },
	{ "SDL_RWFromFile", (uintptr_t)&SDL_RWFromFile },
	{ "SDL_RWFromMem", (uintptr_t)&SDL_RWFromMem },
	{ "SDL_SetColorKey", (uintptr_t)&SDL_SetColorKey },
	{ "SDL_SetEventFilter", (uintptr_t)&SDL_SetEventFilter },
	{ "SDL_SetHint", (uintptr_t)&SDL_SetHint },
	{ "SDL_SetMainReady_REAL", (uintptr_t)&SDL_SetMainReady },
	{ "SDL_SetRenderDrawBlendMode", (uintptr_t)&SDL_SetRenderDrawBlendMode },
	{ "SDL_SetRenderDrawColor", (uintptr_t)&SDL_SetRenderDrawColor },
	{ "SDL_SetRenderTarget", (uintptr_t)&SDL_SetRenderTarget },
	{ "SDL_SetTextureBlendMode", (uintptr_t)&SDL_SetTextureBlendMode },
	{ "SDL_SetTextureColorMod", (uintptr_t)&SDL_SetTextureColorMod },
	{ "SDL_ShowCursor", (uintptr_t)&SDL_ShowCursor },
	{ "SDL_ShowSimpleMessageBox", (uintptr_t)&SDL_ShowSimpleMessageBox },
	{ "SDL_StartTextInput", (uintptr_t)&SDL_StartTextInput },
	{ "SDL_StopTextInput", (uintptr_t)&SDL_StopTextInput },
	{ "SDL_strdup", (uintptr_t)&SDL_strdup },
	{ "SDL_UnlockMutex", (uintptr_t)&SDL_UnlockMutex },
	{ "SDL_UnlockSurface", (uintptr_t)&SDL_UnlockSurface },
	{ "SDL_UpdateTexture", (uintptr_t)&SDL_UpdateTexture },
	{ "SDL_UpperBlit", (uintptr_t)&SDL_UpperBlit },
	{ "SDL_WaitThread", (uintptr_t)&SDL_WaitThread },
	{ "SDL_GetKeyFromScancode", (uintptr_t)&SDL_GetKeyFromScancode },
	{ "SDL_GetNumVideoDisplays", (uintptr_t)&SDL_GetNumVideoDisplays },
	{ "SDL_GetDisplayBounds", (uintptr_t)&SDL_GetDisplayBounds },
	{ "SDL_UnionRect", (uintptr_t)&SDL_UnionRect },
	{ "SDL_GetKeyboardFocus", (uintptr_t)&SDL_GetKeyboardFocus },
	{ "SDL_GetRelativeMouseMode", (uintptr_t)&SDL_GetRelativeMouseMode },
	{ "SDL_NumJoysticks", (uintptr_t)&SDL_NumJoysticks },
	{ "SDL_GameControllerOpen", (uintptr_t)&SDL_GameControllerOpen },
	{ "SDL_GameControllerGetJoystick", (uintptr_t)&SDL_GameControllerGetJoystick },
	{ "SDL_HapticOpenFromJoystick", (uintptr_t)&SDL_HapticOpenFromJoystick },
	{ "SDL_GetPerformanceFrequency", (uintptr_t)&SDL_GetPerformanceFrequency },
	{ "SDL_GetPerformanceCounter", (uintptr_t)&SDL_GetPerformanceCounter },
	{ "SDL_GetMouseFocus", (uintptr_t)&SDL_GetMouseFocus },
	{ "SDL_ShowMessageBox", (uintptr_t)&SDL_ShowMessageBox },
	{ "SDL_RaiseWindow", (uintptr_t)&SDL_RaiseWindow },
	{ "SDL_GL_GetAttribute", (uintptr_t)&SDL_GL_GetAttribute },
	{ "SDL_GL_CreateContext", (uintptr_t)&SDL_GL_CreateContext },
	{ "SDL_GL_GetProcAddress", (uintptr_t)&SDL_GL_GetProcAddress_fake },
	{ "SDL_GL_DeleteContext", (uintptr_t)&SDL_GL_DeleteContext },
	{ "SDL_GetDesktopDisplayMode", (uintptr_t)&SDL_GetDesktopDisplayMode },
	{ "SDL_SetWindowData", (uintptr_t)&SDL_SetWindowData },
	{ "SDL_GetWindowFlags", (uintptr_t)&SDL_GetWindowFlags },
	{ "SDL_GetWindowSize", (uintptr_t)&SDL_GetWindowSize },
	{ "SDL_GetWindowDisplayIndex", (uintptr_t)&SDL_GetWindowDisplayIndex },
	{ "SDL_SetWindowFullscreen", (uintptr_t)&SDL_SetWindowFullscreen },
	{ "SDL_SetWindowSize", (uintptr_t)&SDL_SetWindowSize },
	{ "SDL_SetWindowPosition", (uintptr_t)&SDL_SetWindowPosition },
	{ "SDL_GL_GetCurrentWindow", (uintptr_t)&SDL_GL_GetCurrentWindow },
	{ "SDL_GetWindowData", (uintptr_t)&SDL_GetWindowData },
	{ "SDL_GetWindowTitle", (uintptr_t)&SDL_GetWindowTitle },
	{ "SDL_SetWindowTitle", (uintptr_t)&SDL_SetWindowTitle },
	{ "SDL_GetWindowPosition", (uintptr_t)&SDL_GetWindowPosition },
	{ "SDL_GL_SetSwapInterval", (uintptr_t)&SDL_GL_SetSwapInterval },
	{ "SDL_IsGameController", (uintptr_t)&SDL_IsGameController },
	{ "SDL_JoystickGetDeviceGUID", (uintptr_t)&SDL_JoystickGetDeviceGUID },
	{ "SDL_GameControllerNameForIndex", (uintptr_t)&SDL_GameControllerNameForIndex },
	{ "SDL_GetWindowFromID", (uintptr_t)&SDL_GetWindowFromID },
	{ "SDL_GL_SwapWindow", (uintptr_t)&SDL_GL_SwapWindow_fake },
	{ "SDL_SetMainReady", (uintptr_t)&SDL_SetMainReady },
	{ "SDL_NumAccelerometers", (uintptr_t)&ret0 },
	{ "Android_JNI_GetEnv", (uintptr_t)&Android_JNI_GetEnv },
	{ "raise", (uintptr_t)&raise },
};
static size_t numhooks = sizeof(default_dynlib) / sizeof(*default_dynlib);

void *dlsym_hook( void *handle, const char *symbol) {
	printf("Searching for %s...\n", symbol);
	
	for (size_t i = 0; i < numhooks; i++) {
		if (!strcmp(symbol, default_dynlib[i].symbol)) {
			return (void *)default_dynlib[i].func;
		}
	}
	
	printf("Not Found!\n");
	return NULL;
}

int check_kubridge(void) {
	int search_unk[2];
	return _vshKernelSearchModuleByName("kubridge", search_unk);
}

enum MethodIDs {
	UNKNOWN = 0,
	INIT,
	GET_SCREEN_HEIGHT_PIXEL,
	GET_SCREEN_HEIGHT_INCH,
	GET_HIGH_RESOLUTION,
	GET_CURRENT_LANGUAGE
} MethodIDs;

typedef struct {
	char *name;
	enum MethodIDs id;
} NameToMethodID;

static NameToMethodID name_to_method_ids[] = {
	{ "<init>", INIT },
	{ "GetScreenHeightPixel", GET_SCREEN_HEIGHT_PIXEL },
	{ "GetScreenHeightInch", GET_SCREEN_HEIGHT_INCH },
	{ "GetHighResolution", GET_HIGH_RESOLUTION },
	{ "getCurrentLanguage", GET_CURRENT_LANGUAGE },
};

int GetMethodID(void *env, void *class, const char *name, const char *sig) {
	//printf("GetMethodID: %s\n", name);

	for (int i = 0; i < sizeof(name_to_method_ids) / sizeof(NameToMethodID); i++) {
		if (strcmp(name, name_to_method_ids[i].name) == 0) {
			return name_to_method_ids[i].id;
		}
	}

	return UNKNOWN;
}

int GetStaticMethodID(void *env, void *class, const char *name, const char *sig) {
	//printf("GetStaticMethodID: %s\n", name);
	
	for (int i = 0; i < sizeof(name_to_method_ids) / sizeof(NameToMethodID); i++) {
		if (strcmp(name, name_to_method_ids[i].name) == 0)
			return name_to_method_ids[i].id;
	}

	return UNKNOWN;
}

void CallStaticVoidMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
}

int CallStaticBooleanMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
	switch (methodID) {
	case GET_HIGH_RESOLUTION:
		return 1;
	default:
		return 0;
	}
}

int CallStaticIntMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
	int lang;
	switch (methodID) {
	case GET_SCREEN_HEIGHT_PIXEL:
		return 544;
	case GET_CURRENT_LANGUAGE:
		sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &lang);
		switch (lang) {
		case SCE_SYSTEM_PARAM_LANG_FRENCH:
			return 1;
		case SCE_SYSTEM_PARAM_LANG_GERMAN:
			return 3;
		case SCE_SYSTEM_PARAM_LANG_ITALIAN:
			return 2;
		case SCE_SYSTEM_PARAM_LANG_SPANISH:
			return 4;
		default: // En
			return 0;
		}
	default:
		return 0;	
	}
}

uint64_t CallLongMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
	return -1;
}

void *FindClass(void) {
	return (void *)0x41414141;
}

void *NewGlobalRef(void *env, char *str) {
	return (void *)0x42424242;
}

void DeleteGlobalRef(void *env, char *str) {
}

void *NewObjectV(void *env, void *clazz, int methodID, uintptr_t args) {
	return (void *)0x43434343;
}

void *GetObjectClass(void *env, void *obj) {
	return (void *)0x44444444;
}

char *NewStringUTF(void *env, char *bytes) {
	return bytes;
}

char *GetStringUTFChars(void *env, char *string, int *isCopy) {
	return string;
}

size_t GetStringUTFLength(void *env, char *string) {
	return strlen(string);	
}

int GetJavaVM(void *env, void **vm) {
	*vm = fake_vm;
	return 0;
}

enum {
	MANUFACTURER = 1,
	MODEL,
	BRAND,
	DISPLAY,
	DEVICE,
	XDPI,
	YDPI
};

int GetFieldID(void *env, void *clazz, const char *name, const char *sig) {
	if (!strcmp("xdpi",name))
		return XDPI;
	else if (!strcmp("ydpi",name))
		return YDPI;
	return 0;
}

int GetBooleanField(void *env, void *obj, int fieldID) {
	return 0;
}

void *CallObjectMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
	switch (methodID) {
	default:
		return NULL;
	}
}

int CallBooleanMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
	switch (methodID) {
	default:
		return 0;
	}
}

void CallVoidMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
	switch (methodID) {
	default:
		break;
	}
}

int GetStaticFieldID(void *env, void *clazz, const char *name, const char *sig) {
	if (!strcmp("MANUFACTURER",name))
		return MANUFACTURER;
	else if (!strcmp("MODEL",name))
		return MODEL;
	else if (!strcmp("BRAND",name))
		return BRAND;
	else if (!strcmp("DISPLAY",name))
		return DISPLAY;
	else if (!strcmp("DEVICE",name))
		return DEVICE;
	return 0;
}

void *GetStaticObjectField(void *env, void *clazz, int fieldID) {
	switch (fieldID) {
	case MANUFACTURER:
		return "sony";
	case MODEL:
		return "PSVITA";
	case BRAND:
		return "PlayStation";
	case DISPLAY:
		return "AMOLED";
	case DEVICE:
		return "PSVita";
	default:
		return NULL;
	}
}

void GetStringUTFRegion(void *env, char *str, size_t start, size_t len, char *buf) {
	sceClibMemcpy(buf, &str[start], len);
	buf[len] = 0;
}

void *CallStaticObjectMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
	return NULL;
}

int GetIntField(void *env, void *obj, int fieldID) { return 0; }

float GetFloatField(void *env, void *obj, int fieldID) {
	switch (fieldID) {
	case XDPI:
	case YDPI:
		return 200.0f;
	default:
		return 0.0f;
	}
}

float CallStaticFloatMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
	switch (methodID) {
	case GET_SCREEN_HEIGHT_INCH:
		return 544.0f / 200.0f;
	default:
		if (methodID != UNKNOWN)
			printf("CallStaticDoubleMethodV(%d)\n", methodID);
		return 0;
	}
}

/*int crasher(unsigned int argc, void *argv) {
	uint32_t *nullptr = NULL;
	for (;;) {
		SceCtrlData pad;
		sceCtrlPeekBufferPositive(0, &pad, 1);
		if (pad.buttons & SCE_CTRL_SELECT) *nullptr = 0;
		sceKernelDelayThread(100);
	}
}*/

/*void abort_handler(KuKernelAbortContext *ctx) {
	printf("Crash Detected!!! (Abort Type: 0x%08X)\n", ctx->abortType);
	printf("-----------------\n");
	printf("PC: 0x%08X\n", ctx->pc);
	printf("LR: 0x%08X\n", ctx->lr);
	printf("SP: 0x%08X\n", ctx->sp);
	printf("-----------------\n");
	printf("REGISTERS:\n");
	uint32_t *registers = (uint32_t *)ctx;
	for (int i = 0; i < 13; i++) {
		printf("R%d: 0x%08X\n", i, registers[i]);		
	}
	printf("-----------------\n");
	printf("VFP REGISTERS:\n");
	for (int i = 0; i < 32; i++) {
		printf("D%d: 0x%016llX\n", i, ctx->vfpRegisters[i]);		
	}
	printf("-----------------\n");
	printf("SPSR: 0x%08X\n", ctx->SPSR);
	printf("FPSCR: 0x%08X\n", ctx->FPSCR);
	printf("FPEXC: 0x%08X\n", ctx->FPEXC);
	printf("FSR: 0x%08X\n", ctx->FSR);
	printf("FAR: 0x%08X\n", *(&(ctx->FSR) + 4)); // Using ctx->FAR gives an error for some weird reason
	sceKernelExitProcess(0);
}*/

int main(int argc, char *argv[]) {
	//kuKernelRegisterAbortHandler(abort_handler, NULL);
	//SceUID crasher_thread = sceKernelCreateThread("crasher", crasher, 0x40, 0x1000, 0, 0, NULL);
	//sceKernelStartThread(crasher_thread, 0, NULL);	
	//sceSysmoduleLoadModule(SCE_SYSMODULE_RAZOR_CAPTURE);
	
	SceAppUtilInitParam init_param;
	SceAppUtilBootParam boot_param;
	memset(&init_param, 0, sizeof(SceAppUtilInitParam));
	memset(&boot_param, 0, sizeof(SceAppUtilBootParam));
	sceAppUtilInit(&init_param, &boot_param);
	
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);

	if (check_kubridge() < 0)
		fatal_error("Error kubridge.skprx is not installed.");

	if (!file_exists("ur0:/data/libshacccg.suprx") && !file_exists("ur0:/data/external/libshacccg.suprx"))
		fatal_error("Error libshacccg.suprx is not installed.");

	printf("Loading libc++_shared\n");
	if (so_file_load(&stdcpp_mod, DATA_PATH "/libc++_shared.so", LOAD_ADDRESS) < 0)
		fatal_error("Error could not load %s.", DATA_PATH "/libc++_shared.so");
	so_relocate(&stdcpp_mod);
	so_resolve(&stdcpp_mod, default_dynlib, sizeof(default_dynlib), 0);

	so_flush_caches(&stdcpp_mod);
	so_initialize(&stdcpp_mod);
	
	printf("Loading iconv\n");
	if (so_file_load(&iconv_mod, DATA_PATH "/libiconv.so", LOAD_ADDRESS + 0x1000000) < 0)
		fatal_error("Error could not load %s.", DATA_PATH "/libiconv.so");
	so_relocate(&iconv_mod);
	so_resolve(&iconv_mod, default_dynlib, sizeof(default_dynlib), 0);
	so_flush_caches(&iconv_mod);
	so_initialize(&iconv_mod);
	
	printf("Loading libObbVfs\n");
	if (so_file_load(&obbvfs_mod, DATA_PATH "/libObbVfs.so", LOAD_ADDRESS + 0x2000000) < 0)
		fatal_error("Error could not load %s.", DATA_PATH "/libObbVfs.so");
	so_relocate(&obbvfs_mod);
	so_resolve(&obbvfs_mod, default_dynlib, sizeof(default_dynlib), 0);
	so_flush_caches(&obbvfs_mod);
	so_initialize(&obbvfs_mod);

	printf("Loading libFahrenheit\n");
	if (so_file_load(&fahrenheit_mod, SO_PATH, LOAD_ADDRESS + 0x3000000) < 0)
		fatal_error("Error could not load %s.", SO_PATH);
	so_relocate(&fahrenheit_mod);
	so_resolve(&fahrenheit_mod, default_dynlib, sizeof(default_dynlib), 0);

	patch_game();
	patch_ogg();
	patch_vorbis();
	so_flush_caches(&fahrenheit_mod);
	so_initialize(&fahrenheit_mod);
	
	vglInitWithCustomThreshold(0, SCREEN_W, SCREEN_H, MEMORY_VITAGL_THRESHOLD_MB * 1024 * 1024, 0, 0, 0, SCE_GXM_MULTISAMPLE_4X);
	//vglInitExtended(0, SCREEN_W, SCREEN_H, MEMORY_VITAGL_THRESHOLD_MB * 1024 * 1024, SCE_GXM_MULTISAMPLE_4X); // Debug (Has common dialog usable)

	memset(fake_vm, 'A', sizeof(fake_vm));
	*(uintptr_t *)(fake_vm + 0x00) = (uintptr_t)fake_vm; // just point to itself...
	*(uintptr_t *)(fake_vm + 0x10) = (uintptr_t)ret0;
	*(uintptr_t *)(fake_vm + 0x14) = (uintptr_t)ret0;
	*(uintptr_t *)(fake_vm + 0x18) = (uintptr_t)GetEnv;

	memset(fake_env, 'A', sizeof(fake_env));
	*(uintptr_t *)(fake_env + 0x00) = (uintptr_t)fake_env; // just point to itself...
	*(uintptr_t *)(fake_env + 0x18) = (uintptr_t)FindClass;
	*(uintptr_t *)(fake_env + 0x54) = (uintptr_t)NewGlobalRef;
	*(uintptr_t *)(fake_env + 0x58) = (uintptr_t)DeleteGlobalRef;
	*(uintptr_t *)(fake_env + 0x5C) = (uintptr_t)ret0; // DeleteLocalRef
	*(uintptr_t *)(fake_env + 0x74) = (uintptr_t)NewObjectV;
	*(uintptr_t *)(fake_env + 0x7C) = (uintptr_t)GetObjectClass;
	*(uintptr_t *)(fake_env + 0x84) = (uintptr_t)GetMethodID;
	*(uintptr_t *)(fake_env + 0x8C) = (uintptr_t)CallObjectMethodV;
	*(uintptr_t *)(fake_env + 0x98) = (uintptr_t)CallBooleanMethodV;
	*(uintptr_t *)(fake_env + 0xD4) = (uintptr_t)CallLongMethodV;
	*(uintptr_t *)(fake_env + 0xF8) = (uintptr_t)CallVoidMethodV;
	*(uintptr_t *)(fake_env + 0x178) = (uintptr_t)GetFieldID;
	*(uintptr_t *)(fake_env + 0x17C) = (uintptr_t)GetBooleanField;
	*(uintptr_t *)(fake_env + 0x190) = (uintptr_t)GetIntField;
	*(uintptr_t *)(fake_env + 0x198) = (uintptr_t)GetFloatField;
	*(uintptr_t *)(fake_env + 0x1C4) = (uintptr_t)GetStaticMethodID;
	*(uintptr_t *)(fake_env + 0x1CC) = (uintptr_t)CallStaticObjectMethodV;
	*(uintptr_t *)(fake_env + 0x1D8) = (uintptr_t)CallStaticBooleanMethodV;
	*(uintptr_t *)(fake_env + 0x208) = (uintptr_t)CallStaticIntMethodV;
	*(uintptr_t *)(fake_env + 0x220) = (uintptr_t)CallStaticFloatMethodV;
	*(uintptr_t *)(fake_env + 0x238) = (uintptr_t)CallStaticVoidMethodV;
	*(uintptr_t *)(fake_env + 0x240) = (uintptr_t)GetStaticFieldID;
	*(uintptr_t *)(fake_env + 0x244) = (uintptr_t)GetStaticObjectField;
	*(uintptr_t *)(fake_env + 0x29C) = (uintptr_t)NewStringUTF;
	*(uintptr_t *)(fake_env + 0x2A0) = (uintptr_t)GetStringUTFLength;
	*(uintptr_t *)(fake_env + 0x2A4) = (uintptr_t)GetStringUTFChars;
	*(uintptr_t *)(fake_env + 0x2A8) = (uintptr_t)ret0;
	*(uintptr_t *)(fake_env + 0x36C) = (uintptr_t)GetJavaVM;
	*(uintptr_t *)(fake_env + 0x374) = (uintptr_t)GetStringUTFRegion;
	
	void (*Java_com_aspyr_base_ASPYR_mainObbFileName)(void *env, int r1, char *filename) = (void *)so_symbol(&fahrenheit_mod, "Java_com_aspyr_base_ASPYR_mainObbFileName");
	void (*Java_com_aspyr_base_ASPYR_patchObbFileName)(void *env, int r1, char *filename) = (void *)so_symbol(&fahrenheit_mod, "Java_com_aspyr_base_ASPYR_patchObbFileName");

	Java_com_aspyr_base_ASPYR_mainObbFileName(fake_env, 0, "ux0:data/fahrenheit/main.obb");
	Java_com_aspyr_base_ASPYR_patchObbFileName(fake_env, 0, "ux0:data/fahrenheit/patch.obb");

	void (*Java_org_libsdl_app_SDLActivity_nativeInit)() = (void *)so_symbol(&fahrenheit_mod, "Java_org_libsdl_app_SDLActivity_nativeInit");
	Java_org_libsdl_app_SDLActivity_nativeInit();
	
	return 0;
}

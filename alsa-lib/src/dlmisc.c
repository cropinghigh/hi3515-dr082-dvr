/**
 * \file dlmisc.c
 * \brief dynamic loader helpers
 * \author Jaroslav Kysela <perex@perex.cz>
 * \date 2001
 *
 * Dynamic loader helpers
 */
/*
 *  Dynamic loader helpers
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "list.h"
#include "local.h"
#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
#endif
#include <limits.h>
#include <linux/limits.h>

#if defined(HAVE_LIBDL) && defined(__GLIBC__) && !defined(__UCLIBC__)
#define DL_ORIGIN_AVAILABLE 1
#endif

#ifndef DOC_HIDDEN
#ifndef PIC
struct snd_dlsym_link *snd_dlsym_start = NULL;
#endif
static int snd_plugin_dir_set = 0;
static char *snd_plugin_dir = NULL;
#endif

#ifdef HAVE_LIBPTHREAD
static pthread_mutex_t snd_dlpath_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void snd_dlpath_lock(void)
{
	pthread_mutex_lock(&snd_dlpath_mutex);
}

static inline void snd_dlpath_unlock(void)
{
	pthread_mutex_unlock(&snd_dlpath_mutex);
}
#else
static inline void snd_dlpath_lock(void) {}
static inline void snd_dlpath_unlock(void) {}
#endif

static void snd_dlinfo_origin(char *path, size_t path_len)
{
#ifdef DL_ORIGIN_AVAILABLE
	struct link_map *links;
	Dl_info info;
	char origin[PATH_MAX];
	if (dladdr1(&snd_dlpath, &info, (void**)&links, RTLD_DL_LINKMAP) == 0)
		return;
	if (dlinfo(links, RTLD_DI_ORIGIN, origin))
		return;
	snprintf(path, path_len, "%s/alsa-lib", origin);
	if (access(path, X_OK) == 0)
		snd_plugin_dir = strdup(path);
#endif
}

/**
 *
 * \brief Compose the dynamic path
 * \param path Returned path (string)
 * \param path_len Returned path max size (with trailing zero)
 * \param name Plugin name (relative)
 * \return Zero on success, otherwise a negative error code
 */
int snd_dlpath(char *path, size_t path_len, const char *name)
{
	snd_dlpath_lock();
	if (!snd_plugin_dir_set) {
		const char *env = getenv("ALSA_PLUGIN_DIR");
		if (env) {
			snd_plugin_dir = strdup(env);
		} else {
			snd_dlinfo_origin(path, path_len);
		}
		snd_plugin_dir_set = 1;
	}
	snprintf(path, path_len, "%s/%s",
		 snd_plugin_dir ? snd_plugin_dir : ALSA_PLUGIN_DIR, name);
	snd_dlpath_unlock();
	return 0;
}

/**
 * \brief Opens a dynamic library - ALSA wrapper for \c dlopen.
 * \param name name of the library, similar to \c dlopen.
 * \param mode mode flags, similar to \c dlopen.
 * \param errbuf a string buffer for the error message \c dlerror.
 * \param errbuflen a length of the string buffer for the error message.
 * \return Library handle if successful, otherwise \c NULL.
 *
 * This function can emulate dynamic linking for the static build of
 * the alsa-lib library. In that case, \p name is set to \c NULL.
 */
#ifndef DOXYGEN
EXPORT_SYMBOL void *INTERNAL(snd_dlopen)(const char *name, int mode, char *errbuf, size_t errbuflen)
#else
void *snd_dlopen(const char *name, int mode, char *errbuf, size_t errbuflen)
#endif
{
#ifndef PIC
	if (name == NULL)
		return &snd_dlsym_start;
#else
#ifdef HAVE_LIBDL
	if (name == NULL) {
		static const char * self = NULL;
		if (self == NULL) {
			Dl_info dlinfo;
			if (dladdr(snd_dlopen, &dlinfo) > 0)
				self = dlinfo.dli_fname;
		}
		name = self;
	}
#endif
#endif
#ifdef HAVE_LIBDL
	/*
	 * Handle the plugin dir not being on the default dlopen search
	 * path, without resorting to polluting the entire system namespace
	 * via ld.so.conf.
	 */
	void *handle = NULL;
	const char *filename = name;
	char path[PATH_MAX];

	if (name && name[0] != '/') {
		if (snd_dlpath(path, sizeof(path), name) == 0)
			filename = path;
	}
	handle = dlopen(filename, mode);
	if (!handle)
		goto errpath;
	return handle;
errpath:
	if (errbuf)
		snprintf(errbuf, errbuflen, "%s", dlerror());
#endif
	return NULL;
}

#ifndef DOXYGEN
EXPORT_SYMBOL void *INTERNAL(snd_dlopen_old)(const char *name, int mode)
{
  return INTERNAL(snd_dlopen)(name, mode, NULL, 0);
}
#endif

use_symbol_version(__snd_dlopen_old, snd_dlopen, ALSA_0.9);
use_default_symbol_version(__snd_dlopen, snd_dlopen, ALSA_1.1.6);

/**
 * \brief Closes a dynamic library - ALSA wrapper for \c dlclose.
 * \param handle Library handle, similar to \c dlclose.
 * \return Zero if successful, otherwise an error code.
 *
 * This function can emulate dynamic linking for the static build of
 * the alsa-lib library.
 */
int snd_dlclose(void *handle)
{
#ifndef PIC
	if (handle == &snd_dlsym_start)
		return 0;
#endif
#ifdef HAVE_LIBDL
	return dlclose(handle);
#else
	return 0;
#endif
}

/**
 * \brief Verifies a dynamically loaded symbol.
 * \param handle Library handle, similar to \c dlsym.
 * \param name Symbol name.
 * \param version Version of the symbol.
 * \return Zero is successful, otherwise a negative error code.
 *
 * This function checks that the symbol with the version appended to its name
 * does exist in the library.
 */
static int snd_dlsym_verify(void *handle, const char *name, const char *version)
{
#ifdef HAVE_LIBDL
	int res;
	char *vname;
	
	if (handle == NULL)
		return -EINVAL;
	vname = alloca(1 + strlen(name) + strlen(version) + 1);
	if (vname == NULL)
		return -ENOMEM;
	vname[0] = '_';
	strcpy(vname + 1, name);
	strcat(vname, version);
	res = dlsym(handle, vname) == NULL ? -ENOENT : 0;
	// printf("dlsym verify: %i, vname = '%s'\n", res, vname);
	if (res < 0)
		SNDERR("unable to verify version for symbol %s", name);
	return res;
#else
	return 0;
#endif
}

/**
 * \brief Resolves a symbol from a dynamic library - ALSA wrapper for \c dlsym.
 * \param handle Library handle, similar to \c dlsym.
 * \param name Symbol name.
 * \param version Version of the symbol.
 *
 * This function can emulate dynamic linking for the static build of
 * the alsa-lib library.
 *
 * This special version of the \c dlsym function checks also the version
 * of the symbol. A versioned symbol should be defined using the
 * #SND_DLSYM_BUILD_VERSION macro.
 */
void *snd_dlsym(void *handle, const char *name, const char *version)
{
	int err;

#ifndef PIC
	if (handle == &snd_dlsym_start) {
		/* it's the funny part: */
		/* we are looking for a symbol in a static library */
		struct snd_dlsym_link *link = snd_dlsym_start;
		while (link) {
			if (!strcmp(name, link->dlsym_name))
				return (void *)link->dlsym_ptr;
			link = link->next;
		}
		return NULL;
	}
#endif
#ifdef HAVE_LIBDL
#ifdef VERSIONED_SYMBOLS
	if (version) {
		err = snd_dlsym_verify(handle, name, version);
		if (err < 0)
			return NULL;
	}
#endif
	return dlsym(handle, name);
#else
	return NULL;
#endif
}

/*
 * dlobj cache
 */

#ifndef DOC_HIDDEN
struct dlobj_cache {
	const char *lib;
	const char *name;
	void *dlobj;
	void *func;
	unsigned int refcnt;
	struct list_head list;
};

#ifdef HAVE_LIBPTHREAD
static pthread_mutex_t snd_dlobj_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void snd_dlobj_lock(void)
{
	pthread_mutex_lock(&snd_dlobj_mutex);
}

static inline void snd_dlobj_unlock(void)
{
	pthread_mutex_unlock(&snd_dlobj_mutex);
}
#else
static inline void snd_dlobj_lock(void) {}
static inline void snd_dlobj_unlock(void) {}
#endif

static LIST_HEAD(pcm_dlobj_list);

static struct dlobj_cache *
snd_dlobj_cache_get0(const char *lib, const char *name,
		     const char *version, int verbose)
{
	struct list_head *p;
	struct dlobj_cache *c;
	void *func, *dlobj;
	char errbuf[256];

	list_for_each(p, &pcm_dlobj_list) {
		c = list_entry(p, struct dlobj_cache, list);
		if (c->lib && lib && strcmp(c->lib, lib) != 0)
			continue;
		if (!c->lib && lib)
			continue;
		if (!lib && c->lib)
			continue;
		if (strcmp(c->name, name) == 0) {
			c->refcnt++;
			return c;
		}
	}

	errbuf[0] = '\0';
	dlobj = INTERNAL(snd_dlopen)(lib, RTLD_NOW,
	                   verbose ? errbuf : 0,
	                   verbose ? sizeof(errbuf) : 0);
	if (dlobj == NULL) {
		if (verbose)
			SNDERR("Cannot open shared library %s (%s)",
						lib ? lib : "[builtin]",
						errbuf);
		return NULL;
	}

	func = snd_dlsym(dlobj, name, version);
	if (func == NULL) {
		if (verbose)
			SNDERR("symbol %s is not defined inside %s",
					name, lib ? lib : "[builtin]");
		goto __err;
	}
	c = malloc(sizeof(*c));
	if (! c)
		goto __err;
	c->refcnt = 1;
	c->lib = lib ? strdup(lib) : NULL;
	c->name = strdup(name);
	if ((lib && ! c->lib) || ! c->name) {
		free((void *)c->name);
		free((void *)c->lib);
		free(c);
	      __err:
		snd_dlclose(dlobj);
		return NULL;
	}
	c->dlobj = dlobj;
	c->func = func;
	list_add_tail(&c->list, &pcm_dlobj_list);
	return c;
}

void *snd_dlobj_cache_get(const char *lib, const char *name,
			  const char *version, int verbose)
{
	struct dlobj_cache *c;
	void *func = NULL;

	snd_dlobj_lock();
	c = snd_dlobj_cache_get0(lib, name, version, verbose);
	if (c)
		func = c->func;
	snd_dlobj_unlock();
	return func;
}

void *snd_dlobj_cache_get2(const char *lib, const char *name,
			   const char *version, int verbose)
{
	struct dlobj_cache *c;
	void *func = NULL;

	snd_dlobj_lock();
	c = snd_dlobj_cache_get0(lib, name, version, verbose);
	if (c) {
		func = c->func;
		/* double reference */
		c->refcnt++;
	}
	snd_dlobj_unlock();
	return func;
}

int snd_dlobj_cache_put(void *func)
{
	struct list_head *p;
	struct dlobj_cache *c;
	unsigned int refcnt;

	if (!func)
		return -ENOENT;

	snd_dlobj_lock();
	list_for_each(p, &pcm_dlobj_list) {
		c = list_entry(p, struct dlobj_cache, list);
		if (c->func == func) {
			refcnt = c->refcnt;
			if (c->refcnt > 0)
				c->refcnt--;
			snd_dlobj_unlock();
			return refcnt == 1 ? 0 : -EINVAL;
		}
	}
	snd_dlobj_unlock();
	return -ENOENT;
}

void snd_dlobj_cache_cleanup(void)
{
	struct list_head *p, *npos;
	struct dlobj_cache *c;

	snd_dlobj_lock();
	list_for_each_safe(p, npos, &pcm_dlobj_list) {
		c = list_entry(p, struct dlobj_cache, list);
		if (c->refcnt)
			continue;
		list_del(p);
		snd_dlclose(c->dlobj);
		free((void *)c->name); /* shut up gcc warning */
		free((void *)c->lib); /* shut up gcc warning */
		free(c);
	}
	snd_dlobj_unlock();
	snd_dlpath_lock();
	snd_plugin_dir_set = 0;
	free(snd_plugin_dir);
	snd_plugin_dir = NULL;
	snd_dlpath_unlock();
}
#endif

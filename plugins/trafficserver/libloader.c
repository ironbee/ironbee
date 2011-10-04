#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <ts/ts.h>

typedef struct {
  void *handle;
  void *next;
} link_handle;

static link_handle *libs = NULL;

static void unloadlibs()
{
  link_handle *p = libs;
  while (p != NULL) {
    link_handle *next = p->next;
    dlclose(p->handle);
    TSfree(p);
    p = next;
  }
  libs = NULL;
}

void TSPluginInit(int argc, const char *argv[])
{
  int i;
  TSPluginRegistrationInfo info;

  info.plugin_name = "libloader";
  info.vendor_name = "Qualys, Inc";
  info.support_email = "ironbee-users@lists.sourceforge.com";

  if (TSPluginRegister(TS_SDK_VERSION_3_0, &info) != TS_SUCCESS) {
    TSError("[libloader] Plugin registration failed.\n");
    return;
  }
  atexit(unloadlibs);

  for (i = 1; i < argc; ++i) {
    const char *lib = argv[i];
    void *handle = dlopen(lib, RTLD_GLOBAL|RTLD_NOW);
    if (handle) {
      link_handle *l = TSmalloc(sizeof(link_handle));
      l->handle = handle;
      l->next = libs;
      libs = l;
      TSDebug("libloader", " loaded %s\n", lib);
    }
    else {
      TSError("[libloader] failed to load %s: %s\n", lib, dlerror());
    }
  }
  return;

}

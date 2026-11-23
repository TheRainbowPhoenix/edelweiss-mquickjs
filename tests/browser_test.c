/* Host-side navigation regression test for the Toybox-backed browser.
   Standalone OS-world calls are additionally checked by SH cross-build. */
#include "browser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define HOME_DIR "\\\\drv0\\CASIO\\TOYBX\\_CONTENT"
#define NESTED_DIR HOME_DIR "\\nested"
struct fixture { const char *name; int folder; };
static const struct fixture home_files[]={
    {"demo.js",0},{"nested",1},{"read.me",0},{"OTHER.JS",0}
};
static const struct fixture nested_files[]={{"APP.JS",0}};
static const int *key_queue;
static int key_at,key_length;

static int listing(const char *dir,unsigned first,struct shell_dirent *dest,
                   unsigned cap)
{
    const struct fixture *files=NULL;
    unsigned total=0;
    if(!strcmp(dir,HOME_DIR)) {
        files=home_files;total=sizeof home_files/sizeof home_files[0];
    } else if(!strcmp(dir,NESTED_DIR)) {
        files=nested_files;total=sizeof nested_files/sizeof nested_files[0];
    } else return -1;
    for(unsigned i=0;i<cap && first+i<total;i++) {
        snprintf(dest[i].name,sizeof dest[i].name,"%s",files[first+i].name);
        dest[i].directory=files[first+i].folder;
    }
    return (int)total;
}
static int fsize(const char *path)
{
    return !strcmp(path,HOME_DIR "\\demo.js")?703:
           !strcmp(path,NESTED_DIR "\\APP.JS")?12:-1;
}
static void rect(int x,int y,int w,int h,uint16_t color)
{ (void)x;(void)y;(void)w;(void)h;(void)color; }
static void text(int x,int y,uint16_t color,const char *s)
{ (void)x;(void)y;(void)color;(void)s; }
static void present(void){}
static int wait_key(struct edl_event *ev)
{
    assert(key_at<key_length);
    ev->key=key_queue[key_at++];
    return 0;
}
static const struct edl_api host={
    .magic=EDL_ABI_MAGIC,.version=1,.size=sizeof(struct edl_api),
    .image_path=HOME_DIR "\\qjs.edl",
    .width=528,.height=320,
    .rect=rect,.text=text,.present=present,.wait=wait_key
};
static const struct shell_api shell={
    .cwd=HOME_DIR,.list_dir=listing,.file_size=fsize
};

static int native_calls;
static int native_os_call(uint32_t id,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{
    (void)a;(void)b;(void)c;(void)d;
    native_calls++;
    /* A raw findfirst/findnext/close must never be called by an EDL. */
    assert(id==EDL_OS_BULK_LIST || id==EDL_OS_FILE_SIZE);
    return id==EDL_OS_FILE_SIZE?703:0; /* synthetic empty directory */
}
static const struct edl_api standalone={
    .magic=EDL_ABI_MAGIC,.version=1,.size=sizeof(struct edl_api),
    .image_path=HOME_DIR "\\main.edl",
    .width=528,.height=320,.rect=rect,.text=text,.present=present,
    .wait=wait_key,.os_call=native_os_call
};

static void select_with(const int *keys,int count,const char *expected)
{
    key_queue=keys;key_at=0;key_length=count;
    char chosen[EDL_PATH_MAX]={0};
    assert(qjs_choose_script(&host,&shell,chosen)==(expected?1:0));
    assert(key_at==count);
    if(expected)assert(!strcmp(chosen,expected));
}
int main(void)
{
    /* A 703-byte script is not 512-byte aligned: the launcher must read
       only its real length, not request 512 bytes past EOF. */
    assert(qjs_script_size(&host,&shell,HOME_DIR "\\demo.js")==703);
    char case_path[EDL_PATH_MAX]=HOME_DIR "\\DEMO.JS";
    qjs_correct_shell_filename(&shell,case_path);
    assert(!strcmp(case_path,HOME_DIR "\\demo.js"));
    const int direct[]={EDL_OK};
    select_with(direct,1,HOME_DIR "\\demo.js");
    const int nested[]={EDL_DOWN,EDL_OK,EDL_OK};
    select_with(nested,3,NESTED_DIR "\\APP.JS");
    const int cancel[]={EDL_HOME};
    select_with(cancel,1,NULL);
    assert(qjs_script_size(&standalone,NULL,HOME_DIR "\\demo.js")==703);
    native_calls=0;
    key_queue=cancel;key_at=0;key_length=1;
    char selected[EDL_PATH_MAX]={0};
    assert(qjs_choose_script(&standalone,NULL,selected)==0);
    assert(native_calls==1); /* Only one safe resident bulk scan. */
    puts("PASS: native-file-size interface, filename casing and JS picker navigation");
    return 0;
}

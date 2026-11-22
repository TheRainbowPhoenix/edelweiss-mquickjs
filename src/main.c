/* MicroQuickJS interpreter for Edelweiss + Toybox on EX-word.
 * MQuickJS is MIT licensed (Fabrice Bellard and Charlie Gordon).
 * This glue uses the resident host callbacks, never raw native FS syscalls.
 */
#include "edl.h"
#include "toybox_abi.h"
#include "mquickjs.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define VM_BYTES (160u * 1024u)
#define JS_FILE_MAX (96u * 1024u)
#define MAX_FILES 64
#define NAME_MAX_LOCAL EDL_PATH_MAX
#define INK 0x0841
#define PAPER 0xffff
#define HILITE 0x2273

__attribute__((used,section(".edl.cmd"),aligned(4)))
const struct shell_manifest shell_commands = {
    .magic=0x434d4431u, .version=1, .count=2,
    .names={"js","qjs"}
};

static const struct edl_api *host;
static const struct shell_api *shell;
static int log_y, graphics_started;
static char filenames[MAX_FILES][NAME_MAX_LOCAL];
static unsigned filename_count;

/* gint_start.c normally keeps the API private to gint_fs.c. Linker --wrap
   intercepts that one call without changing Edelweiss's startup/ABI. */
extern void __real_edl_gint_set_api(const struct edl_api *api);
void __wrap_edl_gint_set_api(const struct edl_api *api)
{
    host=api;
    shell=toybox_api(api);
    __real_edl_gint_set_api(api);
}

static void print_bytes(const char *s, size_t length)
{
    if (shell && shell->console_write) {
        while (length) {
            unsigned n=length>512?512:(unsigned)length;
            int rc=shell->console_write(s,n);
            if (rc<=0) break;
            if ((unsigned)rc>n) break;
            s+=rc; length-=(unsigned)rc;
        }
        return;
    }
    if (!host || !host->text || !host->present || graphics_started) return;
    /* Minimal standalone output for JS errors and print(), not a TTY. */
    char row[81];
    while (length) {
        size_t n=0;
        while (length && *s!='\n' && n<80) {
            char ch=*s++;
            length--;
            row[n++]=(ch>=32 && ch<=126)?ch:'?';
        }
        if (length && *s=='\n') {s++;length--;}
        row[n]=0;
        if (log_y>host->height-14) {
            host->rect(0,0,(int)host->width,(int)host->height,PAPER);
            log_y=6;
        }
        host->text(8,log_y,INK,row);
        log_y+=13;
    }
    host->present();
}
static void log_func(void *opaque, const void *data, size_t length)
{
    (void)opaque;
    print_bytes((const char *)data,length);
}
static void print_cstr(const char *s)
{
    print_bytes(s,strlen(s));
}
static void print_line(const char *s)
{
    print_cstr(s);print_bytes("\n",1);
}
static JSValue js_print(JSContext *ctx,JSValue *this_val,int argc,JSValue *argv)
{
    (void)this_val;
    for(int i=0;i<argc;i++) {
        if(i) print_bytes(" ",1);
        if(JS_IsString(ctx,argv[i])) {
            JSCStringBuf buf;
            size_t n;
            const char *s=JS_ToCStringLen(ctx,&n,argv[i],&buf);
            if(!s) return JS_EXCEPTION;
            print_bytes(s,n);
        } else {
            JS_PrintValueF(ctx,argv[i],JS_DUMP_LONG);
        }
    }
    print_bytes("\n",1);
    return JS_UNDEFINED;
}
static int int_arg(JSContext *ctx,int argc,JSValue *argv,int i,int *out)
{
    if(i>=argc) {JS_ThrowTypeError(ctx,"missing argument");return -1;}
    return JS_ToInt32(ctx,out,argv[i]);
}
static int color_arg(JSContext *ctx,int argc,JSValue *argv,int i,int *out)
{
    if(int_arg(ctx,argc,argv,i,out)) return -1;
    *out&=65535;
    return 0;
}
static JSValue js_gint_dclear(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int color=PAPER;
    if(argc && color_arg(ctx,argc,argv,0,&color))return JS_EXCEPTION;
    if(host && host->rect) {
        host->rect(0,0,(int)host->width,(int)host->height,(uint16_t)color);
        graphics_started=1;
    }
    return JS_UNDEFINED;
}
static JSValue js_gint_dtext(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int x,y,color;
    if(int_arg(ctx,argc,argv,0,&x) || int_arg(ctx,argc,argv,1,&y) ||
       color_arg(ctx,argc,argv,2,&color) || argc<4)
        return JS_ThrowTypeError(ctx,"dtext(x,y,color,text)");
    JSCStringBuf buf;
    const char *s=JS_ToCString(ctx,argv[3],&buf);
    if(!s)return JS_EXCEPTION;
    if(host && host->text) host->text(x,y,(uint16_t)color,s);
    graphics_started=1;
    return JS_UNDEFINED;
}
static JSValue js_gint_drect(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int x1,y1,x2,y2,c;
    if(int_arg(ctx,argc,argv,0,&x1) || int_arg(ctx,argc,argv,1,&y1) ||
       int_arg(ctx,argc,argv,2,&x2) || int_arg(ctx,argc,argv,3,&y2) ||
       color_arg(ctx,argc,argv,4,&c)) return JS_EXCEPTION;
    if(host && host->rect) {
        /* Host rect() requires an entirely in-bounds rectangle. Clip first. */
        if(x1>x2){int t=x1;x1=x2;x2=t;}
        if(y1>y2){int t=y1;y1=y2;y2=t;}
        if(x1<0)x1=0; if(y1<0)y1=0;
        if(x2>=(int)host->width)x2=(int)host->width-1;
        if(y2>=(int)host->height)y2=(int)host->height-1;
        if(x1<=x2 && y1<=y2)
            host->rect(x1,y1,x2-x1+1,y2-y1+1,(uint16_t)c);
        graphics_started=1;
    }
    return JS_UNDEFINED;
}
static JSValue js_gint_dupdate(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)ctx;(void)th;(void)argc;(void)argv;
    if(host && host->present)host->present();
    graphics_started=1;
    return JS_UNDEFINED;
}
static JSValue js_gint_getkey(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)ctx;(void)th;(void)argc;(void)argv;
    struct edl_event e={0};
    if(!host || !host->wait || host->wait(&e))return JS_NewInt32(ctx,0);
    return JS_NewInt32(ctx,e.key);
}
static JSValue js_gint_wait(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int ms=0;
    if(int_arg(ctx,argc,argv,0,&ms))return JS_EXCEPTION;
    if(ms<0 || ms>60000)return JS_ThrowRangeError(ctx,"wait: 0..60000 ms");
    if(!host || !host->wait_timeout)
        return JS_ThrowTypeError(ctx,"wait(ms) requires host wait_timeout");
    struct edl_event e={0};
    if(host->wait_timeout(&e,(unsigned)ms))return JS_NewInt32(ctx,0);
    return JS_NewInt32(ctx,e.key);
}
static JSValue js_gint_width(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;(void)argc;(void)argv;
    return JS_NewInt32(ctx,host?(int)host->width:0);
}
static JSValue js_gint_height(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;(void)argc;(void)argv;
    return JS_NewInt32(ctx,host?(int)host->height:0);
}
static JSValue js_gint_rgb(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int r,g,b;
    if(int_arg(ctx,argc,argv,0,&r) || int_arg(ctx,argc,argv,1,&g) ||
       int_arg(ctx,argc,argv,2,&b))return JS_EXCEPTION;
    if(r<0)r=0; if(r>255)r=255;
    if(g<0)g=0; if(g>255)g=255;
    if(b<0)b=0; if(b>255)b=255;
    return JS_NewInt32(ctx,((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
#include "mqjs_stdlib.h"

static int has_js_extension(const char *s)
{
    size_t n=strlen(s);
    if(n<4)return 0;
    char a=s[n-3],b=s[n-2],c=s[n-1];
    return a=='.' && (b=='j'||b=='J') && (c=='s'||c=='S');
}
static int candidate(const char *name)
{
    if(!name || !has_js_extension(name) || strlen(name)>=NAME_MAX_LOCAL)
        return 0;
    for(unsigned i=0;i<filename_count;i++)
        if(!strcmp(filenames[i],name))return 0;
    if(filename_count==MAX_FILES)return 0;
    strcpy(filenames[filename_count++],name);
    return 1;
}
static int sibling_path(const char *name,char path[EDL_PATH_MAX])
{
    if(!host || !host->image_path)return -1;
    const char *sep=strrchr(host->image_path,'\\');
    if(!sep)return -1;
    size_t base=(size_t)(sep-host->image_path)+1;
    size_t len=strlen(name);
    if(base+len>=EDL_PATH_MAX)return -1;
    memcpy(path,host->image_path,base);
    memcpy(path+base,name,len+1);
    return 0;
}
static int resolve_script(const char *name,char path[EDL_PATH_MAX])
{
    if(!name || !*name)return -1;
    if(shell && shell->resolve_path)
        return shell->resolve_path(name,path);
    if(name[0]=='/' || name[0]=='\\') {
        size_t len=strlen(name);
        if(len>=EDL_PATH_MAX)return -1;
        /* Standardize absolute /drv0/NAME.JS to native UNC. */
        size_t dst=0;
        if(name[0]=='/' && name[1]!='/'){path[dst++]='\\';path[dst++]='\\';name++;}
        else if(name[0]=='/' && name[1]=='/') {path[dst++]='\\';path[dst++]='\\';name+=2;}
        for(;*name && dst+1<EDL_PATH_MAX;name++)
            path[dst++]=*name=='/'?'\\':*name;
        path[dst]=0;
        return *name?-1:0;
    }
    return sibling_path(name,path);
}
static void standalone_candidates(void)
{
    /* Edelweiss does not enumerate arbitrary storage files. SCRIPTS.TXT
       can list siblings or full /drv0/ and /crd0/ script paths. */
    char path[EDL_PATH_MAX],buf[2049];
    if(sibling_path("SCRIPTS.TXT",path))return;
    int n=host->read(path,0,buf,2048);
    if(n>0 && n<=2048) {
        buf[n]=0;
        char *p=buf;
        while(*p) {
            char *end=p;
            while(*end && *end!='\r' && *end!='\n')end++;
            char saved=*end;*end=0;
            candidate(p);
            if(!saved)break;
            p=end+1;
            while(*p=='\r' || *p=='\n')p++;
        }
    }
    /* Convenient defaults if the index has not been created. */
    const char *defaults[]={"MAIN.JS","SCRIPT.JS","DEMO.JS"};
    for(unsigned i=0;i<sizeof defaults/sizeof defaults[0];i++){
        if(sibling_path(defaults[i],path))continue;
        char byte;
        if(host->read(path,0,&byte,1)>0)candidate(defaults[i]);
    }
}
static void shell_candidates(void)
{
    if(!shell || !shell->list_dir)return;
    struct shell_dirent entries[8];
    int total=shell->list_dir(shell->cwd,0,entries,8);
    if(total<=0)return;
    for(unsigned first=0;first<(unsigned)total;first+=8){
        if(shell->list_dir(shell->cwd,first,entries,8)<0)break;
        for(unsigned i=0;i<8 && first+i<(unsigned)total;i++)
            if(!entries[i].directory)candidate(entries[i].name);
    }
}
static int choose_file(char out[EDL_PATH_MAX])
{
    filename_count=0;
    if(shell)shell_candidates();
    else standalone_candidates();
    if(!host || !host->rect || !host->text || !host->present || !host->wait)
        return -1;
    unsigned selected=0;
    for(;;){
        host->rect(0,0,(int)host->width,(int)host->height,PAPER);
        host->text(12,12,INK,"MicroQuickJS - select a .JS script");
        host->text(12,30,INK,"UP/DOWN: move   EXE: run   EXIT: close");
        if(filename_count) {
            char shown[65];
            const char *p=filenames[selected];
            size_t len=strlen(p);
            if(len>60)p+=len-60;
            strcpy(shown,p);
            host->text(12,46,INK,shown);
        }
        if(!filename_count)host->text(12,64,INK,"No scripts found (see SCRIPTS.TXT).");
        unsigned first=selected>8?selected-8:0;
        for(unsigned i=first;i<filename_count && i<first+10;i++){
            int y=64+(int)(i-first)*21;
            if(i==selected)host->rect(8,y-3,(int)host->width-16,19,HILITE);
            const char *label=filenames[i];
            const char *slash=strrchr(label,'/');
            const char *backslash=strrchr(label,'\\');
            if(slash && (!backslash || slash>backslash))label=slash+1;
            else if(backslash)label=backslash+1;
            host->text(16,y,i==selected?PAPER:INK,label);
        }
        host->present();
        struct edl_event e={0};
        if(host->wait(&e))return -1;
        if(e.key==EDL_BACK || e.key==EDL_HOME)return 0;
        if(e.key==EDL_UP && selected>0)selected--;
        if(e.key==EDL_DOWN && selected+1<filename_count)selected++;
        if(e.key==EDL_OK && filename_count)
            return resolve_script(filenames[selected],out)?-1:1;
    }
}
static int read_source(const char *path,char **out,size_t *length)
{
    char *buf=malloc(JS_FILE_MAX+1);
    if(!buf)return -1;
    size_t at=0;
    while(at<JS_FILE_MAX){
        unsigned take=512;
        if(take>JS_FILE_MAX-at)take=(unsigned)(JS_FILE_MAX-at);
        int n=host->read(path,(uint32_t)at,buf+at,take);
        if(n<0 || (unsigned)n>take){free(buf);return -1;}
        if(!n)break;
        at+=(unsigned)n;
    }
    if(at==JS_FILE_MAX){
        char extra;
        if(host->read(path,(uint32_t)at,&extra,1)>0){free(buf);return -2;}
    }
    buf[at]=0;
    *out=buf;*length=at;
    return 0;
}
int main(void)
{
    if(!host || !host->read)return -1;
    char path[EDL_PATH_MAX]={0};
    if(shell && shell->argc>=2){
        if(resolve_script(shell->argv[1],path)){
            print_line("js: invalid script path");return 2;
        }
    } else {
        int selected=choose_file(path);
        if(selected<=0)return selected<0?2:0;
    }
    /* After a picker there is no terminal, so show an empty log screen. */
    graphics_started=0;log_y=8;
    if(host->rect && !shell)host->rect(0,0,(int)host->width,(int)host->height,PAPER);
    char *source=0;
    size_t len=0;
    int rc=read_source(path,&source,&len);
    if(rc){
        print_line(rc==-2?"js: source exceeds 96 KiB":"js: cannot read script");
        return 2;
    }
    void *memory=malloc(VM_BYTES);
    if(!memory){free(source);print_line("js: out of memory");return 2;}
    JSContext *ctx=JS_NewContext(memory,VM_BYTES,&js_stdlib);
    if(!ctx){free(source);free(memory);print_line("js: JS context failed");return 2;}
    JS_SetLogFunc(ctx,log_func);
    JSGCRef program_ref;
    JSValue *program=JS_PushGCRef(ctx,&program_ref);
    *program=JS_Parse(ctx,source,len,path,0);
    if(JS_IsException(*program))goto error;
    JSGCRef array_ref;
    JSValue *array=JS_PushGCRef(ctx,&array_ref);
    int count=shell && shell->argc>2?shell->argc-2:0;
    *array=JS_NewArray(ctx,count);
    if(JS_IsException(*array)){JS_PopGCRef(ctx,&array_ref);goto error;}
    for(int i=0;i<count;i++){
        JSValue item=JS_NewString(ctx,shell->argv[i+2]);
        if(JS_IsException(item) ||
           JS_IsException(JS_SetPropertyUint32(ctx,*array,i,item))){
            JS_PopGCRef(ctx,&array_ref);
            goto error;
        }
    }
    JSValue global=JS_GetGlobalObject(ctx);
    if(JS_IsException(JS_SetPropertyStr(ctx,global,"scriptArgs",*array))){
        JS_PopGCRef(ctx,&array_ref);
        goto error;
    }
    JS_PopGCRef(ctx,&array_ref);
    JSValue path_value=JS_NewString(ctx,path);
    if(JS_IsException(path_value))goto error;
    global=JS_GetGlobalObject(ctx);
    if(JS_IsException(JS_SetPropertyStr(ctx,global,"scriptPath",path_value)))
        goto error;
    JSValue value=JS_Run(ctx,*program);
    if(JS_IsException(value))goto error;
    JS_PopGCRef(ctx,&program_ref);
    JS_FreeContext(ctx);
    free(source);
    free(memory);
    return 0;
error:
    {
        JSValue err=JS_GetException(ctx);
        print_cstr("js exception: ");
        JS_PrintValueF(ctx,err,JS_DUMP_LONG);
        print_bytes("\n",1);
    }
    JS_PopGCRef(ctx,&program_ref);
    JS_FreeContext(ctx);
    free(source);
    free(memory);
    return 1;
}
EDL_METADATA const struct edl_metadata edl_meta={
    .magic=EDL_ABI_MAGIC,.version=1,
    .title="MicroQuickJS",.description="JavaScript interpreter and gint graphics"
};

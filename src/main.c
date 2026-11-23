/* MicroQuickJS interpreter for Edelweiss + Toybox on EX-word.
 * MQuickJS is MIT licensed (Fabrice Bellard and Charlie Gordon).
 * This glue uses the resident host callbacks, never raw native FS syscalls.
 */
#include "edl.h"
#include "toybox_abi.h"
#include "browser.h"
#include "gint_draw.h"
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
/* Device diagnostics: distinguish ELF/startup, FS, VM, parsing and execution.
   A long-running JS script must never look like a frozen loader screen. */
static void stage(const char *name)
{
    if(!host || !host->rect || !host->text || !host->present)return;
    host->rect(0,0,(int)host->width,(int)host->height,0xffff);
    host->text(12,12,0x0841,"MicroQuickJS - starting");
    host->text(12,39,0x2273,name);
    host->text(12,73,0x0841,"If frozen, report the displayed stage.");
    host->present();
}

static char filenames[MAX_FILES][NAME_MAX_LOCAL];
static unsigned filename_count;

/* gint_start.c normally keeps the API private to gint_fs.c. Linker --wrap
   intercepts that one call without changing Edelweiss's startup/ABI. */
extern void __real_edl_gint_set_api(const struct edl_api *api);
void __wrap_edl_gint_set_api(const struct edl_api *api)
{
    host=api;
    shell=toybox_api(api);
    qjs_draw_init(api);
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
/* Drawing is bounded so an untrusted JS coordinate cannot force a multi-
   million iteration raster loop or overflow 32-bit geometry calculations. */
static int coordinate_arg(JSContext *ctx,int argc,JSValue *argv,int i,int *out)
{
    if(int_arg(ctx,argc,argv,i,out))return -1;
    if(*out < -4096 || *out > 4096){
        JS_ThrowRangeError(ctx,"coordinate must be -4096..4096");
        return -1;
    }
    return 0;
}
static int color_or_none(JSContext *ctx,int argc,JSValue *argv,int i,int *out)
{
    if(int_arg(ctx,argc,argv,i,out))return -1;
    if(*out == -1)return 0; /* C_NONE for fill and border */
    *out &= 65535;
    return 0;
}
static JSValue js_gint_dpixel(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int x,y,c;
    if(coordinate_arg(ctx,argc,argv,0,&x) || coordinate_arg(ctx,argc,argv,1,&y) ||
       color_arg(ctx,argc,argv,2,&c))return JS_EXCEPTION;
    qjs_draw_pixel(x,y,c);graphics_started=1;
    return JS_UNDEFINED;
}
static JSValue js_gint_dline(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int x1,y1,x2,y2,c;
    if(coordinate_arg(ctx,argc,argv,0,&x1)||coordinate_arg(ctx,argc,argv,1,&y1)||
       coordinate_arg(ctx,argc,argv,2,&x2)||coordinate_arg(ctx,argc,argv,3,&y2)||
       color_arg(ctx,argc,argv,4,&c))return JS_EXCEPTION;
    qjs_draw_line(x1,y1,x2,y2,c);graphics_started=1;
    return JS_UNDEFINED;
}
static JSValue js_gint_dhline(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int y,c;
    if(coordinate_arg(ctx,argc,argv,0,&y)||color_arg(ctx,argc,argv,1,&c))
        return JS_EXCEPTION;
    qjs_draw_hline(y,c);graphics_started=1;
    return JS_UNDEFINED;
}
static JSValue js_gint_dvline(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int x,c;
    if(coordinate_arg(ctx,argc,argv,0,&x)||color_arg(ctx,argc,argv,1,&c))
        return JS_EXCEPTION;
    qjs_draw_vline(x,c);graphics_started=1;
    return JS_UNDEFINED;
}
static JSValue js_gint_drect_border(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int x1,y1,x2,y2,fill,width,border;
    if(coordinate_arg(ctx,argc,argv,0,&x1)||coordinate_arg(ctx,argc,argv,1,&y1)||
       coordinate_arg(ctx,argc,argv,2,&x2)||coordinate_arg(ctx,argc,argv,3,&y2)||
       color_or_none(ctx,argc,argv,4,&fill)||int_arg(ctx,argc,argv,5,&width)||
       color_or_none(ctx,argc,argv,6,&border))return JS_EXCEPTION;
    if(width<0||width>4096)return JS_ThrowRangeError(ctx,"border width: 0..4096");
    qjs_draw_rect_border(x1,y1,x2,y2,fill,width,border);
    graphics_started=1;return JS_UNDEFINED;
}
static JSValue js_gint_dcircle(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int x,y,r,fill,border;
    if(coordinate_arg(ctx,argc,argv,0,&x)||coordinate_arg(ctx,argc,argv,1,&y)||
       int_arg(ctx,argc,argv,2,&r)||color_or_none(ctx,argc,argv,3,&fill)||
       color_or_none(ctx,argc,argv,4,&border))return JS_EXCEPTION;
    if(r<0||r>4096)return JS_ThrowRangeError(ctx,"circle radius: 0..4096");
    qjs_draw_circle(x,y,r,fill,border);
    graphics_started=1;return JS_UNDEFINED;
}
static JSValue js_gint_dellipse(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int x1,y1,x2,y2,fill,border;
    if(coordinate_arg(ctx,argc,argv,0,&x1)||coordinate_arg(ctx,argc,argv,1,&y1)||
       coordinate_arg(ctx,argc,argv,2,&x2)||coordinate_arg(ctx,argc,argv,3,&y2)||
       color_or_none(ctx,argc,argv,4,&fill)||
       color_or_none(ctx,argc,argv,5,&border))return JS_EXCEPTION;
    qjs_draw_ellipse(x1,y1,x2,y2,fill,border);
    graphics_started=1;return JS_UNDEFINED;
}
static JSValue js_gint_dwindow_set(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int b[4];
    for(int i=0;i<4;i++)if(coordinate_arg(ctx,argc,argv,i,&b[i]))return JS_EXCEPTION;
    qjs_draw_window(b[0],b[1],b[2],b[3]);
    return JS_UNDEFINED;
}
static JSValue js_gint_dwindow_get(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;(void)argc;(void)argv;
    int b[4];qjs_draw_get_window(b);
    JSValue array=JS_NewArray(ctx,4);
    if(JS_IsException(array))return array;
    for(unsigned i=0;i<4;i++)
        if(JS_IsException(JS_SetPropertyUint32(ctx,array,i,JS_NewInt32(ctx,b[i]))))
            return JS_EXCEPTION;
    return array;
}
static JSValue js_gint_dpoly(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int fill,border;
    if(argc<3 || !JS_IsArray(ctx,argv[0]))return JS_ThrowTypeError(ctx,
        "dpoly([x0,y0,...],fill,border)");
    if(color_or_none(ctx,argc,argv,1,&fill)||
       color_or_none(ctx,argc,argv,2,&border))return JS_EXCEPTION;
    JSValue len_value=JS_GetPropertyStr(ctx,argv[0],"length");
    int n=0;
    if(JS_IsException(len_value) || JS_ToInt32(ctx,&n,len_value))return JS_EXCEPTION;
    if(n<6 || n>64 || (n&1))return JS_ThrowRangeError(ctx,
        "dpoly requires 3..32 coordinate pairs");
    int xy[64];
    for(int i=0;i<n;i++){
        JSValue v=JS_GetPropertyUint32(ctx,argv[0],(uint32_t)i);
        if(JS_IsException(v) || JS_ToInt32(ctx,&xy[i],v))return JS_EXCEPTION;
        if(xy[i]<-4096 || xy[i]>4096)return JS_ThrowRangeError(ctx,
            "polygon coordinates must be -4096..4096");
    }
    qjs_draw_poly(xy,(unsigned)n/2,fill,border);
    graphics_started=1;return JS_UNDEFINED;
}
static JSValue js_gint_C_RGB(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int r,g,b;
    if(int_arg(ctx,argc,argv,0,&r)||int_arg(ctx,argc,argv,1,&g)||
       int_arg(ctx,argc,argv,2,&b))return JS_EXCEPTION;
    return JS_NewInt32(ctx,((r&31)<<11)|((g&63)<<5)|(b&31));
}
static JSValue js_gint_getkeyevent(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int timeout=0;
    if(argc && int_arg(ctx,argc,argv,0,&timeout))return JS_EXCEPTION;
    if(timeout<0||timeout>60000)return JS_ThrowRangeError(ctx,
        "getkeyevent timeout must be 0..60000 ms");
    struct edl_event event={0};
    if(host){
        if(timeout && host->wait_timeout)host->wait_timeout(&event,(unsigned)timeout);
        else if(!timeout && host->wait)host->wait(&event);
        else return JS_ThrowTypeError(ctx,"host does not support event timeout");
    }
    JSValue obj=JS_NewObject(ctx);
    if(JS_IsException(obj))return obj;
    const char *fields[]={"type","key","x","y","time"};
    int values[]={event.key?(event.key==EDL_TOUCH?4:1):0,
                  event.key,event.x,event.y,0};
    for(unsigned i=0;i<5;i++)
        if(JS_IsException(JS_SetPropertyStr(ctx,obj,fields[i],
                          JS_NewInt32(ctx,values[i]))))return JS_EXCEPTION;
    if(JS_IsException(JS_SetPropertyStr(ctx,obj,"shift",JS_NewBool(0)))||
       JS_IsException(JS_SetPropertyStr(ctx,obj,"alpha",JS_NewBool(0)))||
       JS_IsException(JS_SetPropertyStr(ctx,obj,"mod",JS_NewBool(0))))
        return JS_EXCEPTION;
    return obj;
}
static JSValue js_gint_pollevent(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)argc;(void)argv;
    JSValue ms=JS_NewInt32(ctx,1);
    return js_gint_getkeyevent(ctx,th,1,&ms);
}

static JSValue js_gint_dclear(JSContext *ctx,JSValue *th,int argc,JSValue *argv)
{
    (void)th;
    int color=PAPER;
    if(argc && color_arg(ctx,argc,argv,0,&color))return JS_EXCEPTION;
    qjs_draw_clear(color);
    graphics_started=1;
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
    if(coordinate_arg(ctx,argc,argv,0,&x1)||
       coordinate_arg(ctx,argc,argv,1,&y1)||
       coordinate_arg(ctx,argc,argv,2,&x2)||
       coordinate_arg(ctx,argc,argv,3,&y2)||
       color_arg(ctx,argc,argv,4,&c))return JS_EXCEPTION;
    qjs_draw_rect(x1,y1,x2,y2,c);
    graphics_started=1;
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

/* Edelweiss read() is EXACT-size and fails on requests past EOF. */
static int read_source(const char *path,char **out,size_t *length)
{
    int size=qjs_script_size(host,shell,path);
    if(size<0)return -1;
    if((unsigned)size>JS_FILE_MAX)return -2;
    char *buf=malloc((size_t)size+1);
    if(!buf)return -1;
    unsigned at=0;
    while(at<(unsigned)size) {
        unsigned take=(unsigned)size-at;
        if(take>256)take=256;
        if(host->read(path,at,buf+at,take)!=(int)take) {
            free(buf);return -1;
        }
        at+=take;
    }
    buf[size]=0;*out=buf;*length=(size_t)size;
    return 0;
}
int main(void)
{
    if(!host || !host->read)return -1;
    stage("Payload initialized");
    char path[EDL_PATH_MAX]={0};
    if(shell && shell->argc>=2){
        if(!shell->resolve_path || shell->resolve_path(shell->argv[1],path)){
            print_line("js: invalid script path");return 2;
        }
        qjs_correct_shell_filename(shell,path);
    } else {
        stage("Opening script browser...");
        int selected=qjs_choose_script(host,shell,path);
        if(selected<=0)return selected<0?2:0;
    }
    /* After a picker there is no terminal, so show an empty log screen. */
    graphics_started=0;log_y=8;
    if(host->rect && !shell)host->rect(0,0,(int)host->width,(int)host->height,PAPER);
    char *source=0;
    size_t len=0;
    stage("Opening/reading JS source...");
    int rc=read_source(path,&source,&len);
    if(rc){
        print_line(rc==-2?"js: source exceeds 96 KiB":"js: cannot read script");
        print_line(path);
        if(!shell && host->wait) {
            struct edl_event key={0};
            host->wait(&key); /* Keep error visible on standalone. */
        }
        return 2;
    }
    stage("Allocating VM buffer...");
    void *memory=malloc(VM_BYTES);
    if(!memory){free(source);print_line("js: out of memory");return 2;}
    stage("Initializing JavaScript VM...");
    JSContext *ctx=JS_NewContext(memory,VM_BYTES,&js_stdlib);
    if(!ctx){free(source);free(memory);print_line("js: JS context failed");return 2;}
    JS_SetLogFunc(ctx,log_func);
    JSGCRef program_ref;
    JSValue *program=JS_PushGCRef(ctx,&program_ref);
    stage("Parsing JavaScript...");
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
    stage("Executing JavaScript...");
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

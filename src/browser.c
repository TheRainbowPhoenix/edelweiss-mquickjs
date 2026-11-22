/* Read-only EX-word JS browser: directory enumeration follows EXWord-Explorer.
   The native findfirst/findnext/findclose syscalls MUST use Edelweiss's
   world-switched os_call; direct native calls from an EDL are unsafe. */
#include "browser.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define ROWS 10u
#define RAW_PAGE 8u
#define SCAN_LIMIT 4096u
#define INK 0x0841
#define PAPER 0xffff
#define HILITE 0x2273
#define DIR_KIND 5u

struct entry { char name[128]; int folder; };
static const char *const volumes[]={
    "drv0","sys0","data0","data1","data2","crd0","crd1","crd2"
};
#define N_VOLUMES (sizeof volumes/sizeof volumes[0])

static int equal_name(const char *a,const char *b)
{
    while(*a && *b) {
        unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='A' && x<='Z')x+=32;
        if(y>='A' && y<='Z')y+=32;
        if(x!=y)return 0;
    }
    return !*a && !*b;
}
static int is_js(const char *name)
{
    size_t n=strlen(name);
    return n>=3 && name[n-3]=='.' &&
           (name[n-2]=='j'||name[n-2]=='J') &&
           (name[n-1]=='s'||name[n-1]=='S');
}
static int join(char out[EDL_PATH_MAX],const char *dir,const char *leaf)
{
    size_t n=strlen(dir),m=strlen(leaf);
    if(!m || n+m+2>EDL_PATH_MAX || strchr(leaf,'\\') ||
       strchr(leaf,'/'))return -1;
    memcpy(out,dir,n);
    out[n++]='\\';
    memcpy(out+n,leaf,m+1);
    return 0;
}
static int parent(char dir[EDL_PATH_MAX])
{
    char *p=strrchr(dir+2,'\\');
    if(!p)return -1; /* Root reached: switch to volume selector. */
    *p=0;
    return 0;
}
static int accept(struct entry *rows,unsigned first,unsigned *found,
                  const char *name,int folder)
{
    size_t n=strlen(name);
    if(!n || n>=sizeof rows[0].name || !strcmp(name,".") ||
       !strcmp(name,"..") || (!folder && !is_js(name)))return 0;
    if(*found>=first && *found-first<ROWS) {
        struct entry *dest=&rows[*found-first];
        memcpy(dest->name,name,n+1);
        dest->folder=folder;
    }
    (*found)++;
    return 1;
}
/* List only the visible window, but count every matching folder/.JS entry.
   This permits browsing directories larger than the display's row budget. */
static int list_directory(const struct edl_api *host,const struct shell_api *shell,
                          const char *dir,unsigned first,struct entry rows[ROWS])
{
    memset(rows,0,ROWS*sizeof *rows);
    unsigned found=0;
    if(shell && shell->list_dir) {
        struct shell_dirent page[RAW_PAGE];
        int total=shell->list_dir(dir,0,page,RAW_PAGE);
        if(total<0)return -1;
        for(unsigned at=0;at<(unsigned)total && at<SCAN_LIMIT;at+=RAW_PAGE) {
            if(at && shell->list_dir(dir,at,page,RAW_PAGE)<0)return -1;
            for(unsigned j=0;j<RAW_PAGE && at+j<(unsigned)total;j++)
                accept(rows,first,&found,page[j].name,page[j].directory);
        }
        return (int)found;
    }
    if(!host || !host->os_call)return -1;
    char glob[EDL_PATH_MAX];
    size_t n=strlen(dir);
    if(!n || n+3>=sizeof glob)return -1;
    memcpy(glob,dir,n);
    if(glob[n-1]!='\\')glob[n++]='\\';
    glob[n++]='*';glob[n]=0;
    int handle=-1;
    char name[512]={0};
    uint32_t kind=0;
    int rc=host->os_call(0x3f,(uint32_t)(uintptr_t)glob,
                         (uint32_t)(uintptr_t)&handle,
                         (uint32_t)(uintptr_t)name,
                         (uint32_t)(uintptr_t)&kind);
    if(rc<0)return rc==-16 || rc==-1 ? 0 : -1;
    unsigned count=0;
    do {
        name[sizeof name-1]=0;
        accept(rows,first,&found,name,kind==DIR_KIND);
        name[0]=0;kind=0;
        if(++count>=SCAN_LIMIT)break;
        rc=host->os_call(0x40,(uint32_t)handle,
                         (uint32_t)(uintptr_t)name,
                         (uint32_t)(uintptr_t)&kind,0);
    }while(rc>=0);
    int closed=host->os_call(0x41,(uint32_t)handle,0,0,0);
    if(closed<0 || (count<SCAN_LIMIT && rc!=-1 && rc!=-16))return -1;
    return (int)found;
}
static void short_name(char out[60],const char *name)
{
    size_t n=strlen(name),start=n>52?n-52:0,j=0;
    if(start){out[j++]='.';out[j++]='.';out[j++]='.';start+=3;}
    while(start<n && j<58) {
        unsigned char c=(unsigned char)name[start++];
        out[j++]=(c>=32 && c<127)?(char)c:'?';
    }
    out[j]=0;
}
static void draw(const struct edl_api *host,const char *dir,int disks,
                 unsigned selected,unsigned top,unsigned total,
                 const struct entry rows[ROWS],const char *error)
{
    host->rect(0,0,(int)host->width,(int)host->height,PAPER);
    host->text(12,10,INK,"MicroQuickJS / JavaScript browser");
    host->text(12,29,INK,"UP/DOWN move   EXE open   EXIT back");
    char visible[60];
    short_name(visible,disks?"Choose storage volume":dir);
    host->text(12,48,HILITE,visible);
    if(!total)host->text(12,78,INK,"No .JS here. EXIT to parent folder.");
    for(unsigned i=0;i<ROWS && top+i<total;i++) {
        unsigned index=top+i;
        int y=70+(int)i*21;
        if(index==selected)
            host->rect(8,y-3,(int)host->width-16,19,HILITE);
        char line[80];
        if(disks)snprintf(line,sizeof line,"[DISK] %s",volumes[index]);
        else {
            short_name(visible,rows[i].name);
            snprintf(line,sizeof line,"%s %s",rows[i].folder?"[DIR]":"  JS ",visible);
        }
        host->text(14,y,index==selected?PAPER:INK,line);
    }
    if(error && *error)host->text(12,(int)host->height-20,INK,error);
    host->present();
}
int qjs_choose_script(const struct edl_api *host,const struct shell_api *shell,
                      char out[EDL_PATH_MAX])
{
    if(!host || !host->rect || !host->text || !host->present || !host->wait)
        return -1;
    char dir[EDL_PATH_MAX]={0},target[EDL_PATH_MAX],error[64]={0};
    struct entry rows[ROWS];
    unsigned selected=0,top=0,total=0;
    int disks=1,dirty=1;
    const char *initial=shell && shell->cwd?shell->cwd:host->image_path;
    if(initial && initial[0]=='\\' && initial[1]=='\\') {
        size_t n=strlen(initial);
        if(n<sizeof dir) {
            memcpy(dir,initial,n+1);
            if(!shell && parent(dir))dir[0]=0;
            disks=!dir[0];
        }
    }
    for(;;) {
        if(!disks && dirty) {
            int rc=list_directory(host,shell,dir,top,rows);
            if(rc<0) {
                total=0;
                snprintf(error,sizeof error,"Cannot read this directory.");
            } else {
                total=(unsigned)rc;
                error[0]=0;
            }
            if(selected>=total)selected=total?total-1:0;
            if(top>selected) {
                top=(selected/ROWS)*ROWS;
                if(total)list_directory(host,shell,dir,top,rows);
            }
            dirty=0;
        }
        unsigned visible=disks?N_VOLUMES:total;
        draw(host,dir,disks,selected,top,visible,rows,error);
        struct edl_event ev={0};
        if(host->wait(&ev))return -1;
        if(ev.key==EDL_HOME)return 0;
        if(ev.key==EDL_BACK) {
            if(disks)return 0;
            if(parent(dir))disks=1;
            selected=top=0;dirty=1;error[0]=0;
        } else if(ev.key==EDL_UP) {
            if(selected)selected--;
            if(!disks && selected<top) {
                top=(selected/ROWS)*ROWS;dirty=1;
            }
        } else if(ev.key==EDL_DOWN) {
            if(selected+1<visible)selected++;
            if(!disks && selected>=top+ROWS) {
                top=(selected/ROWS)*ROWS;dirty=1;
            }
        } else if(ev.key==EDL_OK && selected<visible) {
            if(disks) {
                snprintf(dir,sizeof dir,"\\\\%s",volumes[selected]);
                disks=0;selected=top=0;dirty=1;error[0]=0;
                continue;
            }
            const struct entry *entry=&rows[selected-top];
            if(join(target,dir,entry->name)) {
                snprintf(error,sizeof error,"Path is too long.");
                continue;
            }
            if(!entry->folder) {
                memcpy(out,target,strlen(target)+1);
                return 1;
            }
            memcpy(dir,target,strlen(target)+1);
            selected=top=0;dirty=1;error[0]=0;
        }
    }
}
int qjs_script_size(const struct edl_api *host,const struct shell_api *shell,
                    const char *path)
{
    if(shell && shell->file_size)return shell->file_size(path);
    if(!host || !host->os_call)return -1;
    int fd=host->os_call(0x37,(uint32_t)(uintptr_t)path,1,0,0);
    if(fd<0)return -1;
    int n=host->os_call(0x3a,(uint32_t)fd,0,0,0);
    int closed=host->os_call(0x38,(uint32_t)fd,0,0,0);
    return n>=0 && closed>=0?n:-1;
}
void qjs_correct_shell_filename(const struct shell_api *shell,
                                char path[EDL_PATH_MAX])
{
    if(!shell || !shell->list_dir || !shell->file_size ||
       shell->file_size(path)>=0)return;
    char *slash=strrchr(path,'\\');
    if(!slash || !slash[1])return;
    char dir[EDL_PATH_MAX],actual[EDL_PATH_MAX];
    size_t n=(size_t)(slash-path);
    if(n>=sizeof dir)return;
    memcpy(dir,path,n);dir[n]=0;
    struct shell_dirent page[RAW_PAGE];
    int total=shell->list_dir(dir,0,page,RAW_PAGE);
    if(total<=0)return;
    for(unsigned at=0;at<(unsigned)total && at<SCAN_LIMIT;at+=RAW_PAGE) {
        if(at && shell->list_dir(dir,at,page,RAW_PAGE)<0)return;
        for(unsigned j=0;j<RAW_PAGE && at+j<(unsigned)total;j++) {
            if(!page[j].directory && equal_name(page[j].name,slash+1) &&
               !join(actual,dir,page[j].name)) {
                memcpy(path,actual,strlen(actual)+1);
                return;
            }
        }
    }
}

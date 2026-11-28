/* VMSTRESS.JS - moving-GC and ES5 runtime correctness exercise.
   Keeps working set small so failures are real regressions, not normal OOM.
   Each PASS line includes an approximate phase duration (rtc ticks / 128).
   No timers, dynamic imports, or external files required. */
var g=gint;
var t0=g.ticks();
var phases=0;
function check(v,description){
    if(!v)throw new Error("VMSTRESS "+description);
}
function elapsed(t){
    var d=g.ticks()-t;
    if(d<0)d+=11059200;
    return ((d*1000/128)|0);
}
function done(name,t){
    phases++;
    print("PASS "+name+" "+elapsed(t)+"ms");
}
var i,j,t;

/* 1: integer/bitwise paths + IEEE-754 arithmetic. */
t=g.ticks();
var sum=0,parity=0;
for(i=0;i<2000;i++)sum+=i;
for(i=0;i<1000;i++){
    parity^=(i&255);
    parity^=(i&255);
}
check(sum===1999000 && parity===0,"integer and xor");
check((0.1+0.2)>0.3 && (0.1+0.2)<0.31,"IEEE-754");
check((-7%3)===-1 && (3<<5)===96,"signed math");
done("numeric",t);

/* 2: several independent closures, nested calls and recursion. */
t=g.ticks();
function counter(start){
    var n=start;
    return function(delta){n+=delta;return n;};
}
function factorial(n){return n<=1?1:n*factorial(n-1);}
var c1=counter(0),c2=counter(500);
for(i=0;i<750;i++){
    check(c1(1)===i+1,"closure c1");
    check(c2(-1)===499-i,"closure c2");
}
check(factorial(9)===362880,"recursion");
done("closures",t);

/* 3: object turnover, property hash chains and array retention across GC. */
t=g.ticks();
var slots=[],last;
for(i=0;i<1200;i++){
    var o={id:i,step:i+1,tag:"v"+i};
    o["k"+(i%7)]=i*3;
    if(i%3===0)delete o.step;
    slots[i%48]=o;
    check(o.id===i && o["k"+(i%7)]===i*3,"object roundtrip");
}
for(j=0;j<48;j++){
    last=slots[j];
    check(last.id===1152+j,"retained object");
    check(last.tag==="v"+last.id,"retained string");
}
var deleted={a:11,b:12};
delete deleted.a;
check(!("a" in deleted) && deleted.b===12,"delete");
done("objects and arrays",t);

/* 4: repeated string allocation, slicing and numeric coercion. */
t=g.ticks();
var word="",k;
for(i=0;i<320;i++)word+=String.fromCharCode(65+(i%26));
check(word.length===320 && word.slice(0,4)==="ABCD","string concat/slice");
for(i=0;i<600;i++){
    k="K"+i;
    check(k.charAt(0)==="K" && k.length>=2,"string charAt");
}
done("strings",t);

/* 5: JSON and regexp exercise if present in this minimal ROM. */
t=g.ticks();
if(typeof JSON!=="undefined" && typeof JSON.stringify==="function"){
    for(i=0;i<180;i++){
        var encoded=JSON.stringify({x:i,arr:[i,i+1],text:"k"+i});
        var decoded=JSON.parse(encoded);
        check(decoded.x===i && decoded.arr[1]===i+1,"JSON");
    }
    done("JSON",t);
}else print("SKIP JSON (not in ROM)");
t=g.ticks();
if(typeof RegExp==="function"){
    var re=/ab+c/;
    for(i=0;i<200;i++)check(re.test("a"+"bb"+"c"),"RegExp");
    done("regexp",t);
}else print("SKIP RegExp (not in ROM)");

/* 6: typed arrays where shipped in the trimmed stdlib. */
t=g.ticks();
if(typeof Uint8Array==="function"){
    var bytes=new Uint8Array(128);
    for(i=0;i<300;i++)bytes[i%128]=(i*3)&255;
    for(i=0;i<128;i++){
        var lastWrite=i+128*((299-i)/128|0);
        check(bytes[i]===((lastWrite*3)&255),"typed array");
    }
    done("typed array",t);
}else print("SKIP Uint8Array (not in ROM)");

/* 7: live closure refs survive pressure while temporary objects churn. */
t=g.ticks();
var saved=[];
for(i=0;i<1400;i++){
    var item={value:i,array:[i, i+1],name:"live"+i};
    if(i%50===0)saved[i/50]=item;
    var tmp={other:i^0x55,items:[i,i+2],nested:{n:i}};
    check(tmp.nested.n===i,"temporary nested");
}
for(i=0;i<saved.length;i++){
    check(saved[i].value===i*50,"saved object "+i);
    check(saved[i].array[1]===i*50+1,"saved array "+i);
}
done("GC retention",t);

/* 8: host objects interleaved with JS allocations (no touch required). */
t=g.ticks();
for(i=0;i<1500;i++){
    var ev=g.pollevent();
    check(typeof ev.type==="number" && typeof ev.key==="number","event");
    if((i%10)===0){
        var bounds=g.dwindow_get();
        check(bounds.length===4,"clip array");
        var heap={i:i,event:ev,bounds:bounds};
        check(heap.event.type===ev.type,"native root");
    }
}
done("native event/clip GC",t);
print("VMSTRESS OK "+phases+" phases "+elapsed(t0)+"ms");

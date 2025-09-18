#include <curl/system.h>
#include <luaconf.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>
#include <curl/options.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "xcurlconst.h"


/*
typedef enum{
    L_FUNC_REF_WRITE=0,
    L_FUNC_REF_READ,
    L_FUNC_REF_HEADER,
    L_FUNC_REF_DEBUG,
    L_FUNC_REF_SSL_CTX_,
    L_FUNC_REF_SOCKOPT,
    L_FUNC_REF_OPENSOCKET,
    L_FUNC_REF_SEEK,
    L_FUNC_REF_SSH_KEY,
    L_FUNC_REF_INTERLEAVE,
    L_FUNC_REF_CHUNK_BGN_,
    L_FUNC_REF_CHUNK_END_,
    L_FUNC_REF_FNMATCH_,
    L_FUNC_REF_CLOSESOCKET,
    L_FUNC_REF_XFERINFO,
    L_FUNC_REF_RESOLVER_START_,
    L_FUNC_REF_TRAILER,
    L_FUNC_REF_HSTSREAD,
    L_FUNC_REF_HSTSWRITE,
    L_FUNC_REF_PREREQ,
    L_FUNC_REF_SSH_HOSTKEY,
    L_FUNC_REF_LENGTH
}L_FUNC_REF;
*/


typedef enum{
    C_PTR_SLIST_HTTPHEADER=0,
    C_PTR_SLIST_QUOTE,
    C_PTR_SLIST_POSTQUOTE,
    C_PTR_SLIST_TELNETOPTIONS,
    C_PTR_SLIST_PREQUOTE,
    C_PTR_SLIST_HTTP200ALIASES,
    C_PTR_SLIST_MAIL_RCPT,
    C_PTR_SLIST_RESOLVE,
    C_PTR_SLIST_PROXYHEADER,
    C_PTR_SLIST_CONNECT_TO,
    C_PTR_SLIST_LENGTH
}C_PTR_SLIST;

typedef enum {
    IO_NONE,    /* not set / unspecified */
    IO_STREAM,  /* use callback */
    IO_BUFFER,  /* accumulate in memory */
    IO_FILE     /* write/read to FILE* */
} io_type_e;

typedef struct{
    char * ptr;
    size_t len;
    size_t cap;
    size_t max;
}buffer_t;

typedef struct{
    FILE * ptr;
    int path;
}file_t;

typedef struct {
    io_type_e type;
    union{
        buffer_t buffer;
        file_t file;
        int stream;
    };
} io_ctx_t;


typedef struct {
    lua_State * L;
    CURL * easy;
    char error_str[CURL_ERROR_SIZE];
    io_ctx_t output;
    io_ctx_t input;
    struct {
        int prereq;
        int xferinfo;
        int done;
    }callback;
    //int l_func_ref[L_FUNC_REF_LENGTH];
    int error;
    int rc;
    struct curl_slist * c_ptr_slist[C_PTR_SLIST_LENGTH];
    struct curl_mime * mimepost;
}leasy_t;






static void xcurl__push_error(lua_State * L,int error_no,int upval_idx){
    lua_rawgeti(L,lua_upvalueindex(upval_idx),error_no);
    if (lua_type(L,-1)!=LUA_TSTRING){
        lua_pop(L,1);
        lua_pushfstring(L,"ERROR %d",error_no);
    }
}


static void xcurl_easy_io_gc(lua_State * L, io_ctx_t * ctx){
    if (ctx->type==IO_FILE){
        if (ctx->file.ptr){
            fclose(ctx->file.ptr);// ignore errors
            ctx->file.ptr=NULL;
        }
        luaL_unref(L, LUA_REGISTRYINDEX, ctx->file.path);
        ctx->type=IO_NONE;
        return;
    }
    if (ctx->type==IO_STREAM){
        luaL_unref(L, LUA_REGISTRYINDEX, ctx->stream);
        ctx->type=IO_NONE;
        return;
    }
    if (ctx->type==IO_BUFFER){
        if (ctx->buffer.ptr){
            free(ctx->buffer.ptr);
        }
        ctx->type=IO_NONE;
        return;
    }
    // assert is IO_NONE
}




static int xcurl_easy_gc(lua_State * L){
    leasy_t * leasy=lua_touserdata(L,1);
    xcurl_easy_io_gc(L,&leasy->input);
    xcurl_easy_io_gc(L,&leasy->output);
#define REF_GC(L,var) do{\
    if (var!=LUA_NOREF){\
        luaL_unref(L,LUA_REGISTRYINDEX,var);\
        var=LUA_NOREF;\
    } \
} while(0)
    REF_GC(L, leasy->callback.done);
    REF_GC(L, leasy->callback.prereq);
    REF_GC(L, leasy->callback.xferinfo);
    REF_GC(L, leasy->error);
#undef REF_GC
    for (int i=0;i<C_PTR_SLIST_LENGTH;i++){
        if (leasy->c_ptr_slist[i]!=NULL){
            curl_slist_free_all(leasy->c_ptr_slist[i]);
            leasy->c_ptr_slist[i]=NULL;
        }
    }
    if (leasy->mimepost!=NULL){
        curl_mime_free(leasy->mimepost);
        leasy->mimepost=NULL;
    }
    curl_easy_cleanup(leasy->easy);
    leasy->easy=NULL;
    leasy->L=NULL;
    return 0;
}

/*
#define RAWGET_REF(A) \
    leasy_t * leasy=(leasy_t *)userdata;\
    lua_State * L=leasy->L;\
    lua_rawgeti(L,LUA_REGISTRYINDEX,leasy->l_func_ref[L_FUNC_REF_##A]);

    

#define CB_PCALL(nargs,er) \
    if (lua_pcall(L,nargs,1,0)!=LUA_OK){\
        if (leasy->error!=LUA_NOREF) \
            luaL_unref(L,LUA_REGISTRYINDEX,leasy->error);\
        leasy->error=luaL_ref(L,LUA_REGISTRYINDEX);\
        return er;\
    }
*/









#define KB(x)   ((size_t)(x) << 10)
#define MB(x)   ((size_t)(x) << 20)

// Fit to nearest power-of-two ≥ want, min 4K
static size_t round_pow2_fit(size_t want) {
    size_t cap = KB(4);
    while (cap < want) {
        cap <<= 1;
    }
    return cap;
}

#define MAX_CAP_MEM MB(64)

ssize_t xcurl__iobuffer_ensure(buffer_t *b, size_t needed) {
    size_t capacity=b->len + needed;
    if (capacity <= b->cap) return 0;  // Already enough space
    size_t new_cap = 0;
    if (b->cap == 0) {
        // First allocation — fit to pow2 like 4K, 8K, etc.
        new_cap = round_pow2_fit(capacity);
    } else {
        if (b->cap <= KB(256)) {
            new_cap = b->cap * 2;
        } else if (b->cap <= MB(2)) {
            new_cap = b->cap + b->cap / 2;
        } else {
            new_cap = b->cap + MB(4);
        }
        if (new_cap < capacity)
            new_cap = capacity;  // Always satisfy required size
    }
    size_t max=b->max;
    if (max>MAX_CAP_MEM){
        max=MAX_CAP_MEM;
    }
    if (new_cap > max)
        return -2;  // Memory limit reached

    size_t added = new_cap - b->cap;

    char *new_data = (char *)realloc(b->ptr, new_cap);
    if (!new_data) return -1;

    b->ptr = new_data;
    b->cap = new_cap;
    return (ssize_t)added;
}









static size_t cb_WRITEFUNCTION(char *bytes, size_t size, size_t nmemb, void *userdata){
    leasy_t * e=(leasy_t *)userdata;
    io_ctx_t * io = &e->output;
    size_t nbytes = size * nmemb;
    if (io->type==IO_BUFFER){
        int rc=xcurl__iobuffer_ensure(&io->buffer,nbytes);
        if (rc<0){
            if (e->error==LUA_NOREF){
                lua_pushfstring(e->L,rc==-1?"not enough memory!":"buffer error");
                e->error=luaL_ref(e->L, LUA_REGISTRYINDEX);
            }
            return CURL_WRITEFUNC_ERROR;
        }
        memcpy(io->buffer.ptr + io->buffer.len,bytes,nbytes);
        io->buffer.len+=nbytes;
        return nbytes;
    }
    if (io->type==IO_FILE){
        if (!io->file.ptr){
            lua_rawgeti(e->L, LUA_REGISTRYINDEX, io->file.path);// should always be available (the type is last thing get assigned after all fields are processed)
            io->file.ptr=fopen(lua_tostring(e->L, -1), "ab");
            if (!io->file.ptr){
                if (e->error==LUA_NOREF){
                    lua_pushliteral(e->L, "cannot open file : '");
                    lua_pushvalue(e->L, -2);
                    lua_pushliteral(e->L, "'");
                    lua_concat(e->L, 3);
                    e->error=luaL_ref(e->L, LUA_REGISTRYINDEX);
                }
                lua_pop(e->L, 1);
                return CURL_WRITEFUNC_ERROR;
            }
            lua_pop(e->L, 1);
        }
        return fwrite(bytes, size, nmemb, io->file.ptr);
    }
    if (io->type==IO_STREAM){
        lua_rawgeti(e->L, LUA_REGISTRYINDEX, io->stream);
        lua_pushlstring(e->L,bytes,nbytes);
        if (lua_pcall(e->L,1,1,0)){
            if (e->error==LUA_NOREF){
                e->error=luaL_ref(e->L, LUA_REGISTRYINDEX);
            }else{
                lua_pop(e->L, 1);
            }
            return CURL_WRITEFUNC_ERROR;
        }
        if (lua_type(e->L,-1)!=LUA_TNIL)
            nbytes=(size_t)lua_tonumber(e->L,-1);
        lua_pop(e->L, 1);
        return nbytes;
    }
    if (e->error==LUA_NOREF){
        lua_pushfstring(e->L, "response type not implemented");
        e->error=luaL_ref(e->L, LUA_REGISTRYINDEX);
    }
    return CURL_WRITEFUNC_ERROR;
}

/*
static size_t cb_READFUNCTION(char *buffer, size_t size, size_t nmemb, void *userdata)
{
    RAWGET_REF(READ)
    size_t bytes = size * nmemb;
    lua_pushinteger(L,bytes);
    CB_PCALL(1,CURL_READFUNC_ABORT)
    int type=lua_type(L,-1);
    if (type==LUA_TSTRING) {
        const char * res=lua_tolstring(L,-1, &bytes);
        memcpy(buffer, res, bytes);
    }else
        bytes=lua_tointeger(L,-1);
    lua_pop(L, 1);
    return bytes;
}

static int cb_SEEKFUNCTION(void *userdata, curl_off_t offset, int origin)
{
    RAWGET_REF(SEEK)
    lua_pushinteger(L,offset);
    if (SEEK_SET == origin) lua_pushliteral(L, "set");
    else if (SEEK_CUR == origin) lua_pushliteral(L, "cur");
    else if (SEEK_END == origin) lua_pushliteral(L, "end");
    else lua_pushinteger(L, origin);
    CB_PCALL(2,CURL_SEEKFUNC_FAIL)
    int ret=CURL_SEEKFUNC_OK;
    if (lua_type(L,-1)!=LUA_TNIL)
        ret=lua_tointeger(L,-1);
    lua_pop(L, 1);
    return ret;
}
static size_t cb_HEADERFUNCTION(char *buffer, size_t size,
                              size_t nitems, void *userdata)
{
    RAWGET_REF(HEADER)
    size_t bytes = size * nitems;
    lua_pushlstring(L,buffer,bytes);
    CB_PCALL(1,CURL_WRITEFUNC_ERROR)
    if (lua_type(L,-1)!=LUA_TNIL)
        bytes=lua_tointeger(L,-1);
    lua_pop(L, 1);
    return bytes;
}
*/
static size_t cb_XFERINFOFUNCTION(void *userdata,
                                curl_off_t dltotal,
                                curl_off_t dlnow,
                                curl_off_t ultotal,
                                curl_off_t ulnow)
{
    leasy_t * e=(leasy_t *)userdata;
    lua_rawgeti(e->L, LUA_REGISTRYINDEX, e->callback.xferinfo);
    lua_pushinteger(e->L,dltotal);
    lua_pushinteger(e->L,dlnow);
    lua_pushinteger(e->L,ultotal);
    lua_pushinteger(e->L,ulnow);
    if (lua_pcall(e->L, 4, 1, 0)){
        if (e->error==LUA_NOREF){
            e->error=luaL_ref(e->L, LUA_REGISTRYINDEX);
        }else{
            lua_pop(e->L, 1);
        }
        return 1;
    }
    int ret=0;
    if (lua_type(e->L,-1)!=LUA_TNIL)
        ret=lua_tointeger(e->L,-1);
    lua_pop(e->L, 1);
    return ret;
}

#define INVALID_VAL luaL_error(L,"invalid value");

static int xcurl__easy_newindex_io(lua_State * L,leasy_t * leasy,int type,int is_input){
    io_ctx_t *ctx;
    if (is_input)
        return luaL_error(L, "not implemeted yet");
    if (is_input)
        ctx=&leasy->input;
    else
        ctx=&leasy->output;

    xcurl_easy_io_gc(L,ctx);
    
    if (type==LUA_TNIL){
        if (is_input){
            // maybe we need to set upload to 0 ?
        }
        return 0;
    } 
    if (!is_input && type==LUA_TNUMBER){
        lua_Integer n = lua_tointeger(L, 3);
        if (n<=0) return INVALID_VAL;
        ctx->type=IO_BUFFER;
        ctx->buffer.ptr=NULL;
        ctx->buffer.cap=0;
        ctx->buffer.len=0;
        ctx->buffer.max=n;
        return 0;
    }
    if (type==LUA_TSTRING){
        lua_pushvalue(L, 3);
        ctx->type=IO_FILE;
        ctx->file.ptr=NULL;
        ctx->file.path=luaL_ref(L, LUA_REGISTRYINDEX);
        return 0;
    }
    if (type==LUA_TFUNCTION){
        lua_pushvalue(L, 3);
        ctx->type=IO_STREAM;
        ctx->stream=luaL_ref(L, LUA_REGISTRYINDEX);
        return 0;
    }
    return INVALID_VAL;
};

static int xcurl_easy_newindex(lua_State * L){
    leasy_t * leasy=lua_touserdata(L,1);
    int type=lua_type(L,2);
    const struct curl_easyoption * curl_opt;
    if (type==LUA_TSTRING){
        const char * optstr=lua_tostring(L,2);
        if (strcmp(optstr, "output")==0){
            return xcurl__easy_newindex_io(L,leasy, lua_type(L, 3), 0);
        }
        if (strcmp(optstr, "input")==0){
            return xcurl__easy_newindex_io(L,leasy, lua_type(L, 3), 1);
        }
        if (strcmp(optstr, "on_xferinfo")==0){
            if (leasy->callback.xferinfo!=LUA_NOREF){
                luaL_unref(L, LUA_REGISTRYINDEX, leasy->callback.xferinfo);
                curl_easy_setopt(leasy->easy, CURLOPT_NOPROGRESS, 1);
            }
            if (lua_type(L, 3)==LUA_TNIL){
                return 0;
            }
            luaL_checktype(L, 3, LUA_TFUNCTION);
            lua_pushvalue(L, 3);
            leasy->callback.xferinfo=luaL_ref(L, LUA_REGISTRYINDEX);
            curl_easy_setopt(leasy->easy, CURLOPT_NOPROGRESS, 0);
            return 0;
        }
        curl_opt=curl_easy_option_by_name(lua_tostring(L,2));
    }else if (type==LUA_TNUMBER)
        curl_opt=curl_easy_option_by_id(lua_tointeger(L,2));
    else
        curl_opt=NULL;
    if (!curl_opt) return luaL_error(L,"invalid key (unavailable)");
    int ret;
    type=lua_type(L, 3);
    switch (curl_opt->type) {
    case CURLOT_VALUES:
    case CURLOT_LONG:{
        if (curl_opt->id==CURLOPT_NOPROGRESS)
            return luaL_error(L, "invalid key (reserved)"); 
        luaL_checktype(L, 3, LUA_TNUMBER);
        ret=curl_easy_setopt(leasy->easy, curl_opt->id, lua_tointeger(L, 3));
        break;
    }
    case CURLOT_OFF_T:{
        luaL_checktype(L, 3, LUA_TNUMBER);
        ret=curl_easy_setopt(leasy->easy, curl_opt->id, (curl_off_t)lua_tonumber(L, 3));
        break;
    }
    case CURLOT_STRING:{
        const char * value;
        if (type==LUA_TNIL) value=NULL;
        else {
            luaL_checktype(L, 3, LUA_TSTRING);
            value=lua_tostring(L, 3);            
        }
        ret=curl_easy_setopt(leasy->easy, curl_opt->id, value);
        break;
    }
    case CURLOT_BLOB:{
        if (type==LUA_TNIL) ret=curl_easy_setopt(leasy->easy, curl_opt->id, NULL);
        else {
            luaL_checktype(L, 3, LUA_TSTRING);
            struct curl_blob blob;
            blob.data=(void *)lua_tolstring(L,3,&blob.len);
            blob.flags=CURL_BLOB_COPY;
            ret=curl_easy_setopt(leasy->easy, curl_opt->id, blob);
        }
        break;
    }
    case CURLOT_SLIST:{
        int pos;
        switch (curl_opt->id) {
            #define CASE_SLIST(A) case CURLOPT_##A : pos=C_PTR_SLIST_##A;break;
            CASE_SLIST(HTTPHEADER)
            CASE_SLIST(QUOTE)
            CASE_SLIST(POSTQUOTE)
            CASE_SLIST(TELNETOPTIONS)
            CASE_SLIST(PREQUOTE)
            CASE_SLIST(HTTP200ALIASES)
            CASE_SLIST(MAIL_RCPT)
            CASE_SLIST(RESOLVE)
            CASE_SLIST(PROXYHEADER)
            CASE_SLIST(CONNECT_TO)
            #undef CASE_SLIST
            default:return luaL_error(L,"invalid key (not implemented)");
        }
        if (type==LUA_TNIL){
            ret=curl_easy_setopt(leasy->easy,curl_opt->id,NULL);
            if (ret==CURLE_OK){
                if (leasy->c_ptr_slist[pos]!=NULL){
                    curl_slist_free_all(leasy->c_ptr_slist[pos]);
                    leasy->c_ptr_slist[pos]=NULL;
                }
            }
            break;
        }
        luaL_checktype(L, 3, LUA_TTABLE);
        struct curl_slist *slist=NULL,*tmp;
        int len=lua_objlen(L,3);
        if (len>0){
            for(int i=1;i<=len;i++){
                lua_rawgeti(L, 3, i);
                if (lua_type(L, -1)!=LUA_TSTRING){
                    lua_pop(L, 1);
                    if (slist) curl_slist_free_all(slist);
                    return INVALID_VAL
                }
                tmp=curl_slist_append(slist,lua_tostring(L, -1));
                if (!tmp){
                    if (slist) curl_slist_free_all(slist);
                    return luaL_error(L,"not enough memory!");
                }
                slist=tmp;
                lua_pop(L, 1);
            }
        }
        ret=curl_easy_setopt(leasy->easy,curl_opt->id,slist);
        if (ret==CURLE_OK){
            if (leasy->c_ptr_slist[pos]!=NULL)
                curl_slist_free_all(leasy->c_ptr_slist[pos]);
            leasy->c_ptr_slist[pos]=slist;
        }else if (slist!=NULL)
            curl_slist_free_all(slist);
        break;
    }
    case CURLOT_FUNCTION:
    {
        int * l_func_ref;
        void * c_ptr_cb;
        int ud_id;
        int * l_second_func_ref=NULL;// cb that share same ud
        switch (curl_opt->id) {
            #define CASE_CB_NB(A) \
                case CURLOPT_##A##FUNCTION : \
                    l_func_ref=&leasy->l_func_ref[L_FUNC_REF_##A];\
                    c_ptr_cb=cb_##A##FUNCTION;\
                    ud_id=CURLOPT_##A##DATA;
            #define CASE_CB(A) CASE_CB_NB(A) break;  
            //CASE_CB(WRITE)
            //CASE_CB(READ)
            //CASE_CB(HEADER)
            //CASE_CB(DEBUG)
            //CASE_CB(SSL_CTX_)
            //CASE_CB(SOCKOPT)
            //CASE_CB(OPENSOCKET)
            //CASE_CB(SEEK)
            //CASE_CB(SSH_KEY)
            //CASE_CB(INTERLEAVE)
            //CASE_CB_NB(CHUNK_BGN_) l_second_func_ref = &leasy->l_func_ref[L_FUNC_REF_CHUNK_END_];break;
            //CASE_CB_NB(CHUNK_END_) l_second_func_ref = &leasy->l_func_ref[L_FUNC_REF_CHUNK_BGN_];break;
            //CASE_CB(FNMATCH_)
            //CASE_CB(CLOSESOCKET)
            //CASE_CB(XFERINFO)
            //CASE_CB(RESOLVER_START_)
            //CASE_CB(TRAILER)
            //CASE_CB(HSTSREAD)
            //CASE_CB(HSTSWRITE)
            //CASE_CB(PREREQ)
            //CASE_CB(SSH_HOSTKEY)
            #undef CASE_CB
            #undef CASE_CB_NB
            default : return luaL_error(L,"invalid key (not implemented)");
        }
        if (type==LUA_TNIL){
            ret=curl_easy_setopt(leasy->easy,curl_opt->id,NULL);
            if (ret==CURLE_OK){
                if (*l_func_ref!=LUA_NOREF){
                    luaL_unref(L, LUA_REGISTRYINDEX,*l_func_ref);
                    *l_func_ref=LUA_NOREF;
                }
                if (l_second_func_ref==NULL || *l_second_func_ref==LUA_NOREF){
                    ret=curl_easy_setopt(leasy->easy,ud_id,NULL);
                }
            }
            break;
        }
        luaL_checktype(L, 3, LUA_TFUNCTION);

        ret=curl_easy_setopt(leasy->easy,curl_opt->id,c_ptr_cb);
        if (ret==CURLE_OK){
            if (*l_func_ref!=LUA_NOREF)
                luaL_unref(L, LUA_REGISTRYINDEX,*l_func_ref);
            else if (l_second_func_ref==NULL || *l_second_func_ref==LUA_NOREF)
                ret=curl_easy_setopt(leasy->easy,ud_id,leasy);
            *l_func_ref=luaL_ref(L,LUA_REGISTRYINDEX);
        }
        break;
  
    }
    case CURLOT_OBJECT:
        if (curl_opt->id==CURLOPT_MIMEPOST){
            if (type==LUA_TNIL){
                ret=curl_easy_setopt(leasy->easy,curl_opt->id,NULL);
                if (ret==CURLE_OK && leasy->mimepost!=NULL){
                    curl_mime_free(leasy->mimepost);
                    leasy->mimepost=NULL;
                }
                break;
            }
            luaL_checktype(L, 3, LUA_TTABLE);
            curl_mime *mime;
            curl_mimepart *part;
            mime=curl_mime_init(leasy->easy);
            if (!mime) return luaL_error(L,"NULL returned");
            int len=lua_objlen(L,3);
            if (len>0){
                for (int i=1;i<=len;i++){
                    lua_rawgeti(L, 3, i);
                    type=lua_type(L, -1);
                    if (type!=LUA_TTABLE){
                        lua_pop(L, 1);
                        curl_mime_free(mime);
                        return INVALID_VAL
                    }
                    part = curl_mime_addpart(mime);
                    lua_pushnil(L);
                    while (lua_next(L, -2)){
                        type=lua_type(L, -2);
                        if (type!=LUA_TSTRING){
                            lua_pop(L, 3);
                            curl_mime_free(mime);
                            return INVALID_VAL
                        }
                        type=lua_type(L,-1);
                        const char * key=lua_tostring(L, -2);
                        if (type==LUA_TSTRING){
                            if (strcmp(key,"name")==0)
                                curl_mime_name(part,lua_tostring(L, -1));
                            else if (strcmp(key,"type")==0)
                                curl_mime_type(part,lua_tostring(L, -1));
                            else if (strcmp(key,"filename")==0)
                                curl_mime_filename(part,lua_tostring(L, -1));
                            else if (strcmp(key,"encoder")==0)
                                curl_mime_encoder(part,lua_tostring(L, -1));
                            else if (strcmp(key,"data")==0){
                                size_t sz;
                                const char * value=lua_tolstring(L, -1,&sz);
                                curl_mime_data(part,value,sz);
                            }
                            else{
                                lua_pop(L, 3);
                                curl_mime_free(mime);
                                return INVALID_VAL
                            }
                        }/*else if (type==LUA_TTABLE && strcmp(key,"headers")==0){

                        }*/
                        else{
                            lua_pop(L, 3);
                            curl_mime_free(mime);
                            return INVALID_VAL
                        }
                        
                        lua_pop(L, 1);
                    }
                }
            }
            ret=curl_easy_setopt(leasy->easy, curl_opt->id,mime);
            if (ret==CURLE_OK){
                if (leasy->mimepost!=NULL)
                    curl_mime_free(leasy->mimepost);
                leasy->mimepost=mime;
            }else  
                curl_mime_free(mime);
            break;
        }
        return luaL_error(L, "invalid key (not implemented)"); 
    case CURLOT_CBPTR:return luaL_error(L, "invalid key (reserved)"); 
    default: return luaL_error(L,"invalid key (unsupported)");
    }
    if (ret!=CURLE_OK)
        return luaL_error(L,curl_easy_strerror(ret));
    return 0;
}






static int xcurl_easy_index(lua_State * L){
    leasy_t * leasy=lua_touserdata(L,1);
    const char * key=luaL_checkstring(L,2);
    lua_getfield(L,lua_upvalueindex(1),key);
    if (lua_type(L,-1)!=LUA_TNUMBER) {
        if (strcmp(key,"response")==0){
            if (leasy->output.type==IO_BUFFER){
                if (leasy->output.buffer.ptr && leasy->output.buffer.len>0){
                    lua_pushlstring(L, leasy->output.buffer.ptr, leasy->output.buffer.len);
                }else{
                    lua_pushliteral(L, "");
                }
                return 1;
            }
            return 0;
        }
        if (strcmp(key,"iheaders")==0){
            // headers
            lua_newtable(L);
            struct curl_header *prev = NULL;
            struct curl_header *h;
            unsigned int origin = CURLH_HEADER| CURLH_1XX | CURLH_TRAILER | CURLH_CONNECT|CURLH_CONNECT;
            int i=0;
            while((h = curl_easy_nextheader(leasy->easy, origin, -1, prev))) {
                lua_pushstring(L,h->name);
                lua_pushliteral(L, ": ");
                lua_pushstring(L,h->value);
                lua_concat(L, 3);
                lua_rawseti(L,-2,++i);
                prev = h;
            }
            return 1;
        }
        if (strcmp(key,"headers")==0){
            // headers
            lua_newtable(L);
            struct curl_header *prev = NULL;
            struct curl_header *h;
            unsigned int origin = CURLH_HEADER| CURLH_1XX | CURLH_TRAILER | CURLH_CONNECT|CURLH_CONNECT;
            while((h = curl_easy_nextheader(leasy->easy, origin, -1, prev))) {
                lua_pushstring(L,h->name);
                lua_pushstring(L,h->value);
                lua_rawset(L,-3);
                prev = h;
            }
            return 1;
        }
        if (strcmp(key, "error")==0){
            lua_createtable(L, 0, 2);
            lua_pushliteral(L, "name");
            xcurl__push_error(L, leasy->rc, 2);
            lua_rawset(L,-3);
            lua_pushliteral(L, "message");
            if (leasy->error!=LUA_NOREF){
                lua_rawgeti(L, LUA_REGISTRYINDEX, leasy->error);
            }else {
                size_t len = strlen(leasy->error_str);
                if (len && leasy->error_str[len - 1]=='\n') {
                    leasy->error_str[len - 1]='\0';
                    len--;
                }
                lua_pushlstring(L, leasy->error_str, len);
            }
            lua_rawset(L,-3);
            return 1;
        }
        return luaL_error(L,"invalid key (unavailable)");
    }
    int ikey=lua_tointeger(L,-1);lua_pop(L, 1);
    int ikey_type = CURLINFO_TYPEMASK & ikey;
    switch (ikey_type) {
        case CURLINFO_STRING :{
            const char * out=NULL;
            curl_easy_getinfo(leasy->easy,ikey,&out);
            if (out==NULL) lua_pushnil(L);
            else lua_pushstring(L,out);
            return 1;
        }
        case CURLINFO_LONG:{
            long out=0;
            curl_easy_getinfo(leasy->easy,ikey,&out);
            lua_pushinteger(L,out);
            return 1;
        }
        case CURLINFO_OFF_T:{
            curl_off_t out=0;
            curl_easy_getinfo(leasy->easy,ikey,&out);
            lua_pushnumber(L,out);
            return 1;
        }
        case CURLINFO_DOUBLE:{
            double out=0.0;
            curl_easy_getinfo(leasy->easy,ikey,&out);
            lua_pushnumber(L,out);
            return 1;
        }
        case CURLINFO_SLIST:{
            struct curl_slist *out;
            curl_easy_getinfo(leasy->easy,ikey,&out);
            lua_newtable(L);
            int i=0;
            while (out) {
                lua_pushstring(L,out->data);
                lua_rawseti(L,-2,++i);
                out = out->next;
            }
            curl_slist_free_all(out);
            return 1;
        }
        default: return luaL_error(L,"invalid key (not implemented)");
    }
}

static void xcurl__easy_prepare(lua_State * L,leasy_t * leasy){
    leasy->rc=0;
    leasy->error_str[0]='\0';
    if (leasy->error!=LUA_NOREF){
        luaL_unref(leasy->L,LUA_REGISTRYINDEX,leasy->error);
        leasy->error=LUA_NOREF;
    }
    if (leasy->output.type==IO_BUFFER){
        leasy->output.buffer.len=0;
    }else if (leasy->output.type==IO_FILE){
        if (leasy->output.file.ptr){
            fclose(leasy->output.file.ptr);
            leasy->output.file.ptr=NULL;
        }
    }
    if (leasy->callback.done!=LUA_NOREF){
        luaL_unref(leasy->L,LUA_REGISTRYINDEX,leasy->callback.done);
        leasy->callback.done=LUA_NOREF;
    }
}

static void xcurl__easy_flush_output(leasy_t * e){
    if (e->output.type==IO_FILE){
        if (e->output.file.ptr){
            fflush(e->output.file.ptr);
            fclose(e->output.file.ptr);
            e->output.file.ptr=NULL;
        }
    }
}

static int xcurl_easy_call(lua_State * L){
    leasy_t * leasy=lua_touserdata(L,1);
    xcurl__easy_prepare(L, leasy);
    int rc=curl_easy_perform(leasy->easy);
    xcurl__easy_flush_output(leasy);
    lua_pushboolean(L,rc==CURLE_OK);
    return 1;
}



static int xcurl_easy_new(lua_State * L){
    CURL * easy=curl_easy_init();
    if (!easy) return luaL_error(L,"not enough memory!");
    leasy_t * e=lua_newuserdata(L,sizeof(leasy_t));
    if (!e){
        curl_easy_cleanup(easy);
        return luaL_error(L,"not enough memory!");
    }
    e->rc=0;
    e->output.type=IO_NONE;
    e->input.type=IO_NONE;
    e->L=L;
    e->callback.done=LUA_NOREF;
    e->callback.prereq=LUA_NOREF;
    e->callback.xferinfo=LUA_NOREF;
    e->error=LUA_NOREF;

    curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, e->error_str);
    curl_easy_setopt(easy, CURLOPT_PRIVATE,NULL);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION,cb_WRITEFUNCTION);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA,e);
    curl_easy_setopt(easy, CURLOPT_XFERINFOFUNCTION,cb_XFERINFOFUNCTION);
    curl_easy_setopt(easy, CURLOPT_XFERINFODATA,e);
    curl_easy_setopt(easy, CURLOPT_NOPROGRESS,1);
    

    /*for (int i=0;i<L_FUNC_REF_LENGTH;i++)
        leasy->l_func_ref[i]=LUA_NOREF;*/
    for (int i=0;i<C_PTR_SLIST_LENGTH;i++)
        e->c_ptr_slist[i]=NULL;
    e->easy=easy;
    e->mimepost=NULL;
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setmetatable(L,-2);
    return 1;
}




/*static int xcurl_share_gc(lua_State * L){
    CURLSH ** pshare=lua_touserdata(L,1);
    if (*pshare){
        curl_share_cleanup(*pshare);
        *pshare=NULL;
    }
    return 0;
}*/





typedef struct{
    lua_State * L;
    CURLM * multi;
}lmulti_t;





static void xcurl__multi_unrefp(lua_State * L,CURL * e,int get){
    int * refp=NULL;
    curl_easy_getinfo(e,CURLINFO_PRIVATE,&refp);
    if (refp!=NULL){
        if (get){
            lua_rawgeti(L, LUA_REGISTRYINDEX, *refp);
        }
        luaL_unref(L, LUA_REGISTRYINDEX,*refp);
        free(refp);
        curl_easy_setopt(e,CURLOPT_PRIVATE,NULL);
    }
}

static void xcurl__multi_refp(lua_State * L,CURL * e,int idx){
    xcurl__multi_unrefp(L,e,0);
    int * refp=malloc(sizeof(int));
    if (refp){
        lua_pushvalue(L,idx);
        *refp=luaL_ref(L,LUA_REGISTRYINDEX);
        if (curl_easy_setopt(e,CURLOPT_PRIVATE,refp)==CURLE_OK)
            return;
        luaL_unref(L, LUA_REGISTRYINDEX,*refp);
        free(refp);
    }
    luaL_error(L,"failed to ref handle");
}


static int xcurl_multi_gc(lua_State *L){
    lmulti_t * lmulti=lua_touserdata(L,1);
    CURL **list = curl_multi_get_handles(lmulti->multi);
    if(list) {
      int i;
      /* remove all added handles */
      for(i = 0; list[i]; i++) {
        CURL * easy=list[i];
        curl_multi_remove_handle(lmulti->multi, easy);
        xcurl__multi_unrefp(L,easy,0);
      }
      curl_free(list);
    }
    curl_multi_cleanup(lmulti->multi);
    lmulti->multi=NULL;
    lmulti->L=NULL;
    return 0;
}



int xcurl_multi_new(lua_State * L){
    CURLM * multi=curl_multi_init();
    if (!multi) return luaL_error(L,"not enough memory!");
    lmulti_t * lmulti=lua_newuserdata(L,sizeof(lmulti_t));
    if (!lmulti){
        curl_multi_cleanup(multi);
        return luaL_error(L,"not enough memory!"); 
    }
    curl_multi_setopt(multi,CURLMOPT_MAX_HOST_CONNECTIONS,7);
    //lua_newtable(L);
    //lmulti->ref_idx_easy_list=luaL_ref(L,LUA_REGISTRYINDEX);
    lmulti->multi=multi;
    lmulti->L=L;
    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);
    return 1;
}

int xcurl_multi_add(lua_State * L){
    lmulti_t * lmulti=lua_touserdata(L,1);
    luaL_checktype(L,2, LUA_TUSERDATA);
    luaL_checktype(L,3,LUA_TFUNCTION);
    leasy_t * leasy=lua_touserdata(L,2);
    int rc;
    rc=curl_multi_add_handle(lmulti->multi, leasy->easy);
    if (rc!=CURLM_OK){
        xcurl__push_error(L,rc,1);
        return lua_error(L);
    }
    xcurl__multi_refp(L,leasy->easy,2);
    xcurl__easy_prepare(L,leasy);
    leasy->callback.done=luaL_ref(L,LUA_REGISTRYINDEX);
    return 0;
}

int xcurl_multi_remove(lua_State * L){
    lmulti_t * lmulti=lua_touserdata(L,1);
    int rc;
    luaL_checktype(L,2, LUA_TUSERDATA);
    leasy_t * leasy=lua_touserdata(L,2);
    rc=curl_multi_remove_handle(lmulti->multi,leasy->easy);
    if (rc!=CURLM_OK){
        xcurl__push_error(L,rc,1);
        return lua_error(L); 
    }
    xcurl__multi_unrefp(L, leasy->easy,0);
    if (leasy->callback.done!=LUA_NOREF){
        luaL_unref(leasy->L,LUA_REGISTRYINDEX,leasy->callback.done);
        leasy->callback.done=LUA_NOREF;
    }
    return 0;
}
int xcurl_multi_perform(lua_State * L){
    lmulti_t * lmulti=lua_touserdata(L,1);
    int still_running=1;
    int rc,msgq;
    struct CURLMsg *msg;
    CURL *e ;


again:
    rc=curl_multi_perform(lmulti->multi, &still_running);
    if (rc!=CURLM_OK){
        xcurl__push_error(L,rc,1);
        return lua_error(L);
    }
mmsg:
    msg = curl_multi_info_read(lmulti->multi, &msgq);
    if(msg) { //&& (msg->msg == CURLMSG_DONE    // just 'done' expected
        e = msg->easy_handle;
        rc=curl_multi_remove_handle(lmulti->multi, e);
        if (rc!=CURLM_OK){
            xcurl__push_error(L,rc,1);
            return lua_error(L);
        }
        xcurl__multi_unrefp(L,e,1);
        leasy_t * leasy= lua_touserdata(L,-1);
        lua_pop(L, 1);
        lua_rawgeti(L,LUA_REGISTRYINDEX,leasy->callback.done);
        luaL_unref(L,LUA_REGISTRYINDEX, leasy->callback.done);
        leasy->callback.done=LUA_NOREF;
        xcurl__easy_flush_output(leasy);
        leasy->rc=msg->data.result;
        lua_pushboolean(L,msg->data.result==CURLE_OK);
        if (lua_pcall(L,1,0,0)!=LUA_OK){
            return lua_error(L);
        }
        if (msgq) goto mmsg;
        if (still_running){
            curl_multi_poll(lmulti->multi, NULL, 0, 1000, NULL);
        }
        goto again;// may be a new handles added
    }
    if (still_running){
        curl_multi_poll(lmulti->multi, NULL, 0, 1000, NULL);
    }
    lua_pushinteger(L, still_running);
    return 1;        
}


#define MIV(A) lua_pushinteger(L,CURLM_##A);lua_pushstring(L,#A);lua_rawset(L,-3);
static void xcurl__multi_errors_consts(lua_State * L){
    lua_newtable(L);
    MIV(OK)
    MIV(BAD_HANDLE)
    MIV(BAD_EASY_HANDLE)
    MIV(OUT_OF_MEMORY)
    MIV(INTERNAL_ERROR)
    MIV(BAD_SOCKET)
    MIV(UNKNOWN_OPTION)
    MIV(ADDED_ALREADY)
    MIV(RECURSIVE_API_CALL)
    MIV(WAKEUP_FAILURE)
    MIV(BAD_FUNCTION_ARGUMENT)
    MIV(ABORTED_BY_CALLBACK)
    MIV(UNRECOVERABLE_POLL)
}
#undef MIV


static int xcurl_version(lua_State * L){
    lua_pushstring(L, curl_version());
    return 1;
}




int luaopen_xcurl(lua_State * L){
    xcurl__easy_errors_consts(L);
    int e_err_const=lua_gettop(L);
    xcurl__easy_info_consts(L);
    int e_info_const=lua_gettop(L);
    xcurl__multi_errors_consts(L);
    int m_err_const=lua_gettop(L);

    // xcurl
    lua_newtable(L);
    lua_pushcfunction(L, xcurl_version);
    lua_setfield(L, -2, "version");

    // easy_mt
    lua_newtable(L);
    //int easy_mt=lua_gettop(L);
    lua_pushvalue(L, e_info_const);
    lua_pushvalue(L, e_err_const);
    lua_pushcclosure(L, xcurl_easy_index,2);
    lua_setfield(L, -2, "__index"); 
    lua_pushcfunction(L, xcurl_easy_gc);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, xcurl_easy_newindex);
    lua_setfield(L,-2, "__newindex");
    lua_pushcfunction(L, xcurl_easy_call);// upval 1 : info consts upval 2: err const
    lua_setfield(L,-2, "__call");
    
    lua_pushcclosure(L, xcurl_easy_new,1);// upval 1 : easy_mt
    lua_setfield(L,-2,"easy");

    // multi mt
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, xcurl_multi_gc);
    lua_setfield(L,-2,"__gc");
    lua_pushvalue(L, m_err_const);
    lua_pushcclosure(L,xcurl_multi_add,1);
    lua_setfield(L, -2, "add");
    lua_pushvalue(L, m_err_const);
    lua_pushcclosure(L,xcurl_multi_remove,1);
    lua_setfield(L, -2, "remove");
    lua_pushvalue(L,m_err_const);
    lua_pushcclosure(L,xcurl_multi_perform,1);
    lua_setfield(L, -2, "perform");

    lua_pushcclosure(L, xcurl_multi_new,1);// upval 1 : multi_mt
    lua_setfield(L,-2,"multi");
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return 1;
}



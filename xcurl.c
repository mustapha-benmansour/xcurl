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

typedef struct {
    lua_State * L;
    CURL * easy;
    char error[CURL_ERROR_SIZE];
    int l_func_ref[L_FUNC_REF_LENGTH];
    int ref_idx_str_err;
    int ref_idx_cb_on_done;
    struct curl_slist * c_ptr_slist[C_PTR_SLIST_LENGTH];
    struct curl_mime * mimepost;
}leasy_t;




static void _lcurl_easy_err(lua_State * L,leasy_t * leasy,int err){
    if (leasy->ref_idx_str_err!=LUA_NOREF){
        lua_rawgeti(L, LUA_REGISTRYINDEX, leasy->ref_idx_str_err);
        luaL_unref(L, LUA_REGISTRYINDEX, leasy->ref_idx_str_err);
        leasy->ref_idx_str_err=LUA_NOREF;
    }else{
        size_t len = strlen(leasy->error);
        if (len && leasy->error[len - 1]=='\n') {
            leasy->error[len - 1]='\0';
            len--;
        }
        if (len) lua_pushstring(L,leasy->error);
        else lua_pushstring(L,curl_easy_strerror(err));
    }
}

static void _lcurl_push_error(lua_State * L,int error_no,int upval_idx){
    lua_rawgeti(L,lua_upvalueindex(upval_idx),error_no);
    if (lua_type(L,-1)!=LUA_TSTRING){
        lua_pop(L,1);
        lua_pushfstring(L,"ERROR %d",error_no);
    }
}




static int lcurl_easy_gc(lua_State * L){
    leasy_t * leasy=lua_touserdata(L,1);
    //int * refp;
    //curl_easy_getinfo(leasy->easy, CURLINFO_PRIVATE,&refp);
    for (int i=0;i<L_FUNC_REF_LENGTH;i++){
        if (leasy->l_func_ref[i]!=LUA_NOREF){
            luaL_unref(L, LUA_REGISTRYINDEX, leasy->l_func_ref[i]);
            leasy->l_func_ref[i]=LUA_NOREF;
        }
    }
    if (leasy->ref_idx_str_err!=LUA_NOREF){
        luaL_unref(L,LUA_REGISTRYINDEX,leasy->ref_idx_str_err);
        leasy->ref_idx_str_err=LUA_NOREF;
    }
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


#define RAWGET_REF(A) \
    leasy_t * leasy=(leasy_t *)userdata;\
    lua_State * L=leasy->L;\
    lua_rawgeti(L,LUA_REGISTRYINDEX,leasy->l_func_ref[L_FUNC_REF_##A]);

    

#define CB_PCALL(nargs,er) \
    if (lua_pcall(L,nargs,1,0)!=LUA_OK){\
        if (leasy->ref_idx_str_err!=LUA_NOREF) \
            luaL_unref(L,LUA_REGISTRYINDEX,leasy->ref_idx_str_err);\
        leasy->ref_idx_str_err=luaL_ref(L,LUA_REGISTRYINDEX);\
        return er;\
    }

static size_t cb_WRITEFUNCTION(char *buffer, size_t size, size_t nmemb, void *userdata){
    RAWGET_REF(WRITE)
    size_t bytes = size * nmemb;
    lua_pushlstring(L,buffer,bytes);
    CB_PCALL(1,CURL_WRITEFUNC_ERROR)
    if (lua_type(L,-1)!=LUA_TNIL)
        bytes=lua_tointeger(L,-1);
    lua_pop(L, 1);
    return bytes;
}
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
static size_t cb_XFERINFOFUNCTION(void *userdata,
                                curl_off_t dltotal,
                                curl_off_t dlnow,
                                curl_off_t ultotal,
                                curl_off_t ulnow)
{
    RAWGET_REF(XFERINFO)
    lua_pushinteger(L,dltotal);
    lua_pushinteger(L,dlnow);
    lua_pushinteger(L,ultotal);
    lua_pushinteger(L,ulnow);
    CB_PCALL(4,1)
    int ret=0;
    if (lua_type(L,-1)!=LUA_TNIL)
        ret=lua_tointeger(L,-1);
    lua_pop(L, 1);
    return ret;
}



#define INVALID_VAL luaL_error(L,"invalid value");
static int lcurl_easy_newindex(lua_State * L){
    leasy_t * leasy=lua_touserdata(L,1);
    int type=lua_type(L,2);
    const struct curl_easyoption * curl_opt;
    if (type==LUA_TSTRING)
        curl_opt=curl_easy_option_by_name(lua_tostring(L,2));
    else if (type==LUA_TNUMBER)
        curl_opt=curl_easy_option_by_id(lua_tointeger(L,2));
    else
        curl_opt=NULL;
    if (!curl_opt) return luaL_error(L,"invalid key (unavailable)");
    type=lua_type(L, 3);
    int ret;
    switch (curl_opt->type) {
    case CURLOT_LONG:{
        luaL_checktype(L, 3, LUA_TNUMBER);
        ret=curl_easy_setopt(leasy->easy, curl_opt->id, lua_tointeger(L, 3));
        break;
    }
    case CURLOT_VALUES:
    case CURLOT_OFF_T:{
        luaL_checktype(L, 3, LUA_TNUMBER);
        ret=curl_easy_setopt(leasy->easy, curl_opt->id, lua_tointeger(L, 3));
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
        struct curl_slist *slist=NULL;
        int len=lua_objlen(L,3);
        if (len>0){
            for(int i=1;i<=len;i++){
                lua_rawgeti(L, 3, i);
                if (lua_type(L, -1)!=LUA_TSTRING){
                    lua_pop(L, 1);
                    if (slist!=NULL) curl_slist_free_all(slist);
                    return INVALID_VAL
                }
                slist=curl_slist_append(slist,lua_tostring(L, -1));
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
            CASE_CB(WRITE)
            CASE_CB(READ)
            CASE_CB(HEADER)
            //CASE_CB(DEBUG)
            //CASE_CB(SSL_CTX_)
            //CASE_CB(SOCKOPT)
            //CASE_CB(OPENSOCKET)
            CASE_CB(SEEK)
            //CASE_CB(SSH_KEY)
            //CASE_CB(INTERLEAVE)
            //CASE_CB_NB(CHUNK_BGN_) l_second_func_ref = &leasy->l_func_ref[L_FUNC_REF_CHUNK_END_];break;
            //CASE_CB_NB(CHUNK_END_) l_second_func_ref = &leasy->l_func_ref[L_FUNC_REF_CHUNK_BGN_];break;
            //CASE_CB(FNMATCH_)
            //CASE_CB(CLOSESOCKET)
            CASE_CB(XFERINFO)
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






static int lcurl_easy_index(lua_State * L){
    leasy_t * leasy=lua_touserdata(L,1);
    const char * key=luaL_checkstring(L,2);
    lua_getfield(L,lua_upvalueindex(1),key);
    if (lua_type(L,-1)!=LUA_TNUMBER) {
        if (strcmp(key,"headers")==0){
            // headers
            lua_newtable(L);
            struct curl_header *prev = NULL;
            struct curl_header *h;
            unsigned int origin = CURLH_HEADER| CURLH_1XX | CURLH_TRAILER | CURLH_CONNECT|CURLH_CONNECT;
            int i=0;
            while((h = curl_easy_nextheader(leasy->easy, origin, -1, prev))) {
                lua_pushstring(L,h->name);
                lua_pushstring(L,h->value);
                lua_pushvalue(L,-2);
                lua_pushliteral(L, ": ");
                lua_pushvalue(L, -3);
                lua_concat(L, 3);
                lua_rawseti(L,-4,++i);
                lua_rawset(L,-3);
                prev = h;
            }
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
            lua_pushinteger(L,out);
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
    //return luaL_error(L,"invalid key (unavailable)");
}



static int lcurl_easy_call(lua_State * L){
    leasy_t * leasy=lua_touserdata(L,1);
    leasy->error[0]='\0';
    int ret=curl_easy_perform(leasy->easy);
    if (ret==CURLE_OK){
        if (leasy->ref_idx_str_err!=LUA_NOREF){
            luaL_unref(leasy->L,LUA_REGISTRYINDEX,leasy->ref_idx_str_err);
            leasy->ref_idx_str_err=LUA_NOREF;
        }
        lua_pushboolean(L,1);
        return 1;
    }else{
        lua_pushnil(L);
        _lcurl_push_error(L,ret,1);
        lua_pushinteger(L,ret);
        return 3;
    }
}



static int lcurl_easy(lua_State * L){
    CURL * easy=curl_easy_init();
    if (!easy) return luaL_error(L,"NULL returned");
    leasy_t * leasy=lua_newuserdata(L,sizeof(leasy_t));
    if (!leasy){
        curl_easy_cleanup(easy);
        return luaL_error(L,"NULL returned");
    }
    curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, leasy->error);
    curl_easy_setopt(easy, CURLOPT_PRIVATE,NULL);
    leasy->L=L;
    leasy->ref_idx_cb_on_done=LUA_NOREF;
    leasy->ref_idx_str_err=LUA_NOREF;
    for (int i=0;i<L_FUNC_REF_LENGTH;i++)
        leasy->l_func_ref[i]=LUA_NOREF;
    for (int i=0;i<C_PTR_SLIST_LENGTH;i++)
        leasy->c_ptr_slist[i]=NULL;
    leasy->easy=easy;
    leasy->mimepost=NULL;
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setmetatable(L,-2);
    return 1;
}


#define EIV(A) lua_pushinteger(L,CURLE_##A);lua_pushstring(L,#A);lua_rawset(L,-3);
static void init_easy_errors_consts(lua_State * L){
    lua_newtable(L);
	EIV(OK)
	EIV(UNSUPPORTED_PROTOCOL)
	EIV(FAILED_INIT)
	EIV(URL_MALFORMAT)
	EIV(NOT_BUILT_IN)
	EIV(COULDNT_RESOLVE_PROXY)
	EIV(COULDNT_RESOLVE_HOST)
	EIV(COULDNT_CONNECT)
	EIV(WEIRD_SERVER_REPLY)
	EIV(REMOTE_ACCESS_DENIED)
	EIV(FTP_ACCEPT_FAILED)
	EIV(FTP_WEIRD_PASS_REPLY)
	EIV(FTP_ACCEPT_TIMEOUT)
	EIV(FTP_WEIRD_PASV_REPLY)
	EIV(FTP_WEIRD_227_FORMAT)
	EIV(FTP_CANT_GET_HOST)
	EIV(HTTP2)
	EIV(FTP_COULDNT_SET_TYPE)
	EIV(PARTIAL_FILE)
	EIV(FTP_COULDNT_RETR_FILE)
	EIV(OBSOLETE20)
	EIV(QUOTE_ERROR)
	EIV(HTTP_RETURNED_ERROR)
	EIV(WRITE_ERROR)
	EIV(OBSOLETE24)
	EIV(UPLOAD_FAILED)
	EIV(READ_ERROR)
	EIV(OUT_OF_MEMORY)
	EIV(OPERATION_TIMEDOUT)
	EIV(OBSOLETE29)
	EIV(FTP_PORT_FAILED)
	EIV(FTP_COULDNT_USE_REST)
	EIV(OBSOLETE32)
	EIV(RANGE_ERROR)
	EIV(HTTP_POST_ERROR)
	EIV(SSL_CONNECT_ERROR)
	EIV(BAD_DOWNLOAD_RESUME)
	EIV(FILE_COULDNT_READ_FILE)
	EIV(LDAP_CANNOT_BIND)
	EIV(LDAP_SEARCH_FAILED)
	EIV(OBSOLETE40)
	EIV(FUNCTION_NOT_FOUND)
	EIV(ABORTED_BY_CALLBACK)
	EIV(BAD_FUNCTION_ARGUMENT)
	EIV(OBSOLETE44)
	EIV(INTERFACE_FAILED)
	EIV(OBSOLETE46)
	EIV(TOO_MANY_REDIRECTS)
	EIV(UNKNOWN_OPTION)
	EIV(SETOPT_OPTION_SYNTAX)
	EIV(OBSOLETE50)
	EIV(OBSOLETE51)
	EIV(GOT_NOTHING)
	EIV(SSL_ENGINE_NOTFOUND)
	EIV(SSL_ENGINE_SETFAILED)
	EIV(SEND_ERROR)
	EIV(RECV_ERROR)
	EIV(OBSOLETE57)
	EIV(SSL_CERTPROBLEM)
	EIV(SSL_CIPHER)
	EIV(PEER_FAILED_VERIFICATION)
	EIV(BAD_CONTENT_ENCODING)
	EIV(OBSOLETE62)
	EIV(FILESIZE_EXCEEDED)
	EIV(USE_SSL_FAILED)
	EIV(SEND_FAIL_REWIND)
	EIV(SSL_ENGINE_INITFAILED)
	EIV(LOGIN_DENIED)
	EIV(TFTP_NOTFOUND)
	EIV(TFTP_PERM)
	EIV(REMOTE_DISK_FULL)
	EIV(TFTP_ILLEGAL)
	EIV(TFTP_UNKNOWNID)
	EIV(REMOTE_FILE_EXISTS)
	EIV(TFTP_NOSUCHUSER)
	EIV(OBSOLETE75)
	EIV(OBSOLETE76)
	EIV(SSL_CACERT_BADFILE)
	EIV(REMOTE_FILE_NOT_FOUND)
	EIV(SSH)
	EIV(SSL_SHUTDOWN_FAILED)
	EIV(AGAIN)
	EIV(SSL_CRL_BADFILE)
	EIV(SSL_ISSUER_ERROR)
	EIV(FTP_PRET_FAILED)
	EIV(RTSP_CSEQ_ERROR)
	EIV(RTSP_SESSION_ERROR)
	EIV(FTP_BAD_FILE_LIST)
	EIV(CHUNK_FAILED)
	EIV(NO_CONNECTION_AVAILABLE)
	EIV(SSL_PINNEDPUBKEYNOTMATCH)
	EIV(SSL_INVALIDCERTSTATUS)
	EIV(HTTP2_STREAM)
	EIV(RECURSIVE_API_CALL)
	EIV(AUTH_ERROR)
	EIV(HTTP3)
	EIV(QUIC_CONNECT_ERROR)
	EIV(PROXY)
	EIV(SSL_CLIENTCERT)
	EIV(UNRECOVERABLE_POLL)
	EIV(TOO_LARGE)
	EIV(ECH_REQUIRED)
}
#undef EIV

#define IIV(A,a) lua_pushstring(L,a);lua_pushinteger(L,CURLINFO_##A);lua_rawset(L,-3);
static void init_easy_info_consts(lua_State * L){
    lua_newtable(L);
    IIV(EFFECTIVE_URL,"effective_url")
    IIV(RESPONSE_CODE,"response_code")
    IIV(TOTAL_TIME,"total_time")
    IIV(NAMELOOKUP_TIME,"namelookup_time")
    IIV(CONNECT_TIME,"connect_time")
    IIV(PRETRANSFER_TIME,"pretransfer_time")
    IIV(SIZE_UPLOAD_T,"size_upload_t")
    IIV(SIZE_DOWNLOAD_T,"size_download_t")
    IIV(SPEED_DOWNLOAD_T,"speed_download_t")
    IIV(SPEED_UPLOAD_T,"speed_upload_t")
    IIV(HEADER_SIZE,"header_size")
    IIV(REQUEST_SIZE,"request_size")
    IIV(SSL_VERIFYRESULT,"ssl_verifyresult")
    IIV(FILETIME,"filetime")
    IIV(FILETIME_T,"filetime_t")
    IIV(CONTENT_LENGTH_DOWNLOAD_T,"content_length_download_t")
    IIV(CONTENT_LENGTH_UPLOAD_T,"content_length_upload_t")
    IIV(STARTTRANSFER_TIME,"starttransfer_time")
    IIV(CONTENT_TYPE,"content_type")
    IIV(REDIRECT_TIME,"redirect_time")
    IIV(REDIRECT_COUNT,"redirect_count")
    IIV(PRIVATE,"private")
    IIV(HTTP_CONNECTCODE,"http_connectcode")
    IIV(HTTPAUTH_AVAIL,"httpauth_avail")
    IIV(PROXYAUTH_AVAIL,"proxyauth_avail")
    IIV(OS_ERRNO,"os_errno")
    IIV(NUM_CONNECTS,"num_connects")
    IIV(SSL_ENGINES,"ssl_engines")
    IIV(COOKIELIST,"cookielist")
    IIV(FTP_ENTRY_PATH,"ftp_entry_path")
    IIV(REDIRECT_URL,"redirect_url")
    IIV(PRIMARY_IP,"primary_ip")
    IIV(APPCONNECT_TIME,"appconnect_time")
    IIV(CERTINFO,"certinfo")
    IIV(CONDITION_UNMET,"condition_unmet")
    IIV(RTSP_SESSION_ID,"rtsp_session_id")
    IIV(RTSP_CLIENT_CSEQ,"rtsp_client_cseq")
    IIV(RTSP_SERVER_CSEQ,"rtsp_server_cseq")
    IIV(RTSP_CSEQ_RECV,"rtsp_cseq_recv")
    IIV(PRIMARY_PORT,"primary_port")
    IIV(LOCAL_IP,"local_ip")
    IIV(LOCAL_PORT,"local_port")
    IIV(ACTIVESOCKET,"activesocket")
    IIV(TLS_SSL_PTR,"tls_ssl_ptr")
    IIV(HTTP_VERSION,"http_version")
    IIV(PROXY_SSL_VERIFYRESULT,"proxy_ssl_verifyresult")
    IIV(SCHEME,"scheme")
    IIV(TOTAL_TIME_T,"total_time_t")
    IIV(NAMELOOKUP_TIME_T,"namelookup_time_t")
    IIV(CONNECT_TIME_T,"connect_time_t")
    IIV(PRETRANSFER_TIME_T,"pretransfer_time_t")
    IIV(STARTTRANSFER_TIME_T,"starttransfer_time_t")
    IIV(REDIRECT_TIME_T,"redirect_time_t")
    IIV(APPCONNECT_TIME_T,"appconnect_time_t")
    IIV(RETRY_AFTER,"retry_after")
    IIV(EFFECTIVE_METHOD,"effective_method")
    IIV(PROXY_ERROR,"proxy_error")
    IIV(REFERER,"referer")
    IIV(CAINFO,"cainfo")
    IIV(CAPATH,"capath")
    IIV(XFER_ID,"xfer_id")
    IIV(CONN_ID,"conn_id")
    IIV(QUEUE_TIME_T,"queue_time_t")
    IIV(USED_PROXY,"used_proxy")
}
#undef IIV

/*static int lcurl_share_gc(lua_State * L){
    CURLSH ** pshare=lua_touserdata(L,1);
    if (*pshare){
        curl_share_cleanup(*pshare);
        *pshare=NULL;
    }
    return 0;
}*/





typedef struct{
    lua_State * L;
    //int ref_idx_easy_list;
    CURLM * multi;
}lmulti_t;



static int lcurl_multi_gc(lua_State *L){
    lmulti_t * lmulti=lua_touserdata(L,1);
    CURL **list = curl_multi_get_handles(lmulti->multi);
    if(list) {
      int i;
      /* remove all added handles */
      for(i = 0; list[i]; i++) {
        CURL * easy=list[i];
        curl_multi_remove_handle(lmulti->multi, easy);
        int * refp;
        curl_easy_getinfo(easy,CURLINFO_PRIVATE,&refp);
        if (refp!=NULL){
            luaL_unref(L, LUA_REGISTRYINDEX,*refp);
            free(refp);
            curl_easy_setopt(easy,CURLOPT_PRIVATE,NULL);
        }
      }
      curl_free(list);
    }
    curl_multi_cleanup(lmulti->multi);
    return 0;
}



int lcurl_multi(lua_State * L){
    CURLM * multi=curl_multi_init();
    if (!multi) return luaL_error(L,"NULL returned");
    lmulti_t * lmulti=lua_newuserdata(L,sizeof(lmulti_t));
    if (!lmulti){
        curl_multi_cleanup(multi);
        return luaL_error(L,"NULL returned"); 
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


int lcurl_multi_newindex(lua_State * L){
    lmulti_t * lmulti=lua_touserdata(L,1);
    int ret;
    int type=lua_type(L,2);
    if (type==LUA_TUSERDATA){
        leasy_t * leasy=lua_touserdata(L,2);
        if (lua_isnil(L, 3)){
            ret=curl_multi_remove_handle(lmulti->multi,leasy->easy);
            if (ret!=CURLM_OK){
                _lcurl_push_error(L,ret,1);
                return lua_error(L); 
            }
            int * refp;
            curl_easy_getinfo(leasy->easy,CURLINFO_PRIVATE,&refp);
            if (refp!=NULL){
                luaL_unref(L,LUA_REGISTRYINDEX,*refp);
                free(refp);
                curl_easy_setopt(leasy->easy,CURLOPT_PRIVATE,NULL);
            }
            return 0;
        }
        luaL_checktype(L,3,LUA_TFUNCTION);
        ret=curl_multi_add_handle(lmulti->multi, leasy->easy);
        if (ret!=CURLM_OK){
            _lcurl_push_error(L,ret,1);
            return lua_error(L);
        }

        //we assume that multi do not accept already linked easies
        // registry easy as global
        int * refp=malloc(sizeof(int));
        lua_pushvalue(L,2);
        *refp=luaL_ref(L,LUA_REGISTRYINDEX);
        if (curl_easy_setopt(leasy->easy,CURLOPT_PRIVATE,refp)!=CURLE_OK){
            luaL_unref(L, LUA_REGISTRYINDEX,*refp);
            free(refp);
            return luaL_error(L,"failed");
        }
        leasy->ref_idx_cb_on_done=luaL_ref(L,LUA_REGISTRYINDEX);
        leasy->error[0]='\0';
        return 0;
    }
    /*
    if (type==LUA_TSTRING){
        const char * key=lua_tostring(L, 2);

        if (strcmp(key,"chunk_length_penalty_size")==0){
            ret=curl_multi_setopt(lmulti->multi,)
        }

        static const char *int_keys[]={
            "chunk_length_penalty_size",
            "content_length_penalty_size",
            "max_host_connections",
            "max_pipeline_length",
            "max_total_connections",
            "maxconnects",
            "pipelining",
            "pipelining_site_bl",
            "pipelining_server_bl",
            "max_concurrent_streams",
            NULL
        };
        for (int i=0;int_keys[i]!=NULL;i++){
            if (strcmp(key,int_keys[i])==0){
                curl_multi_setopt(lmulti->multi,)
            }
        }
        
    }*/
    return luaL_error(L,"not imp");
}
int lcurl_multi_call(lua_State * L){
    lmulti_t * lmulti=lua_touserdata(L,1);
    int still_running=1;
    int numfds;
    int ret;
    struct CURLMsg *msg;
    int type=lua_type(L,2);
    do{
        if (type==LUA_TFUNCTION){
            lua_pushvalue(L, 2);
            if (lua_pcall(L,0,0,0)!=LUA_OK) 
                return lua_error(L);
        }
        ret=curl_multi_perform(lmulti->multi, &still_running);
        if (still_running)
            ret=curl_multi_poll(lmulti->multi, NULL, 0, 0, &numfds);
        if (ret==CURLM_OK){
            do {
                int msgq = 0;
                msg = curl_multi_info_read(lmulti->multi, &msgq);
                if(msg && (msg->msg == CURLMSG_DONE)) {
                    CURL *e = msg->easy_handle;
                    ret=curl_multi_remove_handle(lmulti->multi, e);
                    if (ret!=CURLM_OK)
                        break;
                    int * refp;
                    curl_easy_getinfo(e, CURLINFO_PRIVATE,&refp);
                    lua_rawgeti(L,LUA_REGISTRYINDEX,*refp);
                    luaL_unref(L, LUA_REGISTRYINDEX,*refp);
                    curl_easy_setopt(e,CURLOPT_PRIVATE,NULL);
                    free(refp);
                    leasy_t * leasy= lua_touserdata(L,-1);
                    lua_pop(L, 1);
                    lua_rawgeti(L,LUA_REGISTRYINDEX,leasy->ref_idx_cb_on_done);
                    luaL_unref(L,LUA_REGISTRYINDEX, leasy->ref_idx_cb_on_done);
                    leasy->ref_idx_cb_on_done=LUA_NOREF;
                    int narg;
                    if (msg->data.result==CURLE_OK){
                        lua_pushboolean(L,1);
                        narg=1;
                    }else{
                        lua_pushnil(L);
                        _lcurl_push_error(L,msg->data.result,2);
                        lua_pushinteger(L,msg->data.result);
                        narg=3;
                    }
                    if (lua_pcall(L,narg,0,0)!=LUA_OK)
                        return lua_error(L);
                }
            } while(msg);
        }
        if (ret!=CURLM_OK){
            _lcurl_push_error(L,ret,1);
            return lua_error(L);
        }
    }while(still_running);
    return 0;
}


#define MIV(A) lua_pushinteger(L,CURLM_##A);lua_pushstring(L,#A);lua_rawset(L,-3);
static void init_multi_errors_consts(lua_State * L){
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





int luaopen_xcurl(lua_State * L){
    lua_newtable(L);
    //init_easy_errors_consts(L);
    {
        lua_newtable(L);
        init_easy_info_consts(L);
        lua_pushcclosure(L, lcurl_easy_index,1);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lcurl_easy_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, lcurl_easy_newindex);
        lua_setfield(L,-2, "__newindex");
        //lua_pushvalue(L,-2);
        init_easy_errors_consts(L);
        lua_pushcclosure(L, lcurl_easy_call,1);
        lua_setfield(L,-2, "__call");

        lua_pushcclosure(L, lcurl_easy,1);
        lua_setfield(L,-2,"easy");
    }
    {
        lua_newtable(L);
        lua_pushcfunction(L, lcurl_multi_gc);
        lua_setfield(L,-2,"__gc");
        init_multi_errors_consts(L);
        lua_pushvalue(L,-1);
        lua_pushcclosure(L,lcurl_multi_newindex,1);
        lua_setfield(L, -3, "__newindex");
        //lua_pushvalue(L,-3);
        init_easy_errors_consts(L);
        lua_pushcclosure(L,lcurl_multi_call,2);
        lua_setfield(L, -2, "__call");

        lua_pushcclosure(L,lcurl_multi,1);
        lua_setfield(L, -2, "multi");
    }
    //lua_pop(L, 1);//pop err const tb frm stack
    curl_global_init(CURL_GLOBAL_ALL);
    return 1;
}



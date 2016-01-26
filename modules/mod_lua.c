#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include "tsar.h"
#include <assert.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

struct stats_lua {
    unsigned long long luadat;     /* accepted connections */
};

static char *lua_usage = "    --lua   call lua";

static struct mod_info lua_info[] = {
/*as merge options are realated to the order of
 * putting data into tsar framework (in function read_lua_stats)
 * so, all values are merged by adding data from diffrent ports together 
 * but rt and sslhst are average values (sum(diff)/sum(diff)== average)*/
/*                          merge_opt work on ---->         merge_opt work here */
    {"luadat", SUMMARY_BIT,  MERGE_SUM,  STATS_SUB},         /*0 st_lua.naccept*/
};
#define MAX_LUA 10
static char lua_file[MAX_LUA][256];
static int  lua_count = 0;
static int  lua_set_current = 0;
static int  lua_read_current = 0;
static void
set_lua_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    lua_State *L = luaL_newstate();
    char *lua_file_p = lua_file[lua_set_current++];

    if (lua_set_current >= lua_count) lua_set_current = 0 ;

    //luaL_openlibs(L);
    luaL_openlibs(L);
    // Tell Lua to load & run the file sample_script.lua
    //
    luaL_dofile(L, lua_file_p);
    // Push "sum" function to the Lua stack
    lua_getglobal(L, "set_data");
    // Checks if top of the Lua stack is a function.
    if(lua_isfunction(L, -1)) {
        lua_pushnumber(L, pre_array[0]); // push b to the top of the stack
        lua_pushnumber(L, cur_array[0]); // push b to the top of the stack
        lua_pushnumber(L, inter); // push b to the top of the stack
        // Perform the function call (2 arguments, 1 result) */
        if (lua_pcall(L, 3, 1, 0) != 0) {
            printf("Error running function f:%s`", lua_tostring(L, -1));
        }
        
        // Let's retrieve the function result
        // Check if the top of the Lua Stack has a numeric value
        if (!lua_isnumber(L, -1)) {
            printf("Function `sum' must return a number");
        }
        st_array[0]  = (double)lua_tonumber(L, -1); // retrieve the result 
    }

    lua_pop(L, 1); // Pop retrieved value from the Lua stack

    // Close the Lua state
    lua_close(L);
}


/*
 *read data from nginx and store the result in buf
 * */
static int
read_one_lua_stats(char *parameter, char * buf, int pos)
{
    int             write_flag = 0, addr_len, domain;
    int             m, sockfd, send;
    void            *addr;
    char            request[LEN_4096], line[LEN_4096];
    FILE            *stream = NULL;
    struct stats_lua st_lua;
    memset(&st_lua, 0, sizeof(struct stats_lua));
    int error;
    // Create new Lua state and load the lua libraries
    lua_State *L = luaL_newstate();
    //luaL_openlibs(L);
    luaL_openlibs(L);
    // Tell Lua to load & run the file sample_script.lua
    luaL_dofile(L, parameter);
    //luaL_loadfile(L,parameter);
    // Push "sum" function to the Lua stack
    lua_getglobal(L, "read_data");
    // Checks if top of the Lua stack is a function.
    if(lua_isfunction(L, -1)) {
        // Perform the function call (2 arguments, 1 result) */
        if (lua_pcall(L, 0, 1, 0) != 0) {
            printf("Error running function f:%s`", lua_tostring(L, -1));
        }
        
        // Let's retrieve the function result
        // Check if the top of the Lua Stack has a numeric value
        if (!lua_isnumber(L, -1)) {
            printf("Function `sum' must return a number");
        }
        st_lua.luadat  = lua_tonumber(L, -1); // retrieve the result 
    }

    lua_pop(L, 1); // Pop retrieved value from the Lua stack

    // Close the Lua state
    lua_close(L);
    
   write_flag = 1; 
    if (write_flag) {
        pos += snprintf(buf + pos, LEN_1M - pos, "%s=%lld" ITEM_SPLIT,
                parameter,
                st_lua.luadat
                );
        if (strlen(buf) == LEN_1M - 1) {
            return -1;
        }
        return pos;
    } else {
        return pos;
    }
}

static void
read_lua_stats(struct module *mod, char *parameter)
{
    int     pos = 0;
    int     new_pos = 0;
    int     tmplen = 0;
    char    buf[LEN_1M];
    char    *token;
    char    *mod_parameter;
    buf[0] = '\0';

    DIR *dirp;
    struct dirent *direntp;
    if (lua_count == 0) {

        dirp = opendir(parameter);
        if(dirp != NULL)
        {
            while(1)
            {
                direntp = readdir(dirp);
                if(direntp == NULL)
                    break;
                else if(direntp->d_name[0] != '.') {
                    int namelen = strlen(direntp->d_name);
                    if (namelen > 3 && strncmp(direntp->d_name + namelen - 4, ".lua", 4) == 0) {
                        tmplen = strlen(parameter);
                        mod_parameter = lua_file[lua_count++];
                        printf("get one file\n");
                        strncpy(mod_parameter, parameter, tmplen);
                        strncpy(mod_parameter + tmplen, direntp->d_name, namelen);
                        mod_parameter[tmplen + namelen] = '\0';
                    }
                }
            }
            closedir(dirp);
        }
    }
    int i =0;
    for(i; i < lua_count; i++) {
        mod_parameter = lua_file[i];
        pos = read_one_lua_stats(mod_parameter, buf, pos);
        printf("get one lua stat,pos : %d\n", pos);
        if(pos == -1){
            break;
        }
    }

    if(pos != -1) {
        set_mod_record(mod,buf);
    }
    return;
    strcpy(mod_parameter, parameter);
    if ((token = strtok(mod_parameter, W_SPACE)) == NULL) {
        pos = read_one_lua_stats(token,buf,pos);
    } else {
        do {
            pos = read_one_lua_stats(token,buf,pos);
            if(pos == -1){
                break;
            } 
        }
        while ((token = strtok(NULL, W_SPACE)) != NULL);
    }
    if(pos != -1) {
        set_mod_record(mod,buf);
    }
}

void
mod_register(struct module *mod)
{
    /*get lua file number*/
    register_mod_fields(mod, "--lua", lua_usage, lua_info, 1, read_lua_stats, set_lua_record);
}

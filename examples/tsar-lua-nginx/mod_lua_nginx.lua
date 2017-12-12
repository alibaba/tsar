local _M = {
    _VERSION = "0.1.0"
}

local cjson = require("cjson")
local socket = require("socket")
local http = require("socket.http")
local ltn12 = require("ltn12")
local DETAIL_BIT = tsar.DETAIL_BIT
local SUMMARY_BIT = tsar.SUMMARY_BIT
local HIDE_BIT = tsar.HIDE_BIT
local STATS_NULL = tsar.STATS_NULL
local STATS_SUB = tsar.STATS_SUB
local STATS_SUB_INTER = tsar.STATS_SUB_INTER
local string_format = string.format
local string_match = string.match
local string_find = string.find
local concat = table.concat

local info = {
    {hdr = "accept", summary_bit = DETAIL_BIT,  merge_mode = 0, stats_opt = STATS_SUB},
    {hdr = "handle", summary_bit = DETAIL_BIT,  merge_mode = 0, stats_opt = STATS_SUB},
    {hdr = "  reqs", summary_bit = DETAIL_BIT,  merge_mode = 0, stats_opt = STATS_SUB},
    {hdr = "active", summary_bit = DETAIL_BIT,  merge_mode = 0, stats_opt = STATS_NULL},
    {hdr = "  read", summary_bit = DETAIL_BIT,  merge_mode = 0, stats_opt = STATS_NULL},
    {hdr = " write", summary_bit = DETAIL_BIT,  merge_mode = 0, stats_opt = STATS_NULL},
    {hdr = "  wait", summary_bit = DETAIL_BIT,  merge_mode = 0, stats_opt = STATS_NULL},
    {hdr = "   qps", summary_bit = SUMMARY_BIT, merge_mode = 0, stats_opt = STATS_SUB_INTER},
    {hdr = "    rt", summary_bit = SUMMARY_BIT, merge_mode = 0, stats_opt = STATS_NULL},
    {hdr = "sslqps", summary_bit = SUMMARY_BIT, merge_mode = 0, stats_opt = STATS_SUB_INTER},
    {hdr = "spdyps", summary_bit = SUMMARY_BIT, merge_mode = 0, stats_opt = STATS_SUB_INTER},
    {hdr = "sslhst", summary_bit = SUMMARY_BIT, merge_mode = 0, stats_opt = STATS_NULL},
    {hdr = "sslhsc", summary_bit = HIDE_BIT,    merge_mode = 0, stats_opt = STATS_NULL},
}

function _M.read(mod, param)
    local naccept, nhandled, nrequest, nrstime
    local result = http.request("http://127.0.0.1/nginx_status")

    local nactive = string_match(result, [[Active connections: (%d+)]])
    local nreading = string_match(result, [[Reading: (%d+)]])
    local nwriting = string_match(result, [[Writing: (%d+)]])
    local nwaiting = string_match(result, [[Waiting: (%d+)]])
    if string_find(result, [[server accepts handled requests request_time]]) then
        naccept, nhandled, nrequest, nrstime = string.match(result, "(%d+) (%d+) (%d+) (%d+)")
    elseif string_find(result, [[server accepts handled requests]]) then
        naccept, nhandled, nrequest = string.match(result, "(%d+) (%d+) (%d+)")
    else
        naccept = string_match(result, "Server accepts: (%d+)")
        nhandled = string_match(result, "nhandled: (%d+)")
        nrequest = string_match(result, "requests: (%d+)")
        nrstime = string_match(result, "request_time: (%d+)")
    end

    local d = {}

    d[1] = naccept
    d[2] = nhandled
    d[3] = nrequest
    d[4] = nactive
    d[5] = nreading
    d[6] = nwriting
    d[7] = nwaiting
    d[8] = nrequest
    d[9] = nrstime
    d[10] = 0
    d[11] = 0
    d[12] = 0
    d[13] = 0

    return concat(d, ",")
end

function _M.set(mod, st_array, pre_array, cur_array, interval)
    for i = 1, 3 do
        if cur_array[i] >= pre_array[i] then
            st_array[i] = cur_array[i] - pre_array[i]
        else
            st_array[i] = 0
        end
    end

    for i = 4, 7 do
        st_array[i] = cur_array[i]
    end

    if cur_array[3] >= pre_array[3] then
        st_array[7] =  (cur_array[3] - pre_array[3]) * 1.0 /  interval
    else
        st_array[7] = 0
    end

    if cur_array[9] >= pre_array[9] then
        if cur_array[3] > pre_array[3] then
            st_array[9] =  (cur_array[9] - pre_array[9]) * 1.0 / (cur_array[3] - pre_array[3]);
        else
            st_array[9] = 0
        end
    end

    for i = 10, 11 do
        if cur_array[i] >= pre_array[i] then
            st_array[i] = (cur_array[i] - pre_array[i]) * 1.0 / interval
        else
            st_array[i] = 0
        end
    end

    if cur_array[12] >= pre_array[12] then
        if cur_array[12] > pre_array[12] then
            st_array[12] = (cur_array[12] - pre_array[12]) * 1.0 / (cur_array[13] - pre_array[13])
        else
            st_array[12] = 0
        end
    end

    return st_array, pre_array, cur_array
end

function _M.register()
    return {
        opt = "--lua_nginx",
        usage = "nginx statistics",
        info = info,
    }
end

return _M

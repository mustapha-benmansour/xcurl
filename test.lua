

local xcurl=require'xcurl'
print(xcurl.version())




local clock=os.clock()
local def_reg_count=0

do
    for k,v in pairs(debug.getregistry()) do
        if type(v)=='function' or type(v)=='userdata' then
            def_reg_count=def_reg_count+1
        end
    end
end

local function X()
    local count=0
    for k,v in pairs(debug.getregistry()) do
        if type(v)=='function' or type(v)=='userdata' then
            count=count+1
        end
    end
    print(string.format('count %d',(count-def_reg_count)))
    clock=os.clock()
end
local function Xclock()
    local x=os.clock()
    if x-clock>.2 then
        X()
    end
end

X()

local multi=xcurl.multi()

local i=0
local t=0;
local xt=os.clock()
local add_easy
local function make_easy(i)
    ---@type xcurl.easy
    local easy=xcurl.easy()
    easy.url='http://google.com'
    --easy.nobody=false
    easy.followlocation=1
    easy.maxredirs=10
    easy.autoreferer=1
    easy.timeout_ms=5000
    easy.useragent="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36"
    easy.ssl_verifyhost=0
    easy.ssl_verifypeer=0
    easy.output=1024*512
    --[[easy.output=function(data)
        --print(data) 
        --error('hi')
        if i==10 then
            error('ERR')
        end
    end]]
    --[[easy.on_xferinfo=function (dltotal, dlnow, ultotal, ulnow)
        print(dltotal, dlnow, ultotal, ulnow)
    end]]
    return easy,function (ok)
        print('on_done',ok,easy.error.message)
        collectgarbage('collect')
        if t<100 or os.clock()-xt>2 then
            add_easy()
            xt=os.clock()
            X()
        end
        if i==10 then
            error('ERR')
        end
    end
end



function add_easy()
    i=i+1
    local easy,fn=make_easy(i)
    t=t+1
    multi:add(easy,function (ok)
        t=t-1
        X()
        fn(ok)
    end)
end



add_easy()


multi:perform(function(err)
    print('ERR=>',err)
end)






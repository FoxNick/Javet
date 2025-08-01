#ifndef QUICKJS_EXTEND_LIBRARIES
#define QUICKJS_EXTEND_LIBRARIES

#include <string>
#include <cstring>
#include "../quickjs/quickjs.h"

const char *DATE_POLYFILL = R"lit((() => {
    const _Date = Date;
    // use _Date avoid recursion in _parse.
    const _parse = (date) => {
        if (date === null) {
            // null is invalid
            return new _Date(NaN);
        }
        if (date === undefined) {
            // today
            return new _Date();
        }
        if (date instanceof Date) {
            return new _Date(date);
        }

        if (typeof date === 'string' && !/Z$/i.test(date)) {
            // YYYY-MM-DD HH:mm:ss.sssZ
            const d = date.match(/^(\d{4})[-/]?(\d{1,2})?[-/]?(\d{0,2})[Tt\s]*(\d{1,2})?:?(\d{1,2})?:?(\d{1,2})?[.:]?(\d+)?$/);
            if (d) {
                let YYYY = d[1];
                let MM = d[2] - 1 || 0;
                let DD = d[3] || 1;

                const HH = d[4] || 0;
                const mm = d[5] || 0;
                const ss = d[6] || 0;
                const sssZ = (d[7] || '0').substring(0, 3);

                // Consider that only date strings (such as "1970-01-01") will be processed as UTC instead of local time.
                let utc = (d[4] === undefined) && (d[5] === undefined) && (d[6] === undefined) && (d[7] === undefined);
                if (utc) {
                    return new Date(Date.UTC(YYYY, MM, DD, HH, mm, ss, sssZ));
                }
                return new Date(YYYY, MM, DD, HH, mm, ss, sssZ);
            }
        }

        // everything else
        return new _Date(date);
    };

    const handler = {
        construct: function (target, args) {
            if (args.length === 1 && typeof args[0] === 'string') {
                return _parse(args[0]);
            }

            return new target(...args);
        },
        get(target, prop) {
            if (typeof target[prop] === 'function' && target[prop].name === 'parse') {
                return new Proxy(target[prop], {
                    apply: (target, thisArg, argumentsList) => {
                        if (argumentsList.length === 1 && typeof argumentsList[0] === 'string') {
                            return _parse(argumentsList[0]).getTime();
                        }

                        return Reflect.apply(target, thisArg, argumentsList);
                    }
                });
            } else {
                return Reflect.get(target, prop);
            }
        }
    };

    Date = new Proxy(Date, handler);
})();)lit";

const char *CONSOLE = R"lit(// Init format at first.
{
    const LINE = "\n"
    const TAB = "  "
    const SPACE = " "

    function format(value, opt) {
        const defaultOpt = {
            maxStringLength: 10000,
            depth: 2,
            maxArrayLength: 100,
            seen: [],
            reduceStringLength: 100
        }
        if (!opt) {
            opt = defaultOpt
        } else {
            opt = Object.assign(defaultOpt, opt)
        }

        return formatValue(value, opt, 0)
    }

    function formatValue(value, opt, recurseTimes) {
        if (typeof value !== 'object' && typeof value !== 'function') {
            return formatPrimitive(value, opt)
        }

        if (value === null) {
            return 'null'
        }

        if (typeof value === 'function') {
            return formatFunction(value)
        }

        if (typeof value === 'object') {
            if (opt.seen.includes(value)) {
                let index = 1
                if (opt.circular === undefined) {
                    opt.circular = new Map()
                    opt.circular.set(value, index)
                } else {
                    index = opt.circular.get(value)
                    if (index === undefined) {
                        index = opt.circular.size + 1
                        opt.circular.set(value, index)
                    }
                }

                return `[Circular *${index}]`
            }

            if (opt.depth !== null && ((recurseTimes - 1) === opt.depth)) {
                if (value instanceof Array) {
                    return '[Array]'
                }
                return '[Object]'
            }

            recurseTimes++
            opt.seen.push(value)
            const string = formatObject(value, opt, recurseTimes)
            opt.seen.pop()
            return string
        }
    }

    function formatObject(value, opt, recurseTimes) {
        if (value instanceof RegExp) {
            return `${value.toString()}`
        }

        if (value instanceof Error) {
            return `${value.toString()}`
        }

        if (value instanceof Promise) {
            // quickjs 环境下通过 native 提供的方式获取 Promise 状态
            if (typeof getPromiseState !== "undefined") {
                const { result, state} = getPromiseState(value)
                if (state === 'fulfilled') {
                    return `Promise { ${formatValue(result, opt, recurseTimes)} }`
                } else if (state === 'rejected'){
                    return `Promise { <rejected> ${formatValue(result, opt, recurseTimes)} }`
                } else if (state === 'pending'){
                    return `Promise { <pending> }`
                }
            } else {
                return `Promise {${formatValue(value, opt, recurseTimes)}}`
            }
        }

        if (value instanceof Array) {
            return formatArray(value, opt, recurseTimes)
        }

        if (value instanceof Float64Array) {
            return `Float64Array(1) [ ${value} ]`
        }

        if (value instanceof BigInt64Array) {
            return `BigInt64Array(1) [ ${value}n ]`
        }

        if (value instanceof Map) {
            return formatMap(value, opt, recurseTimes)
        }

        return formatProperty(value, opt, recurseTimes)
    }

    function formatProperty(value, opt, recurseTimes) {
        let string = ''
        string += '{'
        const keys = Object.keys(value)
        const length = keys.length
        for (let i = 0; i < length; i++) {
            if (i === 0) {
                string += SPACE
            }
            string += LINE
            string += TAB.repeat(recurseTimes)

            const key = keys[i]
            string += `${key}: `
            string += formatValue(value[key], opt, recurseTimes)
            if (i < length -1) {
                string += ','
            }
            string += SPACE
        }

        string += LINE
        string += TAB.repeat(recurseTimes - 1)
        string += '}'

        if (string.length < opt.reduceStringLength) {
            string = string.replaceAll(LINE, "").replaceAll(TAB, "")
        }

        return string
    }

    function formatMap(value, opt, recurseTimes) {
        let string = `Map(${value.size}) `
        string += '{'
        let isEmpty = true
        value.forEach((v, k, map) => {
            isEmpty = false
            string += ` ${format(k, opt, recurseTimes)} => ${format(v, opt, recurseTimes)}`
            string += ','
        })

        if (!isEmpty) {
            // 删除最后多余的逗号
            string = string.substr(0, string.length -1) + ' '
        }

        string += '}'
        return string
    }

    function formatArray(value, opt, recurseTimes) {
        let string = '['
        value.forEach((item, index, array) => {
            if (index === 0) {
                string += ' '
            }
            string += formatValue(item, opt, recurseTimes)
            if (index === opt.maxArrayLength - 1) {
                string += `... ${array.length - opt.maxArrayLength} more item${array.length - opt.maxArrayLength > 1 ? 's' : ''}`
            } else if (index !== array.length - 1) {
                string += ','
            }
            string += ' '
        })
        string += ']'
        return string
    }

    function formatFunction(value) {
        let type = 'Function'

        if (value.constructor.name === 'AsyncFunction') {
            type = 'AsyncFunction'
        }

        if (value.constructor.name === 'GeneratorFunction') {
            type = 'GeneratorFunction'
        }

        if (value.constructor.name === 'AsyncGeneratorFunction') {
            type = 'AsyncGeneratorFunction'
        }

        let fn = `${value.name ? `: ${value.name}` : ' (anonymous)'}`
        return `[${type + fn}]`
    }

    function formatPrimitive(value, opt) {
        const type = typeof value
        switch (type) {
            case "string":
                return formatString(value, opt)
            case "number":
                return Object.is(value, -0) ? '-0' : `${value}`
            case "bigint":
                return `${String(value)}n`
            case "boolean":
                return `${value}`
            case "undefined":
                return "undefined"
            case "symbol":
                return `${value.toString()}`
            default:
                return value.toString
        }
    }

    function formatString(value, opt) {
        let trailer = ''
        if (opt.maxStringLength && value.length > opt.maxStringLength) {
            const remaining = value.length - opt.maxStringLength
            value = value.slice(0, opt.maxStringLength)
            trailer = `... ${remaining} more character${remaining > 1 ? 's' : ''}`
        }

        return `'${value}'${trailer}`
    }

    globalThis.format = format
}

// Then console init.
{
    globalThis.console = {
        stdout: function (level, msg) {
            throw new Error("When invoke console stuff, you should be set a stdout of platform to console.stdout.")
        },
        log: function (...args) {
            this.print("log", ...args)
        },
        debug: function(...args) {
            this.print("debug", ...args)
        },
        info: function (...args) {
            this.print("info", ...args)
        },
        warn: function (...args) {
            this.print("warn", ...args)
        },
        error: function (...args) {
            this.print("error", ...args)
        },
        print: function (level, ...args) {
            let msg = ''
            args.forEach((value, index) => {
                if (index > 0) {
                    msg += ", "
                }

                msg += globalThis.format(value)
            })

            this.stdout(level, msg)
        }
    }
})lit";
const char *POLYWASM = R"lit(var ge=new ArrayBuffer(8),ke=new Float32Array(ge),Ee=new Float64Array(ge),Ae=new Int32Array(ge),Ge=new BigInt64Array(ge),Se=new BigUint64Array(ge),ve=new WeakMap,Je=[],xe=class{},te={U(n,l){return(n<0||n===0&&Object.is(n,-0))!==(l<0||l===0&&Object.is(l,-0))?-n:n},d(n){return Se[0]=n,Ge[0]},H(n){return ke[0]=n,Ae[0]},Q(n){return Ae[0]=n,ke[0]},W(n){return Ee[0]=n,Se[0]},q(n){return Se[0]=n,Ee[0]},N(n,l){return n<<l|n>>>32-l},D(n,l){return n>>>l|n<<32-l},V(n,l){return(n<<l|n>>64n-l)&0xFFFFFFFFFFFFFFFFn},j(n,l){return(n>>l|n<<64n-l)&0xFFFFFFFFFFFFFFFFn},G(n){return n?Math.clz32(n&-n)^31:32},J(n){let l=0;for(;n;)l++,n&=n-1;return l},K(n){let l=Math.clz32(Number(n>>32n&0xFFFFFFFFn));return l===32&&(l+=Math.clz32(Number(n&0xFFFFFFFFn))),BigInt(l)},Z(n){let l=Number(n&0xFFFFFFFFn);return l?BigInt(Math.clz32(l&-l)^31):(l=Number(n>>32n&0xFFFFFFFFn),l?BigInt(32+Math.clz32(l&-l)^31):64n)},X(n){let l=0n;for(;n;)l++,n&=n-1n;return l},P(n){return n=Math.trunc(n),n>=2147483647?2147483647:n<=-2147483648?-2147483648:n|0},O(n){return n=Math.trunc(n),n>=4294967295?-1:n<=0?0:n|0},I(n){return n=Math.trunc(n),n>=9223372036854776e3?0x7FFFFFFFFFFFFFFFn:n<=-9223372036854776e3?0x8000000000000000n:n===n?BigInt(n)&0xFFFFFFFFFFFFFFFFn:0n},T(n){return n=Math.trunc(n),n>=18446744073709552e3?0xFFFFFFFFFFFFFFFFn:n>0?BigInt(n):0n},Y(n){return n&0x80n?n|0xFFFFFFFFFFFFFF00n:n&0xFFn},ee(n){return n&0x8000n?n|0xFFFFFFFFFFFF0000n:n&0xFFFFn},te(n){return n&0x80000000n?n|0xFFFFFFFF00000000n:n&0xFFFFFFFFn},B(n,l,u,y,O){n===l?l.copyWithin(u,y,y+O):l.set(n.subarray(y,y+O),u)},z(n,l,u,y,O){if(u>>>=0,y>>>=0,O>>>=0,y+O>l.length||u+O>n.length)throw RangeError();if(u<=y)for(let p=0;p<O;p++)n[u+p]=l[y+p];else for(let p=O-1;p>=0;p--)n[u+p]=l[y+p]},w(n,l,u){let y=n.length;if(u>>>=0,y+u>n.ae)return-1;for(let O=0;O<u;O++)n.push(l);return y},ne(n,l,u,y){if(l>>>=0,y>>>=0,l+y>n.length)throw RangeError();for(let O=0;O<y;O++)n[l+O]=u},C(n,l,u){if(n instanceof xe)return n.m=l,n.le=u,Je;n=new xe;let y;for(;y=l.apply(n,u),n.m;)l=n.m,u=n.le,n.m=null;return y},h(n){if(n===null)return n;let l=ve.get(n);if(l)return l;throw Error("Unexpected foreign function object")},M(n){if(n===null)return null;if(!n.p){let[l,u]=n.ie,y=[],O=[];for(let x=0;x<l.length;x++)y.push("a"+x),O.push(fe("a"+x,l[x]));let p=`f.x(${O})`;if(u.length===1)p="return "+Fe(p,u[0]);else if(u.length>1){p=`let r=${p};`;for(let x=0;x<u.length;x++)p+=`r[${x}]=${Fe(`r[${x}]`,u[x])};`;p+="return r"}ve.set(n.p=new Function("f","l",`return(${y})=>{${p}}`)(n,this),n)}return n.p}};var Le=n=>typeof n=="string"?n>="P"&&n<="S":n?typeof n[0]!="string"&&n[0]<0:!1;var Ue=[{e:[["$",40,41,42,43,44,45,46,47,48,49,50,51,52,53],[106,"x",[65,"Q"]],"P","R"],t:["$","x",[-2,"P","Q"],"R"]},{e:[["$",54,55,56,57,58,59,60,61,62],[106,"x",[65,"Q"]],"y","P","R"],t:["$","x","y",[-2,"P","Q"],"R"]},{e:[60,"x","y","P","R"],i:{y:[{e:[66,"Q"],t:[58,"x",[65,[-1,"Q"]],"P","R"]},{e:[["$",48,49,50,51,52,53,41],"z","Q","S"],t:[58,"x",[45,"z","Q","S"],"P","R"]},{e:[["@",172,173],"z"],t:[58,"x","z","P","R"]}]}},{e:[61,"x","y","P","R"],i:{y:[{e:[66,"Q"],t:[59,"x",[65,[-1,"Q"]],"P","R"]},{e:[48,"z","Q","S"],t:[59,"x",[44,"z","Q","S"],"P","R"]},{e:[49,"z","Q","S"],t:[59,"x",[45,"z","Q","S"],"P","R"]},{e:[["$",50,51,52,53,41],"z","Q","S"],t:[59,"x",[47,"z","Q","S"],"P","R"]},{e:[["@",172,173],"z"],t:[59,"x","z","P","R"]}]}},{e:[62,"x","y","P","R"],i:{y:[{e:[66,"Q"],t:[54,"x",[65,[-1,"Q"]],"P","R"]},{e:[48,"z","Q","S"],t:[54,"x",[44,"z","Q","S"],"P","R"]},{e:[49,"z","Q","S"],t:[54,"x",[45,"z","Q","S"],"P","R"]},{e:[50,"z","Q","S"],t:[54,"x",[46,"z","Q","S"],"P","R"]},{e:[51,"z","Q","S"],t:[54,"x",[47,"z","Q","S"],"P","R"]},{e:[["$",52,53,41],"z","Q","S"],t:[54,"x",[40,"z","Q","S"],"P","R"]},{e:[["@",172,173],"z"],t:[54,"x","z","P","R"]}]}},{e:[80,"x"],i:{x:[{e:[["$",48,49],"y","P","R"],t:[69,[45,"y","P","R"]]},{e:[["$",50,51],"y","P","R"],t:[69,[47,"y","P","R"]]},{e:[["$",52,53],"y","P","R"],t:[69,[40,"y","P","R"]]},{e:[["@",172,173],"y"],t:[69,"y"]}]}},{e:[81,[49,"x","P","R"],[66,"Q"]],t:[70,[45,"x","P","R"],[65,[-1,"Q"]]],n:["Q","<=",0xFFn]},{e:[82,[49,"x","P","R"],[66,"Q"]],t:[71,[45,"x","P","R"],[65,[-1,"Q"]]],n:["Q","<=",0xFFn]},{e:[81,[51,"x","P","R"],[66,"Q"]],t:[70,[47,"x","P","R"],[65,[-1,"Q"]]],n:["Q","<=",0xFFFFn]},{e:[82,[51,"x","P","R"],[66,"Q"]],t:[71,[47,"x","P","R"],[65,[-1,"Q"]]],n:["Q","<=",0xFFFFn]},{e:[81,[53,"x","P","R"],[66,"Q"]],t:[70,[40,"x","P","R"],[65,[-1,"Q"]]],n:["Q","<=",0xFFFFFFFFn]},{e:[82,[53,"x","P","R"],[66,"Q"]],t:[71,[40,"x","P","R"],[65,[-1,"Q"]]],n:["Q","<=",0xFFFFFFFFn]},{e:[240,"x"],i:{x:[{e:[242,"y"],t:[240,"y"]},{e:[["@",69,80],"x"],t:[241,"y"]}]}},{e:[241,"x"],i:{x:[{e:[242,"y"],t:[241,"y"]},{e:[["@",69,80],"y"],t:[240,"y"]},{e:[70,"y","z"],t:[240,[71,"y","z"]]},{e:[71,"y","z"],t:[240,[70,"y","z"]]},{e:[72,"y","z"],t:[240,[78,"y","z"]]},{e:[73,"y","z"],t:[240,[79,"y","z"]]},{e:[74,"y","z"],t:[240,[76,"y","z"]]},{e:[75,"y","z"],t:[240,[77,"y","z"]]},{e:[76,"y","z"],t:[240,[74,"y","z"]]},{e:[77,"y","z"],t:[240,[75,"y","z"]]},{e:[78,"y","z"],t:[240,[72,"y","z"]]},{e:[79,"y","z"],t:[240,[73,"y","z"]]},{e:[81,"y","z"],t:[240,[82,"y","z"]]},{e:[82,"y","z"],t:[240,[81,"y","z"]]},{e:[83,"y","z"],t:[240,[89,"y","z"]]},{e:[84,"y","z"],t:[240,[90,"y","z"]]},{e:[85,"y","z"],t:[240,[87,"y","z"]]},{e:[86,"y","z"],t:[240,[88,"y","z"]]},{e:[87,"y","z"],t:[240,[85,"y","z"]]},{e:[88,"y","z"],t:[240,[86,"y","z"]]},{e:[89,"y","z"],t:[240,[83,"y","z"]]},{e:[90,"y","z"],t:[240,[84,"y","z"]]}]}},{e:[243,"x"],i:{x:[{e:[40,"y","P","R"],t:[245,"y","P","R"]}]}},{e:[244,"x"],i:{x:[{e:[41,"y","P","R"],t:[246,"y","P","R"]},{e:[66,"P"],t:[66,"P"],n:["P","<=",0x7FFFFFFFFFFFFFFFn]},{e:[["$",49,51,53],"y","P","R"],t:["$","y","P","R"]}]}},{e:[167,"x"],i:{x:[{e:[66,"P"],t:[65,[-1,"P"]]},{e:[48,"y","P","R"],t:[44,"y","P","R"]},{e:[49,"y","P","R"],t:[45,"y","P","R"]},{e:[50,"y","P","R"],t:[46,"y","P","R"]},{e:[51,"y","P","R"],t:[47,"y","P","R"]},{e:[["$",52,53,41],"y","P","R"],t:[40,"y","P","R"]},{e:[["@",172,173],"y"],t:"y"},{e:[124,[["@",172,173],"y"],[66,"P"]],t:[106,"y",[65,[-1,"P"]]]}]}},{e:[131,"x",[66,"P"]],i:{x:[{e:[66,"Q"],t:[66,[-3,"P","Q"]]},{e:[131,"y",[66,"Q"]],t:[131,"y",[66,[-3,"P","Q"]]]},{e:[49,"y","Q","S"],t:[49,"y","Q","S"],n:[["P","&",0xFFn],"===",0xFFn]},{e:[48,"y","Q","S"],t:[49,"y","Q","S"],n:["P","===",0xFFn]},{e:[51,"y","Q","S"],t:[51,"y","Q","S"],n:[["P","&",0xFFFFn],"===",0xFFFFn]},{e:[50,"y","Q","S"],t:[51,"y","Q","S"],n:["P","===",0xFFFFn]},{e:[53,"y","Q","S"],t:[53,"y","Q","S"],n:[["P","&",0xFFFFFFFFn],"===",0xFFFFFFFFn]},{e:[52,"y","Q","S"],t:[53,"y","Q","S"],n:["P","===",0xFFFFFFFFn]}]}}],He=()=>{let n=0,l=()=>"v"+n++,u=(M,b,$,v,S)=>{if(b<$.length){let C=$[b];if(typeof C=="string")A[C]=`${B}[${M}+${b+1}]`,u(M,b+1,$,v,S);else{let E=l(),z=l();i+=`var ${E}=${B}[${M}+${b+1}],${z}=${B}[${E}]&65535;`,y(E,z,C,v,c=>{u(M,b+1,$,c,S)})}}else S(v)},y=(M,b,[$,...v],S,C)=>{let E=[];if(typeof $=="number")E.push(`${b}===${$}`);else{let[z,...c]=$;c.sort((a,_)=>a-_),U[z]={y:M,se:b,oe:c.some(a=>P.has(a))};for(let a=0;a<c.length;a++){let _=1;for(;a+_<c.length&&c[a+_-1]+1===c[a+_];)_++;E.push(_>2?`${b}>=${c[a]}&&${b}<=${c[a+=_-1]}`:`${b}===${c[a]}`)}}S=S.concat({y:M,b:v.map(z=>typeof z=="string"?z:null)}),i+=`if(${E.join("||")}){`,u(M,0,v,S,C),i+="}"},O=(M,b,$,v,S,C)=>{for(let{e:E,i:z,t:c,n:a}of $)y(M,b,E,S,_=>{let F=Object.create(C);p(a,F,()=>{if(z){for(let o in z)x(o,F);for(let o in z){let f=F[o],d=l();i+=`var ${d}=${B}[${f}]&65535;`,O(f,d,z[o],null,_,F)}}if(c){let o=T(c,F,_.slice(),`|${B}[${N}]&${-1<<24}`);typeof c!="string"&&(typeof c[0]=="string"?U[c[0]].oe:P.has(c[0]))?(N!==o&&(i+=`${N}=${o};`),i+="continue"):i+="return "+o}})})},p=(M,b,$)=>{if(M){let v=S=>typeof S=="string"?`${D}[${b[S]||A[S]}]&0xFFFFFFFFFFFFFFFFn`:typeof S=="bigint"?S+"n":`(${v(S[0])})${S[1]}(${v(S[2])})`;i+=`if(${v(M)}){`,$(),i+="}"}else $()},x=(M,b)=>{if(!(M in b)){let $=l();i+=`var ${$}=${A[M]};`,b[M]=$}},T=(M,b,$,v="")=>{if(typeof M=="string")return b[M]||A[M];if(M[0]===-1){let o=T(M[1],b,$);return`Number(${D}[${o}]&0xFFFFFFFFn)`}if(M[0]===-2){let o=T(M[1],b,$),f=T(M[2],b,$);return`${o}+${f}`}if(M[0]===-3){let o=M[1];typeof o=="string"&&x(o,b);let f=T(o,b,$),d=T(M[2],b,$);return i+=`${D}[${f}]&=${D}[${d}];`,f}let[S,...C]=M,z=C.length-+Le(C[C.length-1])-+Le(C[C.length-2])<<16,c=-1,a,_,F;for(let o=0;o<$.length;o++){let f=$[o];if(f.b.length===C.length){let d=0;for(let I=0;I<C.length;I++)C[I]===f.b[I]&&d++;d>c&&(c=d,a=o,_=f.y,F=f.b)}}if(!(typeof S=="string"&&U[S].y===_)){let o=(typeof S=="string"?`${U[S].se}|${z}`:`${S|z}`)+v;_?($.splice(a,1),i+=`${B}[${_}]=${o};`):(_=l(),i+=`var ${_}=${J}(${o},${M.length});`)}for(let o=0;o<C.length;o++)if(F&&C[o]!==F[o]){let f=T(C[o],b,$);i+=`${B}[${_}+${o+1}]=${f};`}return _},A={},U={},Y=l(),B=l(),D=l(),J=l(),N=l(),t=l(),P=new Set;for(let{e:[M]}of Ue)if(typeof M=="number")P.add(M);else{let[,...b]=M;for(let $ of b)P.add($)}let i=`for(;;){var ${t}=${B}[${N}]&65535;`;return O(N,t,Ue,null,[],{}),i+=`return ${N}}`,new Function(B,D,J,N,i)};var Ze=n=>{let l=new DataView(n.buffer),u=()=>{let a=0,_=0,F;do F=n[c++],a|=(F&127)<<_,_+=7;while(F&128);return a>>>0},y=()=>{let a=0,_=0,F;do F=n[c++],a|=(F&127)<<_,_+=7;while(F&128);return _<32&&F&64?a|-1<<_:a},O=()=>{let a=0n,_=0n,F;do F=n[c++],a|=BigInt(F&127)<<_,_+=7n;while(F&128);return _<64&&F&64?a|~0n<<_:a},p=()=>{let a=l.getFloat32(c,!0);return c+=4,a},x=()=>{let a=l.getFloat64(c,!0);return c+=8,a},T=(a=u())=>[...n.slice(c,c+=a)],A=(a=u())=>decodeURIComponent(escape(String.fromCharCode(...n.slice(c,c+=a)))),U=(a=n[c++])=>[u(),a===0?1/0:u()],Y=()=>{let a=[],_;for(;(_=n[c++])!==11;)if(_===65){let F=u();a.push(()=>F)}else if(_===35){let F=u();a.push(o=>{if(F>=o.length)throw RangeError();return o[F]})}else if(_===106){let F=a.pop(),o=a.pop();a.push(f=>o(f)+F(f)|0)}else if(_===107){let F=a.pop(),o=a.pop();a.push(f=>o(f)-F(f)|0)}else if(_===108){let F=a.pop(),o=a.pop();a.push(f=>Math.imul(o(f),F(f)))}else throw new Q("Unsupported constant instruction: "+L(_));if(a.length!==1)throw new Q("Unsupported constant");return a[0]},B=()=>{let a=n[c++],_;if(a===210)_=u();else if(a===208){if(n[c++]!==112)throw new Q("Unsupported reference type: "+L(n[c-1]));_=null}else throw new Q("Unsupported constant instruction: "+L(a));if(n[c++]!==11)throw new Q("Expected end after constant: "+L(n[c-1]));return _},D=a=>{let _=n[c++],F;if(_===65&&a===127){let o=y();F=()=>o}else if(_===66&&a===126){let o=O();F=()=>o}else if(_===67&&a===125){let o=p();F=()=>o}else if(_===68&&a===124){let o=x();F=()=>o}else if(_===208&&(a===112||a===111))c++,F=()=>null;else if(_===210&&a===112){let o=u();F=(f,d)=>d(o)}else if(_===35){let o=u();F=f=>{if(o>=f.length)throw RangeError();return f[o]}}else throw new Q("Unsupported constant instruction: "+L(_));if(n[c++]!==11)throw new Q("Expected end after constant: "+L(n[c-1]));return F},J=[],N=[],t=[],P=[],i=[],M=[],b=[],$=[],v=[],S=new Map,C=[],E=[],z=-1,c=8;if(n.slice(0,8).join(",")!=="0,97,115,109,1,0,0,0")throw new Q("Invalid file header");for(;c+5<n.length;){let a=n[c++],_=u(),F=c+_;if(a===0){let o=A();if(N.push([o,n.slice(c,F)]),o==="name"){let f=n[c++],d=c+u();if(f===1)for(let I=0,R=u();I<R&&c<d;I++)S.set(u(),A())}}else if(a===1)for(let o=0,f=u();o<f;o++){if(n[c++]!==96)throw new Q("Invalid function type: "+L(n[c-1]));E.push([T(),T()])}else if(a===2)for(let o=0,f=u();o<f;o++){let d=A(),I=A(),R=n[c++];if(R===0)$.push([d,I,R,u()]);else if(R===1)$.push([d,I,R,n[c++],...U()]);else if(R===2)$.push([d,I,R,...U()]);else if(R===3)$.push([d,I,R,n[c++],n[c++]]);else throw new Q("Unsupported import type: "+L(R))}else if(a===3){let o=u();for(let f=0;f<o;f++)M.push(u())}else if(a===4)for(let o=0,f=u();o<f;o++)C.push([n[c++],...U()]);else if(a===5)for(let o=0,f=u();o<f;o++)v.push(U());else if(a===6)for(let o=0,f=u();o<f;o++){let d=n[c++],I=n[c++],R=D(d);b.push([d,I,R])}else if(a===7)for(let o=0,f=u();o<f;o++){let d=A(),I=n[c++],R=u();i.push([d,I,R])}else if(a===8)z=u();else if(a===9)for(let o=0,f=u();o<f;o++){let d=u();if(d>7)throw new Q("Unsupported element kind: "+d);let I=d&3,R=I===2?u():I===0?0:null,K=d&1?null:Y();if(I&&n[c++]!==(d&4?112:0))throw new Q("Unsupported element type: "+L(n[c-1]));let Me=[];for(let pe=0,ie=u();pe<ie;pe++)Me.push(d&4?B():u());P.push([R,K,Me])}else if(a===10)for(let o=0,f=u();o<f;o++){let d=u()+c,I=u(),R=[];for(let K=0;K<I;K++)R.push([u(),n[c++]]);J.push([R,c,d]),c=d}else if(a===11)for(let o=0,f=u();o<f;o++){let d=u();if(d>2)throw new Q("Unsupported data mode: "+d);let I=d===2?u():0,R=d===1?null:Y(),K=u();t.push([I,R,n.slice(c,c+=K)])}else if(a!==12)throw new Q("Unsupported section type: "+L(a));c=F}return{re:n,o:l,R:J,pe:N,_e:t,ue:P,ce:i,k:M,Fe:b,ge:$,fe:v,de:S,me:z,he:C,E}},L=n=>"0x"+n.toString(16).toUpperCase().padStart(2,"0"),Pe=new Map,X=class{constructor(l){Pe.set(this,Ze(l instanceof Uint8Array?l:new Uint8Array(l instanceof ArrayBuffer?l:l.buffer)))}},Q=class extends Error{constructor(l){super(l),this.name="CompileError"}};var Oe=(n,l)=>{if(l===125||l===124)return+n;if(l===127)return n|0;if(l===126)return BigInt(n)&0xFFFFFFFFFFFFFFFFn;if(l===111)return n;throw Error("Unsupported cast to type: "+L(l))},fe=(n,l)=>{if(l===125||l===124)return"+"+n;if(l===127)return n+"|0";if(l===126)return`BigInt(${n})&0xFFFFFFFFFFFFFFFFn`;if(l===111)return n;if(l===112)return`l.h(${n})`;throw Error("Unsupported cast to type: "+L(l))},Fe=(n,l)=>{if(l===124||l===127)return n;if(l===125)return`Math.fround(${n})`;if(l===126)return`l.d(${n})`;if(l===111)return n;if(l===112)return`l.M(${n})`;throw Error("Unsupported cast to type: "+L(l))};var Ye={1:520,26:521,32:28,33:25,34:29,35:28,36:25,37:29,38:26,40:61,41:61,42:61,43:61,44:61,45:61,46:61,47:61,48:61,49:61,50:61,51:61,52:61,53:61,54:58,55:58,56:58,57:58,58:58,59:58,60:58,61:58,62:58,63:28,64:29,69:13,70:78,71:78,72:78,73:206,74:78,75:206,76:78,77:206,78:78,79:206,80:13,81:78,82:78,83:334,84:78,85:334,86:78,87:334,88:78,89:334,90:78,91:78,92:78,93:78,94:78,95:78,96:78,97:78,98:78,99:78,100:78,101:78,102:78,103:13,104:13,105:13,106:14,107:14,108:14,109:14,110:142,111:14,112:142,113:14,114:14,115:14,116:14,117:14,118:14,119:14,120:14,121:13,122:13,123:13,124:14,125:14,126:14,127:270,128:14,129:270,130:14,131:14,132:14,133:14,134:1038,135:1038,136:1038,137:1038,138:1038,139:13,140:13,141:13,142:13,143:13,144:13,145:13,146:14,147:14,148:14,149:14,150:14,151:14,152:14,153:13,154:13,155:13,156:13,157:13,158:13,159:13,160:14,161:14,162:14,163:14,164:14,165:14,166:14,167:13,168:13,169:13,170:13,171:13,172:13,173:13,174:13,175:13,176:13,177:13,178:525,179:653,180:269,181:13,182:525,183:525,184:653,185:269,186:13,187:525,188:13,189:13,190:13,191:13,192:13,193:13,194:13,195:13,196:13,209:77,210:28,64512:13,64513:13,64514:13,64515:13,64516:13,64517:13,64518:13,64519:13,64521:24,64525:24,64527:30,64528:28,64529:27};var et=new Int32Array(65536),tt=He(),Qe=(n,l,u,y,O,p,x,T,A,U,Y)=>{let B=()=>{let e=0,s=0,r;do r=I[W++],e|=(r&127)<<s,s+=7;while(r&128);return e>>>0},D=()=>{let e=0,s=0,r;do r=I[W++],e|=(r&127)<<s,s+=7;while(r&128);return s<32&&r&64?e|-1<<s:e},J=()=>{let e=0n,s=0n,r;do r=I[W++],e|=BigInt(r&127)<<s,s+=7n;while(r&128);return s<64&&r&64?e|~0n<<s:e},N=()=>{let e=I[W];if(e===64)return W++,[0,0];if(e&64)return W++,[0,1];let s=B(),[r,h]=ie[s];return[r.length,h.length]},t=et,P=[],i=0,M=[],b=0,$=e=>{for(;b<e;)_e.push("s"+ ++b);return"s"+e},v={},S=e=>(v[e]||(_e.push(`t${e}=t[${e}]`),v[e]=!0),"t"+e),C=(e,s,r,h)=>`c.${e+h}[${a(s)}${r?"+"+r:""}]`,E=(e,s,r,h,w)=>`c.${e+h}[${a(s)}${r?"+"+r:""}]=${w}`,z=(e,s,r,h)=>`c.${"o"+h}.get${e}(${a(s)}${r?"+"+r:""},1)`,c=(e,s,r,h,w)=>`c.${"o"+h}.set${e}(${a(s)}${r?"+"+r:""},${w},1)`,a=e=>e<0?$(-e):`(${_(e)})`,_=e=>{let s=t[e],r=s&65535;switch(r){case 16:case 18:{let h=s>>16&255,w=t[e+h+1],[q,H]=l[w],ee=[];for(let G=1;G<=h;G++)ee.push(a(t[e+G]));let j=`f[${w}]`,se=r===18?`l.C(this,${j},[${ee}])`:`${j}(${ee})`;if(H.length<2)return se;let oe=t[e+h+2],Z=[];for(let G=0;G<H.length;G++)Z.push($(oe+G));return`[${Z}]=${se}`}case 17:case 19:{let h=s>>16&255,w=t[e+h+2],q=t[e+h+3],[H,ee]=ie[q],j=[],se=a(t[e+1]);for(let V=1;V<=h;V++)j.push(a(t[e+V+1]));let oe=`${S(w)}[${se}].x`,Z=r===19?`l.C(this,${oe},[${j}])`:`${oe}(${j})`;if(ee.length<2)return Z;let G=t[e+h+4],ue=[];for(let V=0;V<ee.length;V++)ue.push($(G+V));return`[${ue}]=${Z}`}case 27:case 28:return`${a(t[e+1])}?${a(t[e+2])}:${a(t[e+3])}`;case 32:return re[t[e+1]];case 33:case 34:return`${re[t[e+2]]}=${a(t[e+1])}`;case 35:return`g[${t[e+1]}]`;case 36:return`g[${t[e+2]}]=${a(t[e+1])}`;case 37:return S(t[e+2])+`[${a(t[e+1])}]`;case 38:return S(t[e+3])+`[${a(t[e+1])}]=${a(t[e+2])}`;case 40:return z("Int32",t[e+1],t[e+2],t[e+3]);case 245:return z("Uint32",t[e+1],t[e+2],t[e+3]);case 41:return z("BigUint64",t[e+1],t[e+2],t[e+3]);case 246:return z("BigInt64",t[e+1],t[e+2],t[e+3]);case 42:return z("Float32",t[e+1],t[e+2],t[e+3]);case 43:return z("Float64",t[e+1],t[e+2],t[e+3]);case 44:return C("u",t[e+1],t[e+2],t[e+3]);case 45:return C("l",t[e+1],t[e+2],t[e+3]);case 46:return z("Int16",t[e+1],t[e+2],t[e+3]);case 47:return z("Uint16",t[e+1],t[e+2],t[e+3]);case 48:return`BigInt(${C("u",t[e+1],t[e+2],t[e+3])})&0xFFFFFFFFFFFFFFFFn`;case 49:return`BigInt(${C("l",t[e+1],t[e+2],t[e+3])})`;case 50:return`BigInt(${z("Int16",t[e+1],t[e+2],t[e+3])})&0xFFFFFFFFFFFFFFFFn`;case 51:return`BigInt(${z("Uint16",t[e+1],t[e+2],t[e+3])})`;case 52:return`BigInt(${z("Int32",t[e+1],t[e+2],t[e+3])})&0xFFFFFFFFFFFFFFFFn`;case 53:return`BigInt(${z("Uint32",t[e+1],t[e+2],t[e+3])})`;case 54:return c("Int32",t[e+1],t[e+3],t[e+4],a(t[e+2]));case 55:return c("BigUint64",t[e+1],t[e+3],t[e+4],a(t[e+2]));case 56:return c("Float32",t[e+1],t[e+3],t[e+4],a(t[e+2]));case 57:return c("Float64",t[e+1],t[e+3],t[e+4],a(t[e+2]));case 58:return E("l",t[e+1],t[e+3],t[e+4],a(t[e+2]));case 59:return c("Int16",t[e+1],t[e+3],t[e+4],a(t[e+2]));case 60:return E("l",t[e+1],t[e+3],t[e+4],`Number(${a(t[e+2])}&255n)`);case 61:return c("Int16",t[e+1],t[e+3],t[e+4],`Number(${a(t[e+2])}&65535n)`);case 62:return c("Int32",t[e+1],t[e+3],t[e+4],`Number(${a(t[e+2])}&0xFFFFFFFFn)`);case 63:return`c.${"A"+t[e+1]}.g`;case 64:return`c.${"A"+t[e+2]}.v(${a(t[e+1])})`;case 65:return t[e+1]+"";case 66:return(M[t[e+1]]&0xFFFFFFFFFFFFFFFFn)+"n";case 67:{let h=R.getFloat32(t[e+1],!0);return Object.is(h,-0)?"-0":h+""}case 68:{let h=R.getFloat64(t[e+1],!0);return Object.is(h,-0)?"-0":h+""}case 240:return a(t[e+1]);case 241:return`!${a(t[e+1])}`;case 242:return`${a(t[e+1])}?1:0`;case 243:return`${a(t[e+1])}>>>0`;case 244:return`l.d(${a(t[e+1])})`;case 69:case 80:return`${a(t[e+1])}?0:1`;case 70:case 81:case 91:case 97:return`${a(t[e+1])}===${a(t[e+2])}`;case 71:case 82:case 92:case 98:return`${a(t[e+1])}!==${a(t[e+2])}`;case 72:case 73:case 83:case 84:case 93:case 99:return`${a(t[e+1])}<${a(t[e+2])}`;case 74:case 75:case 85:case 86:case 94:case 100:return`${a(t[e+1])}>${a(t[e+2])}`;case 76:case 77:case 87:case 88:case 95:case 101:return`${a(t[e+1])}<=${a(t[e+2])}`;case 78:case 79:case 89:case 90:case 96:case 102:return`${a(t[e+1])}>=${a(t[e+2])}`;case 103:return`Math.clz32(${a(t[e+1])})`;case 104:return`l.G(${a(t[e+1])})`;case 105:return`l.J(${a(t[e+1])})`;case 106:return`${a(t[e+1])}+${a(t[e+2])}|0`;case 107:return`${a(t[e+1])}-${a(t[e+2])}|0`;case 108:return`Math.imul(${a(t[e+1])},${a(t[e+2])})`;case 110:case 109:return`${a(t[e+1])}/${a(t[e+2])}|0`;case 112:case 111:return`${a(t[e+1])}%${a(t[e+2])}|0`;case 113:return`${a(t[e+1])}&${a(t[e+2])}`;case 114:return`${a(t[e+1])}|${a(t[e+2])}`;case 115:return`${a(t[e+1])}^${a(t[e+2])}`;case 116:return`${a(t[e+1])}<<${a(t[e+2])}`;case 117:return`${a(t[e+1])}>>${a(t[e+2])}`;case 118:return`${a(t[e+1])}>>>${a(t[e+2])}|0`;case 119:return`l.N(${a(t[e+1])},${a(t[e+2])})`;case 120:return`l.D(${a(t[e+1])},${a(t[e+2])})`;case 121:return`l.K(${a(t[e+1])})`;case 122:return`l.Z(${a(t[e+1])})`;case 123:return`l.X(${a(t[e+1])})`;case 124:return`(${a(t[e+1])}+${a(t[e+2])})&0xFFFFFFFFFFFFFFFFn`;case 125:return`(${a(t[e+1])}-${a(t[e+2])})&0xFFFFFFFFFFFFFFFFn`;case 126:return`(${a(t[e+1])}*${a(t[e+2])})&0xFFFFFFFFFFFFFFFFn`;case 127:return`${a(t[e+1])}/${a(t[e+2])}&0xFFFFFFFFFFFFFFFFn`;case 128:return`${a(t[e+1])}/${a(t[e+2])}`;case 129:return`${a(t[e+1])}%${a(t[e+2])}&0xFFFFFFFFFFFFFFFFn`;case 130:return`${a(t[e+1])}%${a(t[e+2])}`;case 131:return`${a(t[e+1])}&${a(t[e+2])}`;case 132:return`${a(t[e+1])}|${a(t[e+2])}`;case 133:return`${a(t[e+1])}^${a(t[e+2])}`;case 134:return`${a(t[e+1])}<<${a(t[e+2])}&0xFFFFFFFFFFFFFFFFn`;case 135:return`l.d(${a(t[e+1])})>>${a(t[e+2])}&0xFFFFFFFFFFFFFFFFn`;case 136:return`${a(t[e+1])}>>${a(t[e+2])}`;case 137:return`l.V(${a(t[e+1])},${a(t[e+2])})`;case 138:return`l.j(${a(t[e+1])},${a(t[e+2])})`;case 139:case 153:return`Math.abs(${a(t[e+1])})`;case 140:case 154:return`-${a(t[e+1])}`;case 141:case 155:return`Math.ceil(${a(t[e+1])})`;case 142:case 156:return`Math.floor(${a(t[e+1])})`;case 143:case 157:return`Math.trunc(${a(t[e+1])})`;case 144:case 158:return`Math.round(${a(t[e+1])})`;case 145:case 159:return`Math.sqrt(${a(t[e+1])})`;case 146:case 160:return`${a(t[e+1])}+${a(t[e+2])}`;case 147:case 161:return`${a(t[e+1])}-${a(t[e+2])}`;case 148:case 162:return`${a(t[e+1])}*${a(t[e+2])}`;case 149:case 163:return`${a(t[e+1])}/${a(t[e+2])}`;case 150:case 164:return`Math.min(${a(t[e+1])},${a(t[e+2])})`;case 151:case 165:return`Math.max(${a(t[e+1])},${a(t[e+2])})`;case 152:case 166:return`l.U(${a(t[e+1])},${a(t[e+2])})`;case 167:return`Number(${a(t[e+1])}&0xFFFFFFFFn)|0`;case 168:case 169:case 170:case 171:return`Math.trunc(${a(t[e+1])})|0`;case 172:return`BigInt(${a(t[e+1])})`;case 173:return`BigInt(${a(t[e+1])}>>>0)`;case 174:case 175:case 176:case 177:return`BigInt(Math.trunc(${a(t[e+1])}))&0xFFFFFFFFFFFFFFFFn`;case 180:case 181:case 186:case 185:return`Number(${a(t[e+1])})`;case 188:return`l.H(${a(t[e+1])})`;case 189:return`l.W(${a(t[e+1])})`;case 190:return`l.Q(${a(t[e+1])})`;case 191:return`l.q(${a(t[e+1])})`;case 192:return`${a(t[e+1])}<<24>>24`;case 193:return`${a(t[e+1])}<<16>>16`;case 194:return`l.Y(${a(t[e+1])})`;case 195:return`l.ee(${a(t[e+1])})`;case 196:return`l.te(${a(t[e+1])})`;case 208:return"null";case 209:return`${a(t[e+1])}===null`;case 210:return`F(${t[e+1]})`;case 64512:return`l.P(${a(t[e+1])})`;case 64513:return`l.O(${a(t[e+1])})`;case 64514:return`l.P(${a(t[e+1])})`;case 64515:return`l.O(${a(t[e+1])})`;case 64516:return`l.I(${a(t[e+1])})`;case 64517:return`l.T(${a(t[e+1])})`;case 64518:return`l.I(${a(t[e+1])})`;case 64519:return`l.T(${a(t[e+1])})`;case 64520:return`l.B(d[${t[e+4]}],c.${"l"+t[e+5]},${a(t[e+1])},${a(t[e+2])},${a(t[e+3])})`;case 64521:return`d[${t[e+1]}]=new Uint8Array`;case 64522:return`l.B(c.${"l"+t[e+4]},c.${"l"+t[e+5]},${a(t[e+1])},${a(t[e+2])},${a(t[e+3])})`;case 64523:return`c.${"l"+t[e+4]}.fill(${a(t[e+1])},T=${a(t[e+2])},T+${a(t[e+3])})`;case 64524:return`l.z(${S(t[e+4])},e[${t[e+5]}],${a(t[e+1])},${a(t[e+2])},${a(t[e+3])})`;case 64525:return`e[${t[e+1]}]=[]`;case 64526:return`l.z(${S(t[e+4])},${S(t[e+5])},${a(t[e+1])},${a(t[e+2])},${a(t[e+3])})`;case 64527:return`l.w(${S(t[e+3])},${a(t[e+1])},${a(t[e+2])})`;case 64528:return S(t[e+1])+".length";case 64529:return`l.ne(${S(t[e+4])},${a(t[e+1])},${a(t[e+2])},${a(t[e+3])})`;default:throw"Internal error"}},F=(e,s)=>{let r=i;return t[r]=e,i+=s,r},o=(e,s=g)=>{P.push(i),t[i++]=e|65536|s<<24,t[i++]=-s},f=()=>{d(),ne(0),m[m.length-1].a=!0},d=(e=!1)=>{let s=[],r=P.length-1,h=H=>{let ee=t[H],j=ee&65535,se=ee>>16&255,oe=j>=40&&j<=62||j>=64520&&j<=64523;for(let Z=se-1;r>=0&&Z>=0;Z--){let G=-t[H+Z+1],ue=!1;for(let V=r;V>=0;V--){let $e=P[V];if($e===null)continue;let Re=t[$e],ce=Re&65535;if(oe&&(ce<65||ce>66)&&ce!=32)break;if(Re>>>24===G){P[V]=null,ue||(r=V-1),t[H+Z+1]=h($e);break}if(ce!==243&&ce!==244)break;ue=!0}}return tt(t,M,F,H)},w;for(;r>=0;){let H=r--;(w=P[H])!==null&&(P[H]=h(w))}let q;for(r=P.length-1,e&&(r>=0&&(w=P[r])!==null&&t[w]>>>24===g?(q=_(w),r--):q="s"+g,g--);r>=0;)if((w=P[r--])!==null){let H=t[w]>>>24;s.push(`${H?$(H)+"=":""}${_(w)};`)}return k+=s.reverse().join(""),M.length=0,P.length=0,i=0,q},{re:I,o:R,R:K,k:Me,de:pe,E:ie}=A,[We,qe]=ie[Me[U]],[Ne,De,Ve]=K[U],re=[],ze=We.length;for(let e=0;e<ze;e++)re.push("a"+e);let _e=["L","T"];for(let[e,s]of Ne)for(let r=0;r<e;r++){let h="l"+_e.length;re.push(h),_e.push(h+(s===126?"=0n":"=0"))}let ae=256,ye=e=>{let s=m.length<ae;s?k+=`b${m.length}:`:m.length===ae&&(k+=`L=1;b${m.length}:for(;;){switch(L){case 1:`,be=2);let r=s?-1:be++,h=s?-1:e!==0?be++:0,[w,q]=N();return m.push({c:w,a:!1,F:e,$:r,_:h,r:g-w,s:q}),h},ne=(e=m.length-B()-1)=>{if(m[m.length-1].a)return;let s=m[e];if(e)if(s.F===1){if(g>s.r+s.c)for(let r=1;r<=s.c;r++)k+=`s${s.r+r}=s${g-s.c+r};`;k+=e<ae?`continue b${e};`:`L=${s._};continue;`}else{if(g>s.r+s.s)for(let r=1;r<=s.s;r++)k+=`s${s.r+r}=s${g-s.s+r};`;k+=e<=ae?`break b${e};`:`L=${s.$};continue;`}else if(s.s===1)k+=`return s${g};`;else if(s.s>1){let r=[];for(let h=s.s-1;h>=0;h--)r.push("s"+(g-h));k+=`return[${r}];`}else k+="return;"},m=[{c:0,a:!1,F:0,$:-1,_:-1,r:0,s:qe.length}],we=e=>{let s=Ye[e]|0;if(!(s&8))return!1;if(s&8)if(m[m.length-1].a)s&32&&I[W++]&64&&B(),s&16&&B();else{let r=s&3;if(s&1024&&(P.push(i),t[i++]=66|g+1<<24,t[i++]=M.length,M.push(63n),P.push(i),t[i++]=131|2<<16|g<<24,t[i++]=-g,t[i++]=-(g+1)),g-=r,s&384)for(let h=0;h<r;h++)o(s&128?243:244,g+h+1);if(!(s&512)){let h=0;s&32&&I[W++]&64&&(h=B()),P.push(i),s&4&&(e|=g+1<<24),t[i++]=e|r<<16;for(let w=1;w<=r;w++)t[i++]=-(g+w);s&16&&(t[i++]=B()),s&32&&(t[i++]=h)}s&4&&g++,s&64&&o(242)}return!0},g=0,W=De,be=0,k="b0:{";for(;W<Ve;){let e=I[W++];if(!we(e))switch(e){case 0:{let s=m[m.length-1];d(),s.a||(k+='"unreachable"();',s.a=!0);break}case 2:d(),ye(0)<0&&(k+="{");break;case 3:{d();let s=ye(1);k+=s<0?"for(;;){":`case ${s}:`;break}case 4:{m[m.length-1].a||o(m.length<ae?240:241);let s=d(!0),r=ye(2);k+=r<0?`if(${s}){`:`if(${s}){L=${r};continue}`;break}case 5:{d();let s=m.length-1,r=m[s];ne(s),k+=s<ae?"}else{":`case ${r._}:`,r.F=0,g=r.r+r.c,r.a=!1;break}case 11:{d();let s=m.length-1,r=m[s];r.F!==2&&(r._=0),r.F=0,ne(s),s<ae?k+="}":(r._&&(k+=`case ${r._}:`),k+=`case ${r.$}:`,s==ae&&(k+="}break}")),g=r.r+r.s,m.pop();break}case 12:d(),ne(),m[m.length-1].a=!0;break;case 13:{m[m.length-1].a||o(240);let s=d(!0);k+=`if(${s}){`,ne(),k+="}";break}case 14:{let s=d(!0);k+=`switch(${s}){`;for(let r=0,h=B();r<h;r++)k+=`case ${r}:`,ne();k+="default:",ne(),k+="}",m[m.length-1].a=!0;break}case 15:f();break;case 16:case 18:{let s=e===18,r=B();if(!m[m.length-1].a){let[h,w]=l[r];g-=h.length,P.push(i),w.length===1&&(e|=g+1<<24),t[i++]=e|h.length<<16;for(let q=1;q<=h.length;q++)t[i++]=-(g+q);t[i++]=r,w.length>1&&(t[i++]=g+1),g+=w.length}s&&f();break}case 17:case 19:{let s=e===19,r=B(),h=B();if(!m[m.length-1].a){let[w,q]=ie[r];g-=w.length+1,P.push(i),q.length===1&&(e|=g+1<<24),t[i++]=e|w.length<<16,t[i++]=-(g+w.length+1);for(let H=1;H<=w.length;H++)t[i++]=-(g+H);t[i++]=h,t[i++]=r,q.length>1&&(t[i++]=g+1),g+=q.length}s&&f();break}case 27:case 28:{if(e===28){let s=B();if(s!==1)throw Error("Unsupported select type count "+s);W++}m[m.length-1].a||(o(240),g-=2,P.push(i),t[i++]=e|3<<16|g<<24,t[i++]=-(g+2),t[i++]=-g,t[i++]=-(g+1));break}case 65:m[m.length-1].a?D():(P.push(i),t[i++]=e|++g<<24,t[i++]=D());break;case 66:m[m.length-1].a?J():(P.push(i),t[i++]=e|++g<<24,t[i++]=M.length,M.push(J()));break;case 67:m[m.length-1].a||(P.push(i),t[i++]=e|++g<<24,t[i++]=W),W+=4;break;case 68:m[m.length-1].a||(P.push(i),t[i++]=e|++g<<24,t[i++]=W),W+=8;break;case 208:W++,m[m.length-1].a||(P.push(i),t[i++]=e|++g<<24);break;case 252:if(e=64512|I[W++],we(e))continue;switch(e){case 64520:{let s=B(),r=B();m[m.length-1].a||(g-=2,P.push(i),t[i++]=e|3<<16|g<<24,t[i++]=-g,t[i++]=-(g+1),t[i++]=-(g+2),t[i++]=s,t[i++]=r);break}case 64522:{let s=B(),r=B();m[m.length-1].a||(g-=2,P.push(i),t[i++]=e|3<<16|g<<24,t[i++]=-g,t[i++]=-(g+1),t[i++]=-(g+2),t[i++]=r,t[i++]=s);break}case 64523:{let s=B();m[m.length-1].a||(g-=2,P.push(i),t[i++]=e|3<<16|g<<24,t[i++]=-(g+1),t[i++]=-g,t[i++]=-(g+2),t[i++]=s);break}case 64524:{let s=B(),r=B();m[m.length-1].a||(g-=2,P.push(i),t[i++]=e|3<<16|g<<24,t[i++]=-g,t[i++]=-(g+1),t[i++]=-(g+2),t[i++]=r,t[i++]=s);break}case 64526:{let s=B(),r=B();m[m.length-1].a||(g-=2,P.push(i),t[i++]=e|3<<16|g<<24,t[i++]=-g,t[i++]=-(g+1),t[i++]=-(g+2),t[i++]=s,t[i++]=r);break}default:throw Error("Unsupported instruction: 0xFC "+L(e&255))}break;default:throw Error("Unsupported instruction: "+L(e))}}if(b>255)throw Error("Deep stacks are not supported");let Ce=JSON.stringify("wasm:"+(pe.get(Y)||`function[${U}]`)),je=`return{${Ce}(${re.slice(0,ze)}){var ${_e};${k}}}[${Ce}]`;return new Function("f","F","c","t","d","e","g","l",je)(n,u,T,y,O,p,x,te)};var de=class{valueOf(){return this.value}},at=(n,l)=>{let[u,y]=n,O=[],p=[];for(let T=0;T<u.length;T++)O.push("a"+T),p.push(Fe("a"+T,u[T]));let x=`f(${p})`;if(y.length===1)x="return "+fe(x,y[0]);else if(y.length>1){x=`let r=${x};`;for(let T=0;T<y.length;T++)x+=`r[${T}]=${fe(`r[${T}]`,y[T])};`;x+="return r"}return new Function("f","l",`return(${O})=>{${x}}`)(l,te)},le=class{constructor(l,u){let y=Pe.get(l),{R:O,_e:p,ue:x,ce:T,k:A,Fe:U,ge:Y,fe:B,me:D,he:J,E:N}=y,t=this.exports=Object.create(null),P=[],i=[],M=[],b=[],$=[],v={},S=[],C=a=>{let _=v[a]||(v[a]={xe:a,ie:M[a],p:null,x:(...F)=>{let o=i[a](...F);return _.x=i[a],o}});return _};for(let a of Y){let[_,F,o,f]=a,d=u[_][F];if(o===0){let I=N[f],R=i.length;i.push((...K)=>(i[R]=at(I,d))(...K)),M.push(I)}else if(o===1)S.push(Be.get(d));else if(o===2)P.push(Te.get(d));else if(o===3)b.push(Oe(d,f)),$.push(f);else throw Error(`Unsupported import type ${L(o)} for "${_}"."${F}"`)}let E={};for(let[a,_]of B)P.push(Te.get(new me({initial:a,maximum:a>_?a:_})));for(let a=0;a<P.length;a++){let _=P[a],F=()=>{E["l"+a]=_.l,E["u"+a]=_.u,E["o"+a]=_.o};E["A"+a]=_,_.L.push(F),F()}for(let[a,_,F]of U)b.push(F(b,C)),$.push(a);let z=[];for(let[a,_,F]of p)_!==null&&(E["l"+a].set(F,_(b)),F=new Uint8Array),z.push(F);for(let a=0;a<O.length;a++){let _=i.length;M.push(N[A[a]]),i.push((...F)=>(i[_]=Qe(i,M,C,S,z,c,b,E,y,a,_))(...F))}let c=[];for(let[a,_,F]of J){if(a!==112&&a!==111)throw Error("Unsupported element type: "+L(a));S.push(Be.get(new he({element:a===111?"externref":"anyfunc",initial:_,maximum:_>F?_:F})))}for(let[a,_,F]of x){let o=[];for(let f of F)o.push(f===null?null:C(f));if(c.push(o),a!==null&&_!==null){let f=S[a],d=_(b);for(let I of o)f[d++]=I}}for(let[a,_,F]of T)if(_===0)t[a]=te.M(C(F));else if(_===1)t[a]=S[F].S;else if(_===2)t[a]=P[F].S;else if(_===3){let o=new de,f=$[F];Object.defineProperty(o,"value",{get:()=>b[F],set:d=>{b[F]=Oe(d,f)}}),t[a]=o}else throw Error(`Unsupported export type ${L(_)} for "${a}"`);D>=0&&i[D]()}},Te=new WeakMap,Ie=n=>Math.max(-1,Math.min(n,65535))|0,me=class{constructor({initial:l,maximum:u}){if(l=Ie(l),u=Ie(u??1/0),l<0||l>u)throw RangeError();let y=new ArrayBuffer(l<<16),O={S:this,f:y,l:new Uint8Array(y),u:new Int8Array(y),o:new DataView(y),g:l,Me:u,L:[],v(p){let x=this.g,T=this.l;if(p=Ie(p),p<0||this.g+p>this.Me)return-1;if(!p)return x;let A=new ArrayBuffer((this.g+=p)<<16),U=new Uint8Array(A);U.set(T);try{structuredClone(this.f,{transfer:[this.f]})}catch{}this.f=A,this.l=U,this.u=new Int8Array(A),this.o=new DataView(A);for(let Y of this.L)Y();return x}};Te.set(this,O),Object.defineProperty(this,"buffer",{get:()=>O.f}),this.grow=p=>{let x=O.v(p);if(x<0)throw RangeError();return x}}},Be=new WeakMap,he=class{constructor({element:l,initial:u,maximum:y}){let O=l=="anyfunc",p=[];if(!O&&l!=="externref")throw TypeError();p.S=this,p.ae=Math.min(4294967295,y??1/0),p.length=u;for(let x=0;x<u;x++)p[x]=null;Be.set(this,p),Object.defineProperty(this,"length",{get:()=>p.length}),this.get=x=>{if(x>>>=0,x>=p.length)throw RangeError();return O?te.M(p[x]):p[x]},this.set=(x,T)=>{if(x>>>=0,x>=p.length)throw RangeError();p[x]=O?te.h(T):T},this.grow=(x,T)=>{let A=te.w(p,O?te.h(T):T,x);if(A<0)throw RangeError();return A}}};var nt=async n=>new X(n),lt=async n=>new X(await(await n).arrayBuffer()),it=async(n,l)=>{if(n instanceof X)return new le(n,l);let u=new X(n);return{module:u,instance:new le(u,l)}},st=async(n,l)=>{let u=new X(await(await n).arrayBuffer());return{module:u,instance:new le(u,l)}},ot=n=>{if(!ArrayBuffer.isView(n)&&!(n instanceof ArrayBuffer))throw TypeError("Invalid buffer source");try{return new X(n),!0}catch{return!1}},Rt={Global:de,Instance:le,compile:nt,compileStreaming:lt,instantiate:it,instantiateStreaming:st,validate:ot,Memory:me,Module:X,Table:he,CompileError:Q};globalThis.WebAssembly = Rt;)lit";
const char *ABBA = R"lit(const abbakeystr='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';function atob(data){if(arguments.length===0){throw new TypeError('1 argument required, but only 0 present.');}data=`${data}`;data=data.replace(/[ \t\n\f\r]/g,'');if(data.length%4===0){data=data.replace(/==?$/,'')}if(data.length%4===1||/[^+/0-9A-Za-z]/.test(data)){throw new DOMException('Failed to decode base64: invalid character','InvalidCharacterError');}let output='';let buffer=0;let accumulatedBits=0;for(let i=0;i<data.length;i++){buffer<<=6;buffer|=atobLookup(data[i]);accumulatedBits+=6;if(accumulatedBits===24){output+=String.fromCharCode((buffer&0xff0000)>>16);output+=String.fromCharCode((buffer&0xff00)>>8);output+=String.fromCharCode(buffer&0xff);buffer=accumulatedBits=0}}if(accumulatedBits===12){buffer>>=4;output+=String.fromCharCode(buffer)}else if(accumulatedBits===18){buffer>>=2;output+=String.fromCharCode((buffer&0xff00)>>8);output+=String.fromCharCode(buffer&0xff)}return output}function atobLookup(chr){const index=abbakeystr.indexOf(chr);return index<0?undefined:index}function btoa(s){if(arguments.length===0){throw new TypeError('1 argument required, but only 0 present.');}let i;s=`${s}`;for(i=0;i<s.length;i++){if(s.charCodeAt(i)>255){throw new DOMException('The string to be encoded contains characters outside of the Latin1 range.','InvalidCharacterError');}}let out='';for(i=0;i<s.length;i+=3){const groupsOfSix=[undefined,undefined,undefined,undefined];groupsOfSix[0]=s.charCodeAt(i)>>2;groupsOfSix[1]=(s.charCodeAt(i)&0x03)<<4;if(s.length>i+1){groupsOfSix[1]|=s.charCodeAt(i+1)>>4;groupsOfSix[2]=(s.charCodeAt(i+1)&0x0f)<<2}if(s.length>i+2){groupsOfSix[2]|=s.charCodeAt(i+2)>>6;groupsOfSix[3]=s.charCodeAt(i+2)&0x3f}for(let j=0;j<groupsOfSix.length;j++){if(typeof groupsOfSix[j]==='undefined'){out+='='}else{out+=btoaLookup(groupsOfSix[j])}}}return out}function btoaLookup(index){if(index>=0&&index<64){return abbakeystr[index]}return undefined}globalThis.atob=atob;globalThis.btoa=btoa;)lit";

static inline void loadExtendLibraries(JSContext *ctx) {
    JS_FreeValue(ctx, JS_Eval(ctx, DATE_POLYFILL, strlen(DATE_POLYFILL), "date-polyfill.js", JS_EVAL_TYPE_GLOBAL));
    JS_FreeValue(ctx, JS_Eval(ctx, CONSOLE, strlen(CONSOLE), "console.js", JS_EVAL_TYPE_GLOBAL));
    JS_FreeValue(ctx, JS_Eval(ctx, POLYWASM, strlen(POLYWASM), "polywasm.js", JS_EVAL_TYPE_GLOBAL));
    JS_FreeValue(ctx, JS_Eval(ctx, ABBA, strlen(ABBA), "abba.js", JS_EVAL_TYPE_GLOBAL));
}

#endif //QUICKJS_EXTEND_LIBRARIES
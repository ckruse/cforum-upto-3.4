start = "default"

list "symbols" = "(,),{,},:,[,],;,\,"
list "keywords" = "break,else,new,var,case,finally,return,void,catch,for,switch,while,continue,function,this,with,default,if,throw,delete,in,try,do,instanceof,typeof,true,false"
list "objects" = "plugins,Boolean,embeds,options,window,String,document,frames,forms,applets,all,Function,Number,Math,Screen,mimeTypes,RegExp,links,layers,navigator,Array,history,images,location,elements,style,Date,event,anchors"

list "attributes" = "visibility,keyCode,which,y,appVersion,target,defaultStatus,SQRT1_2,caller,pageXOffset,name,charset,hash,className,availWidth,nodeValue,nextSibling,vlinkColor,recordNumber,title,pageY,parentTextEdit,defaultCharset,layerY,outerHTML,userAgent,offsetHeight,dataSrc,selectedIndex,clip,ctrlKey,pixelDepth,platform,vspace,arity,background,availHeight,height,length,filename,suffixes,sourceIndex,protocol,left,cookieEnabled,href,id,URL,LOG2E,value,cookie,data,outerHeight,dataPageSize,type,SQRT2,defaultValue,POSITIVE_INFINITY,offsetParent,NEGATIVE_INFINITY,locationbar,previousSibling,appName,status,offsetY,zIndex,referrer,pathname,alinkColor,lastChild,innerText,lowsrc,closed,host,border,enabledPlugin,parentElement,screenY,parentLayer,nodeName,clientX,complete,pageX,personalbar,dataFld,hostname,E,bgColor,childNodes,offsetTop,innerHTML,screenX,linkColor,lang,toolbar,description,innerHeight,arguments,attributes,width,isTextEdit,siblingAbove,MAX_VALUE,text,method,NaN,below,outerWidth,language,shiftKey,innerWidth,tagName,MIN_VALUE,statusbar,LN2,firstChild,search,LOG10E,parentNode,pageYOffset,dataFormatAs,modifiers,prototype,offsetX,menubar,src,port,checked,lastModified,nodeType,siblingBelow,outerText,appCodeName,x,LN10,altKey,fgColor,above,offsetWidth,defaultChecked,clientY,scrollbars,layerX,colorDepth,hspace,action,PI"

list "methods" = "sqrt,strike,captureEvents,floor,javaEnabled,print,parseFloat,getTimezoneOffset,load,isNaN,setTimeout,getDay,prompt,open,setDate,getUTCFullYear,small,getMonth,scrollIntoView,getAttribute,fixed,round,toExponential,unshift,cloneNode,moveBy,removeAttribute,getSeconds,setMilliseconds,appendChild,bold,concat,getElementsByTagName,getElementsByName,removeAttributeNode,setUTCHors,sort,click,setHours,substr,slice,encodeURI,UTC,toLowerCase,sup,replaceChild,escape,split,write,exp,setYear,getUTCDate,setSeconds,removeChild,unescape,setUTCDay,sin,handleEvent,contains,push,replace,setAttribute,shift,tan,insertAdjacentHTML,getYear,submit,scrollTo,getUTCMonth,random,setUTCMilliseconds,reload,scrollBy,close,routeEvent,insertAdjacentText,lastIndexOf,toPrecision,substring,select,moveToAbsolute,back,fontcolor,acos,go,big,splice,italics,charCodeAt,clearInterval,getSelection,hasChildNodes,decodeURI,charAt,forward,toUpperCase,setUTCMinutes,fontsize,anchor,encodeURIComponent,atan,getUTCMinutes,getElementById,match,getMilliseconds,writeln,setAttributeNode,join,abs,insertBefore,getUTCSeconds,deleteData,parse,find,setFullYear,appendData,blur,asin,home,alert,resizeBy,getAttributeNode,setUTCSeconds,setUTCFullYear,createAttribute,indexOf,pop,fromCharCodeAt,search,test,setUTCDate,max,moveBelow,getDate,setUTCMonth,blink,getTime,enableExternalCapture,exec,createElement,setTime,reverse,decodeURIComponent,toFixed,ceil,clearTimeout,disableExternalCapture,getUTCDay,focus,releaseEvents,resizeTo,toString,setInterval,replaceData,confirm,getFullYear,cos,reset,moveTo,log,getUTCHours,setMonth,eval,link,toLocaleString,moveAbove,getUTCMilliseconds,stop,getHours,getMinutes,min,setMinutes,play,parseInt,pow,sub,createTextNode,isFinite,toGMTString,insertData"

block "default"
  lineend stay

  onstring "//" "onelinecomment" "comment"
  onstring "/*" "comment" "comment"

  onstringlist "symbols" highlight "symbol"
  onstringlist "keywords" highlight "keyword"

  onstringlist "objects" highlight "known-object"
  onstringlist "attributes" highlight "known-attribute"
  onstringlist "methods" highlight "known-methods"

  onstring "&quot;" "string" "string"
  onstring "'" "sqstring" "string"

  onregexp "^(\\.|\\*=|/=|%=|\\+=|-=|&lt;&lt;=|&gt;(&gt;){1,2}=|&amp;=|^=|\\|=|\\+(\\+)?|--?|~|!|\\*|/|%|&gt;(&gt;){0,2}|(&lt;){1,2}|&lt;=|&gt;=|==|!=|===|!==|(&amp;){1,2}|\\^|\\|{1,2}|\\?|=)" highlight "operator"

  onregexpafter "^[^a-zA-Z0-9]" "^0[0-7]+\\.?[0-7]*" highlight "octnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^0[xX][0-9A-Fa-f]+\\.?[0-9A-Fa-f]*" highlight "hexnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^[0-9]+\\.?[0-9]*" highlight "number"
end

block "comment"
  lineend stay
  onstring "*/" pop
end

block "onelinecomment"
  lineend pop
end

block "string"
  lineend stay

  onstring "\\\\" highlight "escaped"
  onstring "\\&quot;" highlight "escaped"

  onregexp "^\\\\u[0-9a-fA-F]{4}" highlight "escaped"

  onregexp "^\\\\[0-7]{1,3}" highlight "escaped"
  onregexp "^\\\\x[0-9A-Fa-f]{1,2}" highlight "escaped"
  onregexp "^\\\\[btnvfr]" highlight "escaped"

  onstring "&quot;" pop
end

block "sqstring"
  lineend stay

  onstring "\\\\" highlight "escaped"
  onstring "\\'" highlight "escaped"

  onstring "'" pop
end

# eof

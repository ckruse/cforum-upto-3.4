# \file   sql.pat
# \author Daniela Koller, <dako@wwwtech.de>
#
# Pattern file for SQL
#

# Patternfile muss immer mit einem start = "block" anfangen.
start = "default"

list "word_operators" = "all,and,any,between,exists,in,like,not,or,some"

list "std_function"      = "avg,count,min,max,sum,current_date,current_time,current_timestamp,current_user,session_user,system_user,bit_length_char_length,extract,octet_length,position,concatenate,convert,lower,substring,translate,trim,upper"
list "mssql_function"    = "abs,acos,app_name,ascii,asin,atan,atn2,binary_checksum,cast,ceiling,char,charindex,checksum,checksum_avg,coalesce,col_length,col_name,contains,containsable,convert,cos,cot,count_big,current_timestamp,current_user,datalength,databasepropertyex,dateadd,datediff,datename,datepart,day,db_id,db_name,degrees,difference,exp,floor,file_id,file_name,filegroup_id,filegroup_name,filegroupproperty,fileproperty,fulltextcatalog,fulltextservice,formatmessage,freetexttable,getdate,geansinull,getutcdate,grouping,host_id,host_name,identincr,ident_seed,ident_current,identity,index_col,indexproperty,isdate,is_member,is_srvrolemember,isnull,isnumeric,left,len,log,log10,lower,ltrim,max,min,month,nchar,newid,nullif,object_id,object_name,objectproperty"
list "mysql_function"    = "abs,acos,ascii,asin,atan,atan2,avg,benchmark,binary,bin,bit_count,bit_and,bit_or,cas,when,then,else,end,ceiling,char,coalesce,concat,concat_ws,connection_id,conv,cos,cot,count,current_date,curdate,curtime,current_time,database,date_add,date_sub,adddate,subdate,date_format,dayname,dayofmonth,dayofweek,dayofyear,decode,degrees,elt,encode,encrypt,exp,export_set,field,find_in_set,floor,format,from_days,from_unixtime,get_lock,greatest,hex,interval,hour,ifnull,isnull,insert,instr,last_insert_id,lcase,lower,least,left,length,octet_length,char_length,char_lengt,load_file,locate,position,locate,log,log10,lpad,ltrim,make_set,md5,min,max,minute,mod,month,monthname,now,sysdate,current_timestamp,nullif,oct,ord,password,period_add,period_diff,pi,pow,power,quarter,radians,rand,release_lock,repeat,replace,reverse,right,round,rpad,rtrim,sec_to_time,second,sign,sin,soundex,space,sqrt,std,stddev,strcmp,substring,substring,mid,substring_index,substring,sum,tan,time_format,time_to_sec,to_days,trim,truncate,ucase,upper,unix_timestamp,user,system_user,session_user,version,week,weekday,year,yearweek"
list "oracle_function"   = "abs,acos,add_months,ascii,asin,atan,atan2,avg,bfilename,ceil,chartorowid,chr,concat,convert,corr,cos,cosh,count,covar_pop,covar_samp,cume_dist,decode,dense_rank,deref,dump,exp,first_value,over,floor,greatest,grouping,hextoraw,initcap,instr,instrb,lag,last_day,last_value,lead,least,length,lengthb,ln,log,lower,lpad,ltrim,make_ref,max,min,mod,month_between,new_time,next_day,nls_charset_decl_len,nls_charset_id,nls_charset_name,nls_initcap,nls_lower,nlssort,nls_upper_string,ntile,numtodsinterval,numtoyminterval,nvl,nvl2,percent_rank,power,rank,ratio_to_report,rawtohex,ref,reftohex,regr_xxx,replace,round,row_number,rowidtochar,rpad,rtrim,sign,sin,sinh,soundex,sqrt,stddev,stdev_pop,seddev_samp,substr,substrb,sum,sys_context,sys_guid,sysdate,tan,tanh,to_char,to_date,to_lob,to_multi_byte,to_number,to_single_byte,translate,trim,trunc,uid,upper,user,userenv,value,car_pop,var_samp,over,varance,vsize"
list "postgres_function" = "abstime,abs,acos,age,area,asin,atan,atan2,box,broadcast,case,when,then,else,end,cbrt,center,char,char_length,character_length,circle,coalesce,cos,date_part,date_trunc,degrees,diameter,exp,float,float3,height,host,initcap,interval,integer,isclosed,isopen,isfinite,length,ln,log,lower,lseg,lpad,ltrim,masklen,netmask,npoint,nullif,octet_length,path,pclose,pi,polygon,point,position,pow,popen,reltime,radians,radius,round,rpad,rtrim,sin,sqrt,substring,substr,tan,text,textpos,timestamp,to_char,to_date,to_number,to_timestamp,translate,trim,trunc,upper,carchar,width"

list "std_keywords"      = "by,absolut,action,add,admin,after,aggregate,alias,all,allocate,alter,any,are,array,as,asc,assertion,at,authorization,before,begin,binary,bit,blob,call,cascade,cascaded,case,cast,catalog,char,character,check,class,clob,close,collate,collation,column,commit,completion,condition,connect,connection,constraint,constraints,constructor,contains,continue,corresponding,create,cross,cube,current,current_date,current_path,current_role,current_time,current_timestamp,current_user,cursor,cycle,data,datalink,date,day,deallocate,dec,decimal,declare,default,defferable,delete,depth,deref,desc,descriptor,diagnostics,dictionary,disconnect,do,domain,double,drop,end-exec,equals,escape,except,exception,execute,exit,expand,expanding,false,first,float,for,foreign,free,from,function,general,get,global,goto,group,grouping,handler,hash,hour,identity,if,ignore,immediate,in,indicator,initialize,initially,inner,inout,intersect,interval,into,is,isolation,iterate,join,key,language,large,last,lateral,leading,leave,left,less,level,like,limit,local,localtime,localtimestamp,locator,loop,match,meets,minute,modifies,modify,module,month,names,national,natural,nchar,nclob,new,next,no,none,normalize,not,null,numeric,object,of,off,old,on,only,open,operation,option,or,order,ordinality,out,outer,output,pad,parameter,parameters,partial,path,period,postfix,precedes,precision,prefix,preorder,prepare,preserve,primary,prior,privileges,procedure,public,read,reads,real,recursive,redo,ref,references,referencing,relative,repeat,resignal,restrict,result,return,returns,revoke,right,role,rollback,rollup,routine,row,rows,savepoint,schema,scroll,search,second,section,select,sequence,session,session_user,set,sets,signal,size,smallint,specific,specifictype,sql,sqlexception,sqlstate,sqlwarning,start,state,static,structure,succeeds,sum,system_user,table,temporary,terminate,than,then,time,timestamp,timezone_hour,timezone_minute,to,trailing,transaction,translation,treat,trigger,true,under,undo,union,unique,unknown,until,update,usage,user,using,value,values,variable,varying,view,when,whenever,where,while,with,write,year,zone"
list "mssql_keywords"    = "add,all,alter,any,as,asc,authorzation,backup,begin,between,break,browse,bulk,by,cascade,case,check,checkpoint,close,clustered,coalesce,collate,column,commit,compute,constraint,containscontainstable,continue,convert,create,cross,current,current_date,current_time,current_timestamp,current_user,cursor,database,dbcc,deallocate,declare,default,distinct,distributed,double,drop,dummy,dump,else,end,errlvl,except,exec,execute,fillfactor,for,foreign,freetext,freetexttable,from,full,function,goto,grant,group,having,holdlock,identity,identity_insert,if,in,index,inner,insert,intersect,into,is,join,key,kill,left,like,lineno,load,national,nocheck,nonclustered,null,nullif,of,offset,on,open,opendatasource,openquery,openrowset,openxml,option,or,order,outer,percent,plan,precision,primary,print,proc,readtext,reconfigure,references,replication,restore,restrict,return,revoke,right,rollback,rowcount,rowguid-col,rule,save,schema,select,session_user,set,setuser,shutdown,some,statistics,system_user,table,textsize,then,to,top,tran,transaction,trigger,truncate,tsequal,union,unique,update,updatetext,use,user,values,varying,view,waitfor,when,where,while,with,writetext"
list "mysql_keywords"    = "action,add,after,aggregate,all,alter,as,asc,auto_increment,avg,avg_row_length,between,bigint,binary,bit,blob,bool,both,by,cascade,case,change,char,character,check,checksum,column,columns,comment,constraint,create,cross,current_date,current_time,current_timestamp,data,database,databases,date,datetime,day,day_hour,day_minute,day_second,dayofmonth,dayofweek,dayofyear,dec,decimal,default,delay_key_write,delayed,delete,desc,describe,distinct,distinctrow,double,drop,else,enclosed,end,enum,escape,escaped,exists,explain,fields,file,first,float,float4,float8,flush,for,foreign,from,full,function,global,grant,grants,group,having,heap,high_priority,hosts,hour,hour_minute,hour_second,identified,if,ignore,in,index,infile,inner,insert,insert_id,int,int1,int2,int3,int4,int8,integer,interval,into,is,isam,join,key,keys,kill,klast_insert_id,leading,left,length,like,limit,lines,load,local,lock,logs,long,longblob,longtext,low_priority,match,max,max_rows,mediumblob,mediumint,mediumtext,middleint,min_rows,minute,minute_second,modify,month,monthname,myisam,natural,no,not,null,numeric,on,optimize,option,optionally,or,order,outer,outfile,pack_keys,partial,password,precisiion,primary,privileges,procedure,process,processlist,read,real,references,regexp,reload,rename,replace,restrict,returns,revoke,rlike,row,rows,second,select,set,show,shutdown,smallint,soname,sql_big_result,sql_big_selects,sql_big_tables,sql_log_off,sql_log_update,sql_low_priority_updates,sql_select_limit,sql_small_result,sql_warnings,starting,status,straight_join,string,sql_small_result,tables,temporary,terminated,text,then,time,timestamp,tinyblob,tinyint,tinytext,to,trailing,type,unique,unlock,unsigned,update,usage,use,using,values,varbinary,varchar,variables,varying,when,with,write,zerofill"
list "oracle_keywords"   = "access,add,all,alter,any,array,as,acs,audit,authid,avg,begin,between,binary,body,boolean,bulk,by,char,char_base,check,close,cluster,collect,column,comment,commit,compress,connect,constant,create,current,currval,cursor,date,day,declare,decimal,default,delete,desc,distinct,do,drop,else,elsif,end,exception,exclusive,execute,exists,exit,extends,false,fetch,file,float,for,forall,from,function,goto,grant,group,having,heap,hour,identified,if,immediate,in,increment,index,indicator,initial,insert,integer,interface,intersect,interval,into,is,isolation,java,level,like,limited,lock,long,loop,max,maxextends,min,minus,minute,mlslabel,mod,mode,modify,month,natural,natualn,new,not,nowait,null,number,number_base,ocirowid,of,offline,on,online,opaque,open,operator,option,or,order,organization,others,out,package,partition,pctfree,pls_integer,positive,privileges,procedure,public,raise,range,raw,real,record,ref,release,rename,resource,return,reverse,revoke,rollback,row,rows,rowid,rowlabel,rownum,rowtype,savepoint,second,select,seperate,session,set,share,size,smallint,space,sql,sqlcode,sqlerrm,start,stddev,subtiype,successful,sum,synonym,sysdate,table,then,time,timestamp,to,trigger,true,type,uid,union,unique,update,use,user,validate,values,varchar,charchar2,variance,view,when,whenever,where,while,with,work,write,year,zone"
list "postgres_keywords" = "abort,add,all,allocate,alter,analyze,any,are,as,asc,assertion,at,authorization,avg,begin,between,binary,bit,bit_length,both,by,cascade,cascaded,case,cast,catalog,char,char_length,character,character_length,check,close,cluster,coalesce,collate,collation,column,commit,connect,connection,constraint,continue,convert,copy,corresponding,count,create,cross,current,current_date,current_session,current_time,current_timestamp,current_user,cursor,date,deallocate,dec,decimal,declare,default,delete,desc,describe,descriptor,diagnostics,disconnect,distinct,do,domain,drop,else,end,escape,except,exception,exec,execute,exists,explain,extend,external,extract,false,fetch,first,float,for,foreign,found,from,full,get,global,go,goto,grant,group,having,identity,in,indicator,inner,input,insert,intersect,interval,into,is,join,last,leading,left,like,listen,load,local,lock,lower,max,min,module,move,names,national,natural,nchar,new,no,none,not,notify,null,nullif,numeric,octet_length,offset,on,open,or,order,outer,output,overlaps,partial,position,precision,procedure,public,references,reset,revoke,right,rollback,rows,schema,section,select,session,session_user,set,setof,show,size,some,sql,sqlcode,sqlerror,sqlstate,substring,substring,system_user,table,temporary,then,to,trailing,transaction,translate,translation,trim,true,union,unique,unknown,unlisten,until,update,upper,usage,user,using,vacuum,value,values,varchar,varying,verbose,view,when,whenever,where,with,work,write"

block "default"
  lineend stay

  onstring "--" "onelinecomment"   "comment"
  onstring "/*" "multilinecomment" "comment"

  onstring "'" "qstring" "string"
  onstring "\"" "string" "prop-string-quoted"
  onstring "`" "qop" "prop-quoted"

  onstringlist "word_operators" highlight "operator"
  onregexp "^(\\+|-|\\*|\\\\|=|\\(|\\)|%|\b_\b|,|@|\\.|&amp;|\\||\\^|~|!=|!&lt;|!&gt;|&lt;|&gt;|&lt;&gt;|&lt;=|&gt;=|@@|\\|\\|)" highlight "operator"

  # Funktionen
  onstringlist "std_function"      highlight "function"
  onstringlist "mssql_function"    highlight "prop-function"
  onstringlist "mysql_function"    highlight "prop-function"
  onstringlist "postgres_function" highlight "prop-function"
  onstringlist "oracle_function"   highlight "prop-function"

  # Keywords
  onstringlist "std_keywords"      highlight "keyword"
  onstringlist "mssql_keywords"    highlight "prop-keyword"
  onstringlist "mysql_keywords"    highlight "prop-keyword"
  onstringlist "postgres_keywords" highlight "prop-keyword"
  onstringlist "oracle_keywords"   highlight "prop-keyword"
end

block "string"
  lineend stay
  onstring "\\"" highlight "prop-escaped"

  onstring "'%" highlight "escaped"
  onstring "'_" highlight "escaped"

  onstring "\\%" highlight "prop-escaped"
  onstring "\\_" highlight "prop-escaped"

  onstring "%" highlight "operator"
  onstring "_" highlight "operator"
  onstring "\"" pop
end

block "qop"
  lineend stay
  onstring "\\`" highlight "prop-escaped"
  onstring "`" pop
end

block "onelinecomment"
  lineend pop
end

block "multilinecomment"   
  lineend stay
  onstring "*/" pop
end

block "qstring"
  onstring "''" highlight "prop-escaped"
  onstring "\\'" highlight "prop-escaped"

  onstring "'%" highlight "escaped"
  onstring "'_" highlight "escaped"

  onstring "\\%" highlight "prop-escaped"
  onstring "\\_" highlight "prop-escaped"

  onstring "%" highlight "operator"
  onstring "_" highlight "operator"

  onstring "'" pop
end

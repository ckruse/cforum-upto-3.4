start = "default"

list "symbols" = ".,\,,;,(,),[,]"
list "word_ops" = "and,div,mod,not,or,xor,shr,shl,in,as"
list "keywords" = "absolute,begin,break,case,const,constructor,continue,destructor,do,downto,else,end,file,for,function,goto,if,implementation,inherited,inline,interface,label,nil,object,of,on,operator,packed,procedure,program,record,repeat,self,set,then,to,type,unit,until,uses,var,while,with,dispose,exit,false,true,new,absolute,abstract,alias,assembler,cdecl,default,export,external,far,far16,forward,fpccall,index,name,near,override,pascal,popstack,private,protected,public,published,read,register,safecall,saveregisters,softfloat,stdcall,virtual,write"
list "datatypes" = "integer,real,char,boolean,byte,text,string,array"

block "default"
  lineend stay

  onstring "(*" "bs-comment" "comment"
  onstring "{$" "compiler" "compiler-instruction"
  onstring "{" "b-comment" "comment"

  onstringlist "symbols" highlight "symbol"
  onstringlist "keywords" highlight "keyword"
  onstringlist "word_ops" highlight "operator"
  onstringlist "datatypes" highlight "datatype"

  onregexp "^(\\+|-|\\*|/|=|&lt;&gt;|&lt;|&gt;|&lt;=|&gt;=|@|\\^|\\.\\.|:=)" highlight "operator"

  onregexp "^#[0-9]+" highlight "escaped"
  onregexpafter "^[^a-zA-Z0-9]" "^-?[0-9]+\\.?[0-9]*" highlight "number"
  onregexpafter "^[^a-zA-Z0-9]" "^\\$-?[0-9a-fA-F]+\\.?[0-9a-fA-F]*" highlight "hexnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^%-?[10]+\\.?[10]*" highlight "binnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^&amp;-?[0-7]+\\.?[0-7]*" highlight "octnumber"

  onstring "'" "string" "string"
  onstring "asm" "asm" "asm"
end

block "compiler"
  onstring "}" pop
end

block "asm"
  onstring "end" pop
end

block "string"
  onstring "''" highlight "escaped"
  onstring "'" pop
end

block "bs-comment"
  lineend stay

  onstring "(*" "bs-comment" "comment"
  onstring "{" "b-comment" "comment"
  onstring "*)" pop
end

block "b-comment"
  lineend stay
  onstring "{" "b-comment" "comment"
  onstring "(*" "bs-comment" "comment"
  onstring "}" pop
end

# eof
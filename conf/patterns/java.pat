start = "default"

list "symbols" = "(,),{,},[,],;,\,"
list "keywords" = "abstract,default,if,private,this,boolean,do,implements,protected,throw,break,double,import,public,throws,byte,else,instanceof,return,transient,case,extends,int,short,try,catch,final,interface,static,void,char,finally,long,strictfp,volatile,class,float,native,super,while,const,for,new,switch,continue,goto,package,synchronized,null,true,false,enum"

block "default"
  lineend stay

  onstringlist "symbols" highlight "symbol"
  onstringlist "keywords" highlight "keyword"

  onregexp "^(=|&gt;|&lt;|!|~|\\?|:|\\.|==|&lt;=|&gt;=|!=|&amp;&amp;|\\|\\||\\+\\+|--|\\+|-|\\*|/|&amp;|\\||\\^|%|&lt;&lt;|&gt;&gt;|&gt;&gt;&gt;|\\+=|-=|\\*=|/=|&amp;=|\\|=|\\^=|%=|&lt;&lt;=|&gt;&gt;=|&gt;&gt;&gt;=)" highlight "operator"

  onstring "&quot;" "string" "string"
  onstring "'" "sqstring" "string"

  onstring "//" "onelinecomment" "comment"
  onstring "/*" "comment" "comment"

  onregexpafter "^[^a-zA-Z0-9]" "^0[0-7](\\.[0-7]*|[0-7]+)" highlight "octnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^0[xX][0-9A-Fa-f](\\.[0-9A-Fa-f]+|[0-9A-Fa-f]*)" highlight "hexnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^[0-9](\\.[0-9]+|[0-9]*)" highlight "number"
end

block "onelinecomment"
  lineend pop
end

block "comment"
  onstring "*/" pop
end

block "string"
  onstring "\\\\" highlight "escaped"
  onstring "\\&quot;" highlight "escaped"

  onregexp "^\\\\u[0-9a-fA-F]{4}" highlight "escaped"

  onregexp "^\\\\[0-7]{1,3}" highlight "escaped"
  onregexp "^\\\\x[0-9A-Fa-f]{1,2}" highlight "escaped"
  onregexp "^\\\\[bfnrt']" highlight "escaped"

  onstring "&quot;" pop
end

block "sqstring"
  onstring "\\\\" highlight "escaped"
  onstring "\\'" highlight "escaped"

  onstring "'" pop
end

# eof
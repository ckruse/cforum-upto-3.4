start = "default"

list "symbols" = ";,(,),{,},:,[,],\,"
list "keywords" = "auto,continue,enum,if,short,switch,volatile,break,default,extern,int,signed,typedef,while,case,do,float,long,sizeof,union,char,double,for,register,static,unsigned,const,else,goto,return,struct,void"
list "makros" = "__FILE__,__LINE__,NULL"

block "default"
  # syntax: lineend <neuer-block>
  # spezielle bloecke: stay (hier bleiben), pop (verlassen)
  lineend stay

  onstring "//" "onelinecomment" "comment"
  onstring "/*" "comment" "comment"

  onstring "&quot;" "string" "string"
  onregexp "^'(\\\\'|.)'" highlight "string"

  onregexp "^#\\s*(define|if|ifdef|else|ifndef|endif)" "preprocessor" "preprocessor"

  onstringlist "keywords" highlight "keyword"
  onregexp "^(\\+=|-=|/=|%=|&gt;&gt;=|&amp;=|&lt;&lt;=|-&gt;|\\.|!|~|\\+{1,2}|-{1,2}|\\*|(&amp;){1,2}|\\|{1,2}|\\?|\\^|/|%|&gt;=|&lt;=|==|!=|=|(&gt;){1,2}|(&lt;){1,2})" highlight "operator"
  onstringlist "symbols" highlight "symbol"
  onstringlist "makros" highlight "makro"

  onregexpafter "^[^a-zA-Z0-9]" "^0[0-7]+\\.?[0-7]*" highlight "octnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^0[xX][0-9A-Fa-f]+\\.?[0-9A-Fa-f]*" highlight "hexnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^[0-9]+\\.?[0-9]*" highlight "number"
end

block "preprocessor"
  lineend pop
end

block "onelinecomment"
  lineend pop
end

block "comment"
  lineend stay
  onstring "*/" pop
end

block "string"
  lineend stay

  onstring "\\\\" highlight "escaped"
  onstring "\\&quot;" highlight "escaped"

  onregexp "^\\\\[0-7]{1,3}" highlight "escaped"
  onregexp "^\\\\x[0-9A-Fa-f]{1,2}" highlight "escaped"
  onregexp "^\\\\[btnvfr$]" highlight "escaped"
  onregexp "^\\$+[a-zA-Z_][a-zA-Z0-9_]*(\\[[a-zA-Z0-9_]*\\])*" highlight "variable"
  onregexp "^\\$\\{+[a-zA-Z_][a-zA-Z0-9_]*(\\[[a-zA-Z0-9_]*\\])*\\}" highlight "variable"
  # TODO: da muss noch {$a->b}, {$a['b']}, {$a[\"b\"]} etc. rein...
  onregexp "^\\{\\$+[a-zA-Z_][a-zA-Z0-9_]*(\\[[a-zA-Z0-9_]*\\])*\\}" highlight "variable"

  onstring "&quot;" pop
end

block "sqstring"
  lineend stay

  onstring "\\\\" highlight "escaped"
  onstring "\\'" highlight "escaped"

  onstring "'" pop
end

# eof

start = "default"

list "symbols" = "(,),{,},:,[,],;,\,"
list "keywords" = "break,else,new,var,case,finally,return,void,catch,for,switch,while,continue,function,this,with,default,if,throw,delete,in,try,do,instanceof,typeof"

block "default"
  lineend stay

  onstring "//" "onelinecomment" "comment"
  onstring "/*" "comment" "comment"

  onstringlist "symbols" highlight "symbol"
  onstringlist "keywords" highlight "keyword"

  onstring "&quot;" "string" "string"
  onstring "'" "sqstring" "string"

  onregexp "^(\\.|\\*=|/=|%=|\\+=|-=|&lt;&lt;=|&gt;(&gt;){1,2}=|&amp;=|^=|\\|=|\\+(\\+)?|--?|~|!|\\*|/|%|&gt;(&gt;){0,2}|(&lt;){1,2}|&lt;=|&gt;=|==|!=|===|!==|(&amp;){1,2}|\\^|\\|{1,2}|\\?|=)" highlight "operator"

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

  onregexp "^\\\\[0-7]{1,3}" highlight "escaped"
  onregexp "^\\\\x[0-9A-Fa-f]{1,2}" highlight "escaped"
  onregexp "^\\\\[nrt]" highlight "escaped"

  onstring "&quot;" pop
end

block "sqstring"
  lineend stay

  onstring "\\\\" highlight "escaped"
  onstring "\\'" highlight "escaped"

  onstring "'" pop
end

# eof

# Pattern file for perl
#
start = "default"

list "symbols" = "(,),{,},:,[,],;,\,"
list "keywords" = "and,del,for,is,raise,assert,elif,from,lambda,return,break,else,global,not,try,class,except,if,or,while,continue,exec,import,pass,yield,def,finally,in,print"

block "default"
  lineend stay

  onstring "#" "onelinecomment" "comment"

  onregexp "^(ru?|ur?)?'''" "longstring" "string"
  onregexp "^(ru?|ur?)?&quot;&quot;&quot;" "longstring-dq" "string"

  onregexp "^(ru?|ur?)?'" "shortstring" "string"
  onregexp "^(ru?|ur?)?&quot;" "shortstring-dq" "string"

  onstringlist "keywords" highlight "keyword"
  onstringlist "symbols" highlight "symbol"

  onregexp "^(\\.|=|\\+|-|\\*\\*?|//?|%|&lt;&lt;|&gt;&gt;|&amp;|\\||\\^|~|&lt;|&gt;|&lt;=|&gt;=|==|!=|&lt;&gt;)" highlight "operator"

  onregexpafter "^[^a-zA-Z0-9]" "^0[0-7]+\\.?[0-7]*" highlight "octnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^0[xX][0-9A-Fa-f]+\\.?[0-9A-Fa-f]*" highlight "hexnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^[0-9]+\\.?[0-9]*" highlight "number"
end

block "onelinecomment"
  lineend pop
end

block "longstring"
  lineend stay

  onstring "\\\\" highlight "escaped"
  onstring "\\'" highlight "escaped"

  onregexp "^\\\\[0-7]{1,3}" highlight "escaped"
  onregexp "^\\\\x[0-9A-Fa-f]{1,2}" highlight "escaped"
  onregexp "^\\\\[btnvfr$]" highlight "escaped"
  onregexp "^\\\\u{4}" highlight "escaped"
  onregexp "^\\\\U{8}" highlight "escaped"

  onstring "'" pop
end

block "longstring-dq"
  lineend stay

  onstring "\\\\" highlight "escaped"
  onstring "\\&quot;" highlight "escaped"

  onregexp "^\\\\[0-7]{1,3}" highlight "escaped"
  onregexp "^\\\\x[0-9A-Fa-f]{1,2}" highlight "escaped"
  onregexp "^\\\\[btnvfr$]" highlight "escaped"
  onregexp "^\\\\u{4}" highlight "escaped"
  onregexp "^\\\\U{8}" highlight "escaped"

  onstring "&quot;" pop
end

block "shortstring"
  lineend pop

  onstring "\\\\" highlight "escaped"
  onstring "\\'" highlight "escaped"

  onregexp "^\\\\[0-7]{1,3}" highlight "escaped"
  onregexp "^\\\\x[0-9A-Fa-f]{1,2}" highlight "escaped"
  onregexp "^\\\\[btnvfr$]" highlight "escaped"
  onregexp "^\\\\u{4}" highlight "escaped"
  onregexp "^\\\\U{8}" highlight "escaped"

  onstring "'" pop
end

block "shortstring-dq"
  lineend pop

  onstring "\\\\" highlight "escaped"
  onstring "\\&quot;" highlight "escaped"

  onregexp "^\\\\[0-7]{1,3}" highlight "escaped"
  onregexp "^\\\\x[0-9A-Fa-f]{1,2}" highlight "escaped"
  onregexp "^\\\\[btnvfr$]" highlight "escaped"
  onregexp "^\\\\u{4}" highlight "escaped"
  onregexp "^\\\\U{8}" highlight "escaped"

  onstring "&quot;" pop
end

# eof

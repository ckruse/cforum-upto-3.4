start = "default"

list "symbols" = "(,),\,,;,@,$"
list "keywords" = "BEGIN,class,ensure,nil,self,when,END,def,false,not,super,while,alias,defined,for,or,then,yield,and,do,if,redo,true,begin,else,in,rescue,undef,break,elsif,module,retry,unless,case,end, next,return,until,require,include,loop"
list "makros" = "self,nil,true,false,__FILE__,__LINE__"

block "default"
  lineend stay

  onregexp "^(\\.|=|::|\\[|\\]|\\*\\*|-|\\+|!|~|\\*|/|%|&lt;&lt;|&gt;&gt;|&amp;|\\||\\^|&gt;|&gt;=|&lt;|&lt;=|&lt;=&gt;|==|===|!=|=~|!~|&amp;&amp;|\\|\\||\\.\\.|\\.\\.\\.|\\+=|-=|\\*=|%=|/=|&lt;&lt;=|&gt;&gt;=|\\|=|&amp;=|\\^=|\\*\\*=)" highlight "operator"

  onstring "&quot;" "string" "string"
  onstring "'" "sqstring" "string"
  onstring "`" "exec" "string"

  ostring "#!" "onelinecomment" "shebang"
  onstring "#" "onelinecomment" "comment"

  onstringlist "symbols" highlight "symbol"
  onstringlist "keywords" highlight "keyword"
  onstringlist "makros" highlight "makro"

  onregexpafter "^[^a-zA-Z0-9]" "^0[0-7]\\.?[0-7]*" highlight "octnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^0[xX][0-9A-Fa-f]\\.?[0-9A-Fa-f]*" highlight "hexnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^[0-9]\\.?[0-9]*" highlight "number"

  onregexp_backref "^&lt;&lt;(`|'|&quot;)?(\\w+)\\1" "heredoc" 2 "string"
end

block "onelinecomment"
  lineend pop
end

block "heredoc"
  lineend stay

  onstring "\\\\" highlight "escaped"

  onregexp "^\\\\[0-7]{1,3}" highlight "escaped"
  onregexp "^\\\\x[0-9A-Fa-f]{1,2}" highlight "escaped"
  onregexp "^\\\\[btnvfr$]" highlight "escaped"

  onstring "<br>$$<br>" pop
  onstring "<br />$$<br />" pop
end

block "exec"
  lineend stay

  onstring "\\\\" highlight "escaped"
  onstring "\\`" highlight "escaped"

  onregexp "^\\\\[0-7]{1,3}" highlight "escaped"
  onregexp "^\\\\x[0-9A-Fa-f]{1,2}" highlight "escaped"
  onregexp "^\\\\[btnvfr$]" highlight "escaped"

  onstring "`" pop
end

block "string"
  lineend stay

  onstring "\\\\" highlight "escaped"
  onstring "\\&quot;" highlight "escaped"

  onregexp "^\\\\[0-7]{1,3}" highlight "escaped"
  onregexp "^\\\\x[0-9A-Fa-f]{1,2}" highlight "escaped"
  onregexp "^\\\\[btnvfr$]" highlight "escaped"

  onstring "&quot;" pop
end

block "sqstring"
  lineend stay

  onstring "\\\\" highlight "escaped"
  onstring "\\'" highlight "escaped"

  onstring "'" pop
end

# eof
start = "default"

list "symbols" = ";,(,),{,},:,[,],\,"
list "operators" = "or,xor,and"
list "keywords" = "as,case,if,else,elseif,while,do,for,foreach,break,continue,switch,declare,return,require,include,require_once,include_once,var,class,new,function,default,null,false,true,echo,extends"
list "makros" = "__FILE__,__LINE__,SID,__FUNCTION__,__CLASS__,__METHOD__,PHP_VERSION,PHP_OS,DEFAULT_INCLUDE_PATH,PEAR_INSTALL_DIR,PEAR_EXTENSION_DIR,PHP_EXTENSION_DIR,PHP_BINDIR,PHP_LIBDIR,PHP_DATADIR,PHP_SYSCONFDIR,PHP_LOCALSTATEDIR,PHP_CONFIG_FILE_PATH,PHP_OUTPUT_HANDLER_START,PHP_OUTPUT_HANDLER_CONT,PHP_OUTPUT_HANDLER_END,E_ERROR,E_WARNING,E_PARSE,E_NOTICE,E_CORE_ERROR,E_CORE_WARNING,E_COMPILE_ERROR,E_COMPILE_WARNING,E_USER_ERROR,E_USER_WARNING,E_USER_NOTICE,E_ALL"

block "default"
  # syntax: lineend <neuer-block>
  # spezielle bloecke: stay (hier bleiben), pop (verlassen)
  lineend stay

  onregexp "^&lt;\\?(php)?" highlight "operator"
  onregexp "^\\?&gt;" highlight "operator"

  # syntax: onstring <zeichenkette> <neuer-block> <span-klasse>
  # spezielle bloecke: stay (hier bleiben), pop (verlassen, 3. param nicht notwendig),
  #                    highlight (nur string einzeln highlighten)
  onstring "#" "onelinecomment" "comment"
  onstring "//" "onelinecomment" "comment"
  onstring "/*" "comment" "comment"
  onstring "&quot;" "string" "string"
  onstring "'" "sqstring" "string"

  # syntax: onregexp_backref <pattern> <block> <backreference number> <span-klasse>
  onregexp_backref "^&lt;&lt;&lt;(\\w+)" "heredoc" 1 "heredoc"

  # syntax: onstrings <listen-name> <neuer-block> <span-klasse>
  onstringlist "keywords" highlight "keyword"
  onstringlist "operators" highlight "operator"
  onregexp "^(=|\\+=|-=|\\*=|/=|\\.=|%=|&amp;=|\\|=|^=|~=|&lt;|&gt;|\\|\\||&amp;&amp;|\\||\\^|&amp;|==|!=|===|!==|&lt;|&lt;=|&gt;|&gt;=|=&gt;|&lt;&lt;|&gt;&gt;|\\.|\\*|/|%|!|~|\\+\\+|--|-&gt;|-|\\+|\\?)" highlight "operator"
  onstringlist "symbols" highlight "symbol"
  onstringlist "makros" highlight "makro"

  # syntax: onregexp <regexp> <neuer-block> <span-klasse>
  onregexp "^\\$+[a-zA-Z_][a-zA-Z0-9_]*" highlight "variable"
  
  # syntax onregexpafter <vorher-regexp> <regexp-zu-matchen> <neuer-block> <span-klasse>
  # vorher-regexp wird auf das zeichen davor angewandt
  onregexpafter "^[^a-zA-Z0-9]" "^0[0-7](\\.[0-7]*|[0-7]+)" highlight "octnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^0[xX][0-9A-Fa-f](\\.[0-9A-Fa-f]+|[0-9A-Fa-f]*)" highlight "hexnumber"
  onregexpafter "^[^a-zA-Z0-9]" "^[0-9](\\.[0-9]+|[0-9]*)" highlight "number"
end

block "heredoc"
  lineend stay
  # TODO: highlighting von code
  onstring "<br>$$<br>" pop
  onstring "<br />$$<br />" pop
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
  onregexp "^\\\\[nrt$]" highlight "escaped"
  onregexp "^\\$+[a-zA-Z_][a-zA-Z0-9_]*(\\[[a-zA-Z0-9_]*\\])*" highlight "variable"
  onregexp "^\\$\\{+[a-zA-Z_][a-zA-Z0-9_]*(\\[[a-zA-Z0-9_]*\\])*\\}" highlight "variable"
  # TODO: da muss noch {$a->b}, {$a['b']}, {$a[\"b\"]} etc. rein...
  onregexp "^\\{\\$+[a-zA-Z_][a-zA-Z0-9_]*(\\[[a-zA-Z0-9_]*\\])*\\}" highlight "variable"

  onstring "&quot;" pop
end

block "sqstring"
  lineend stay

  onstring "\\\\\\\\" highlight "escaped"
  onstring "\\\\'" highlight "escaped"

  onstring "'" pop
end

# eof

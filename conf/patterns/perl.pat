# Pattern file for perl
#
start = "default"

list "symbols" = "(,),{,},:,[,]"
list "operators" = "->,++,--,**,!,~,and,+,-,=~,!~,*,/,%,x,+,-,.,<<,>>,<,>,<=,>=,lt,gt,le,ge,==,!=,<=>,eq,ne,cmp,&,|,^,&&,||,..,...,=,+=,-=,*=,/=,**=,^=,\,,=>,not,and,or,xor"
list "keywords" = "continue,do,else,elsif,for,foreach,goto,if,last,my,next,package,return,sub,switch,unless,until,use,while,print,split,require,pack,hex,open,close,opendir,closedir,readdir,chomp,chop,exit,vars"
list "makros" = "__PACKAGE__,SUPER,BEGIN,CHECK,INIT,END,DESTROY"

block "default"
  # syntax: lineend <neuer-block>
  # spezielle bloecke: stay (hier bleiben), pop (verlassen)
  lineend stay

  # syntax: onstring <zeichenkette> <neuer-block> <span-klasse>
  # spezielle bloecke: stay (hier bleiben), pop (verlassen, 3. param nicht notwendig),
  #                    highlight (nur string einzeln highlighten)
  onstring "#!" "onelinecomment" "shebang"
  onstring "#" "onelinecomment" "comment"

  onstring "&quot;" "string" "string"
  onstring "'" "sqstring" "string"

  onregexp_backref "<<(\\w+)" "heredoc" 1 "heredoc"

  # TODO: qr,qx,regexps, etc

  # syntax: onstringlist <listen-name> <neuer-block> <span-klasse>
  onstringlist "keywords" highlight "keyword"
  onstringlist "operators" highlight "operator"
  onstringlist "symbols" highlight "symbol"
  onstringlist "makros" highlight "makro"

  # syntax: onregexp <regexp> <neuer-block> <span-klasse>
  onregexp "\\$+[a-zA-Z_][a-zA-Z0-9_]*" highlight "variable"
  
  # syntax onregexpafter <vorher-regexp> <regexp-zu-matchen> <neuer-block> <span-klasse>
  # vorher-regexp wird auf das zeichen davor angewandt
  onregexpafter "[^a-zA-Z0-9]" "0[0-7]+" highlight "octnumber"
  onregexpafter "[^a-zA-Z0-9]" "0[xX][0-9A-Fa-f]+" highlight "hexnumber"
  onregexpafter "[^a-zA-Z0-9]" "[0-9]+" highlight "number"
end

block "onelinecomment"
  lineend pop
end

block "heredoc"
  lineend stay

  # TODO: code highlighting

  onregexp "^$$" pop
end

block "string"
  lineend stay

  onstring "\\\\\\\\" highlight "escaped"
  onstring "\\\\\&quot;" highlight "escaped"

  onregexp "abc" highlight "escaped"
#  onregexp "\\\\[0-7]{1,3}" highlight "escaped"
#  onregexp "\\\\x[0-9A-Fa-f]{1,2}" highlight "escaped"
#  onregexp "\\\\[nrt$]" highlight "escaped"
#  onregexp "\\$+[a-zA-Z_][a-zA-Z0-9_]*(\\[[a-zA-Z0-9_]*\\])*" highlight "variable"
#  onregexp "\\$\\{+[a-zA-Z_][a-zA-Z0-9_]*(\\[[a-zA-Z0-9_]*\\])*\}" highlight "variable"

  onstring "&quot;" pop
end

block "sqstring"
  lineend stay

  onstring "\\\\\\\\" highlight "escaped"
  onstring "\\\\'" highlight "escaped"

  onstring "'" pop
end

# eof

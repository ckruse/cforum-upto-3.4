start = "default"

block "default"
  lineend stay

  onregexp "^&lt;[A-Za-z][A-Za-z_0-9:-]*\\s*/&gt;" highlight "empty-tag"
  onstring "&lt;?" "pi" "processing-instruction"
  onstring "&lt;!" "specialtag" "tag"
  onstring "&lt;/" "ctag" "tag"
  onstring "&lt;" "tag" "tag"

  onregexp "^&amp;#[0-9]+;" highlight "entity"
  onregexp "^&amp;#x[0-9A-Fa-f]+;" highlight "entity"
  onregexp "^(&amp;|%)[A-Za-z0-9_:]+;" highlight "entity"
  onstring "&amp;" highlight "entityinvalid"
end

block "pi"
  lineend stay

  onregexp_start "^[A-Za-z][A-Za-z_0-9:-]*" highlight "name"
  onregexp "^[A-Za-z-]" "tagattr" "attribute"
  onstring "?&gt;" pop
  onstring "&gt;" pop
end

block "tag"
  lineend stay
  onregexp_start "^[A-Za-z][A-Za-z_0-9:-]*" highlight "name"
  onregexp "^[A-Za-z-]" "tagattr" "attribute"
  onstring "&gt;" pop
end

block "ctag"
  onregexp_start "^[A-Za-z][A-Za-z_0-9:-]*" highlight "name"
  onstring "&gt;" pop
end

block "specialtag"
  onstring "--" "scomment" "comment"
  onstring "&gt;" pop
end

block "tagattr"
  lineend pop
  onregexp "^\\s" pop
  onstring "&gt;" pop 2
  onregexpafter_backref "^=" "^(&quot;|')" "tagqattrvalue" 1 "value"
  onregexpafter "^=" "^." "tagattrvalue" "value"
  onstring "=" highlight "equal"
end

block "tagattrvalue"
  lineend pop 2
  onregexp "^\\s" pop 2
  onstring "&gt;" pop 3

  onregexp "^&amp;#[0-9]+;" highlight "entity"
  onregexp "^&amp;#x[0-9A-Fa-f]+;" highlight "entity"
  onregexp "^(&amp;|%)[A-Za-z0-9_:]+;" highlight "entity"
  onstring "&amp;" highlight "entityinvalid"
end

block "tagqattrvalue"
  lineend stay

  onregexp "^&amp;#[0-9]+;" highlight "entity"
  onregexp "^&amp;#x[0-9A-Fa-f]+;" highlight "entity"
  onregexp "^(&amp;|%)[A-Za-z0-9_:]+;" highlight "entity"
  onstring "&amp;" highlight "entityinvalid"
  onstring "$$" pop 2
end

block "scomment"
  lineend stay
  onstring "--" pop
end

# eof

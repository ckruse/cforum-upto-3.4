start = "default"

block "default"
  lineend stay

  onregexp "^&lt;[A-Za-z][A-Za-z_0-9:-]*\\s*/&gt;" highlight "empty-tag"
  onstring "&lt;!--" "comment" "comment"
  onstring "&lt;!" "specialtag" "tag"
  onstring "&lt;/" "ctag" "tag"
  onstring "&lt;" "tag" "tag"

  onregexp "^&amp;(amp|lt|gt|quot|nbsp|iexcl|cent|pound|curren|yen|brvbar|sect|uml|copy|ordf|laquo|not|shy|reg|macr|deg|plusmn|sup2|sup3|acute|micro|para|middot|cedil|sup1|ordm|raquo|frac14|frac12|frac34|iquest|Agrave|Aacute|Acirc|Atilde|Auml|Aring|AElig|Ccedil|Egrave|Eacute|Ecirc|Euml|Igrave|Iacute|Icirc|Iuml|ETH|Ntilde|Ograve|Oacute|Ocirc|Otilde|Ouml|times|Oslash|Ugrave|Uacute|Ucirc|Uuml|Yacute|THORN|szlig|agrave|aacute|acirc|atilde|auml|aring|aelig|ccedil|egrave|eacute|ecirc|euml|igrave|iacute|icirc|iuml|eth|ntilde|ograve|oacute|ocirc|otilde|ouml|divide|oslash|ugrave|uacute|ucirc|uuml|yacute|thorn|yuml|Alpha|alpha|Beta|beta|Gamma|gamma|Delta|delta|Epsilon|epsilon|Zeta|zeta|Eta|eta|Theta|theta|Iota|iota|Kappa|kappa|Lambda|lambda|Mu|mu|Nu|nu|Xi|xi|Omicron|omicron|Pi|pi|Rho|rho|Sigma|sigmaf|sigma|Tau|tau|Upsilon|upsilon|Phi|phi|Chi|chi|Psi|psi|Omega|omega|thetasym|upsih|piv|forall|part|exist|empty|nabla|isin|notin|ni|prod|sum|minus|lowast|radic|prop|infin|ang|and|or|cap|cup|int|there4|sim|cong|asymp|ne|equiv|le|ge|sub|sup|nsub|sube|supe|oplus|otimes|perp|sdot|loz|lceil|rceil|lfloor|rfloor|lang|rang|larr|uarr|rarr|darr|harr|crarr|lArr|uArr|rArr|dArr|hArr|bull|hellip|prime|oline|frasl|weierp|image|real|trade|euro|alefsym|spades|clubs|hearts|diams|ensp|emsp|thinsp|zwnj|zwj|lrm|rlm|ndash|mdash|lsquo|rsquo|sbquo|ldquo|rdquo|bdquo|dagger|Dagger|permil|lsaquo|rsaquo);" highlight "entity"

  onregexp "^&amp;#[0-9]+;" highlight "entity"
  onregexp "^&amp;#x[0-9A-Fa-f]+;" highlight "entity"
  onregexp "^&amp;[A-Za-z0-9]{1,8};" highlight "entityinvalid"
  onstring "&amp;" highlight "entityinvalid"
end

block "tag"
  lineend stay
  onregexp_start "^[A-Za-z][A-Za-z_0-9]*" highlight "name"
  onregexp "^[A-Za-z]" "tagattr" "attribute"
  onstring "&gt;" pop
end

block "ctag"
  onregexp_start "^[A-Za-z][A-Za-z_0-9-]*" highlight "name"
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

  onregexp "^&amp;(amp|lt|gt|quot|nbsp|iexcl|cent|pound|curren|yen|brvbar|sect|uml|copy|ordf|laquo|not|shy|reg|macr|deg|plusmn|sup2|sup3|acute|micro|para|middot|cedil|sup1|ordm|raquo|frac14|frac12|frac34|iquest|Agrave|Aacute|Acirc|Atilde|Auml|Aring|AElig|Ccedil|Egrave|Eacute|Ecirc|Euml|Igrave|Iacute|Icirc|Iuml|ETH|Ntilde|Ograve|Oacute|Ocirc|Otilde|Ouml|times|Oslash|Ugrave|Uacute|Ucirc|Uuml|Yacute|THORN|szlig|agrave|aacute|acirc|atilde|auml|aring|aelig|ccedil|egrave|eacute|ecirc|euml|igrave|iacute|icirc|iuml|eth|ntilde|ograve|oacute|ocirc|otilde|ouml|divide|oslash|ugrave|uacute|ucirc|uuml|yacute|thorn|yuml|Alpha|alpha|Beta|beta|Gamma|gamma|Delta|delta|Epsilon|epsilon|Zeta|zeta|Eta|eta|Theta|theta|Iota|iota|Kappa|kappa|Lambda|lambda|Mu|mu|Nu|nu|Xi|xi|Omicron|omicron|Pi|pi|Rho|rho|Sigma|sigmaf|sigma|Tau|tau|Upsilon|upsilon|Phi|phi|Chi|chi|Psi|psi|Omega|omega|thetasym|upsih|piv|forall|part|exist|empty|nabla|isin|notin|ni|prod|sum|minus|lowast|radic|prop|infin|ang|and|or|cap|cup|int|there4|sim|cong|asymp|ne|equiv|le|ge|sub|sup|nsub|sube|supe|oplus|otimes|perp|sdot|loz|lceil|rceil|lfloor|rfloor|lang|rang|larr|uarr|rarr|darr|harr|crarr|lArr|uArr|rArr|dArr|hArr|bull|hellip|prime|oline|frasl|weierp|image|real|trade|euro|alefsym|spades|clubs|hearts|diams|ensp|emsp|thinsp|zwnj|zwj|lrm|rlm|ndash|mdash|lsquo|rsquo|sbquo|ldquo|rdquo|bdquo|dagger|Dagger|permil|lsaquo|rsaquo);" highlight "entity"

  onregexp "^&amp;#[0-9]+;" highlight "entity"
  onregexp "^&amp;#x[0-9A-Fa-f]+;" highlight "entity"
  onregexp "^&amp;[A-Za-z0-9]{1,8};" highlight "entityinvalid"
  onstring "&amp;" highlight "entityinvalid"
end

block "tagqattrvalue"
  lineend stay

  onregexp "^&amp;(amp|lt|gt|quot|nbsp|iexcl|cent|pound|curren|yen|brvbar|sect|uml|copy|ordf|laquo|not|shy|reg|macr|deg|plusmn|sup2|sup3|acute|micro|para|middot|cedil|sup1|ordm|raquo|frac14|frac12|frac34|iquest|Agrave|Aacute|Acirc|Atilde|Auml|Aring|AElig|Ccedil|Egrave|Eacute|Ecirc|Euml|Igrave|Iacute|Icirc|Iuml|ETH|Ntilde|Ograve|Oacute|Ocirc|Otilde|Ouml|times|Oslash|Ugrave|Uacute|Ucirc|Uuml|Yacute|THORN|szlig|agrave|aacute|acirc|atilde|auml|aring|aelig|ccedil|egrave|eacute|ecirc|euml|igrave|iacute|icirc|iuml|eth|ntilde|ograve|oacute|ocirc|otilde|ouml|divide|oslash|ugrave|uacute|ucirc|uuml|yacute|thorn|yuml|Alpha|alpha|Beta|beta|Gamma|gamma|Delta|delta|Epsilon|epsilon|Zeta|zeta|Eta|eta|Theta|theta|Iota|iota|Kappa|kappa|Lambda|lambda|Mu|mu|Nu|nu|Xi|xi|Omicron|omicron|Pi|pi|Rho|rho|Sigma|sigmaf|sigma|Tau|tau|Upsilon|upsilon|Phi|phi|Chi|chi|Psi|psi|Omega|omega|thetasym|upsih|piv|forall|part|exist|empty|nabla|isin|notin|ni|prod|sum|minus|lowast|radic|prop|infin|ang|and|or|cap|cup|int|there4|sim|cong|asymp|ne|equiv|le|ge|sub|sup|nsub|sube|supe|oplus|otimes|perp|sdot|loz|lceil|rceil|lfloor|rfloor|lang|rang|larr|uarr|rarr|darr|harr|crarr|lArr|uArr|rArr|dArr|hArr|bull|hellip|prime|oline|frasl|weierp|image|real|trade|euro|alefsym|spades|clubs|hearts|diams|ensp|emsp|thinsp|zwnj|zwj|lrm|rlm|ndash|mdash|lsquo|rsquo|sbquo|ldquo|rdquo|bdquo|dagger|Dagger|permil|lsaquo|rsaquo);" highlight "entity"

  onregexp "^&amp;#[0-9]+;" highlight "entity"
  onregexp "^&amp;#x[0-9A-Fa-f]+;" highlight "entity"
  onregexp "^&amp;[A-Za-z0-9]{1,8};" highlight "entityinvalid"
  onstring "&amp;" highlight "entityinvalid"
  onstring "$$" pop 2
end

block "comment"
  lineend stay
  onstring "--&gt;" pop
end

block "scomment"
  lineend stay
  onstring "--" pop
end

# eof

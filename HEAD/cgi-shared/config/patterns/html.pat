list "entities" = &amp;,&lt;,&gt;,&quot;,&nbsp;,&iexcl;,&cent;,&pound;,&curren;,&yen;,&brvbar;,&sect;,&uml;,&copy;,&ordf;,&laquo;,&not;,&shy;,&reg;,&macr;,&deg;,&plusmn;,&sup2;,&sup3;,&acute;,&micro;,&para;,&middot;,&cedil;,&sup1;,&ordm;,&raquo;,&frac14;,&frac12;,&frac34;,&iquest;,&Agrave;,&Aacute;,&Acirc;,&Atilde;,&Auml;,&Aring;,&AElig;,&Ccedil;,&Egrave;,&Eacute;,&Ecirc;,&Euml;,&Igrave;,&Iacute;,&Icirc;,&Iuml;,&ETH;,&Ntilde;,&Ograve;,&Oacute;,&Ocirc;,&Otilde;,&Ouml;,&times;,&Oslash;,&Ugrave;,&Uacute;,&Ucirc;,&Uuml;,&Yacute;,&THORN;,&szlig;,&agrave;,&aacute;,&acirc;,&atilde;,&auml;,&aring;,&aelig;,&ccedil;,&egrave;,&eacute;,&ecirc;,&euml;,&igrave;,&iacute;,&icirc;,&iuml;,&eth;,&ntilde;,&ograve;,&oacute;,&ocirc;,&otilde;,&ouml;,&divide;,&oslash;,&ugrave;,&uacute;,&ucirc;,&uuml;,&yacute;,&thorn;,&yuml;,&Alpha;,&alpha;,&Beta;,&beta;,&Gamma;,&gamma;,&Delta;,&delta;,&Epsilon;,&epsilon;,&Zeta;,&zeta;,&Eta;,&eta;,&Theta;,&theta;,&Iota;,&iota;,&Kappa;,&kappa;,&Lambda;,&lambda;,&Mu;,&mu;,&Nu;,&nu;,&Xi;,&xi;,&Omicron;,&omicron;,&Pi;,&pi;,&Rho;,&rho;,&Sigma;,&sigmaf;,&sigma;,&Tau;,&tau;,&Upsilon;,&upsilon;,&Phi;,&phi;,&Chi;,&chi;,&Psi;,&psi;,&Omega;,&omega;,&thetasym;,&upsih;,&piv;,&forall;,&part;,&exist;,&empty;,&nabla;,&isin;,&notin;,&ni;,&prod;,&sum;,&minus;,&lowast;,&radic;,&prop;,&infin;,&ang;,&and;,&or;,&cap;,&cup;,&int;,&there4;,&sim;,&cong;,&asymp;,&ne;,&equiv;,&le;,&ge;,&sub;,&sup;,&nsub;,&sube;,&supe;,&oplus;,&otimes;,&perp;,&sdot;,&loz;,&lceil;,&rceil;,&lfloor;,&rfloor;,&lang;,&rang;,&larr;,&uarr;,&rarr;,&darr;,&harr;,&crarr;,&lArr;,&uArr;,&rArr;,&dArr;,&hArr;,&bull;,&hellip;,&prime;,&oline;,&frasl;,&weierp;,&image;,&real;,&trade;,&euro;,&alefsym;,&spades;,&clubs;,&hearts;,&diams;,&ensp;,&emsp;,&thinsp;,&zwnj;,&zwj;,&lrm;,&rlm;,&ndash;,&mdash;,&lsquo;,&rsquo;,&sbquo;,&ldquo;,&rdquo;,&bdquo;,&dagger;,&Dagger;,&permil;,&lsaquo;,&rsaquo;

block "default"
  lineend stay
  onstring "<!--" "comment" "comment"
  onstring "<!" "specialtag" "tag"
  onstring "</" "ctag" "tag"
  onstring "<" "tag" "tag"
  onstrings "entities" highlight "entity"
  onregexp "&#[0-9]+;" highlight "entity"
  onregexp "&#x[0-9A-Fa-f]+;" highlight "entity"
  onregexp "&[A-Za-z0-9]{1,8};" highlight "entityinvalid"
  onstring "&" highlight "entityinvalid"
end

block "tag"
  lineend stay
  onregexpafter "<" "[A-Za-z][A-Za-z_0-9]+" highlight "name"
  onregexpafter "\s" "[A-Za-z]" "tagattr" "attribute"
  onstring ">" pop
end

block "ctag"
  onregexpafter "<" "[A-Za-z][A-Za-z_0-9-]+" highlight "name"
  onstring ">" pop
end

block "specialtag"
  onstring "--" "scomment" "comment"
  onstring ">" pop
end

block "tagattr"
  lineend pop
  onstring "\s" pop
  onstring ">" pop 2
  onregexpafter_backref "=" "([\"'])" "tagqattrvalue" 1 "value"
  onregexpafter "=" "." "tagattrvalue" "value"
  onstring "=" highlight "equal"
end

block "tagattrvalue"
  lineend pop 2
  onstring "\s" pop 2
  onstring ">" pop 3
  onstrings "entities" highlight "entity"
  onregexp "&#[0-9]+;" highlight "entity"
  onregexp "&#x[0-9A-Fa-f]+;" highlight "entity"
  onregexp "&[A-Za-z0-9]{1,8};" highlight "entityinvalid"
  onstring "&" highlight "entityinvalid"
end

block "tagqattrvalue"
  lineend stay
  onstrings "entities" highlight "entity"
  onregexp "&#[0-9]+;" highlight "entity"
  onregexp "&#x[0-9A-Fa-f]+;" highlight "entity"
  onregexp "&[A-Za-z0-9]{1,8};" highlight "entityinvalid"
  onstring "&" highlight "entityinvalid"
  onstring "$$" pop 2
end

block "comment"
  lineend stay
  onstring "-->" pop
end

block "scomment"
  lineend stay
  onstring "--" pop
end

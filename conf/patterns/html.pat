start = "default"

list "entities" = "&amp;amp;,&amp;lt;,&amp;gt;,&amp;quot;,&amp;nbsp;,&amp;iexcl;,&amp;cent;,&amp;pound;,&amp;curren;,&amp;yen;,&amp;brvbar;,&amp;sect;,&amp;uml;,&amp;copy;,&amp;ordf;,&amp;laquo;,&amp;not;,&amp;shy;,&amp;reg;,&amp;macr;,&amp;deg;,&amp;plusmn;,&amp;sup2;,&amp;sup3;,&amp;acute;,&amp;micro;,&amp;para;,&amp;middot;,&amp;cedil;,&amp;sup1;,&amp;ordm;,&amp;raquo;,&amp;frac14;,&amp;frac12;,&amp;frac34;,&amp;iquest;,&amp;Agrave;,&amp;Aacute;,&amp;Acirc;,&amp;Atilde;,&amp;Auml;,&amp;Aring;,&amp;AElig;,&amp;Ccedil;,&amp;Egrave;,&amp;Eacute;,&amp;Ecirc;,&amp;Euml;,&amp;Igrave;,&amp;Iacute;,&amp;Icirc;,&amp;Iuml;,&amp;ETH;,&amp;Ntilde;,&amp;Ograve;,&amp;Oacute;,&amp;Ocirc;,&amp;Otilde;,&amp;Ouml;,&amp;times;,&amp;Oslash;,&amp;Ugrave;,&amp;Uacute;,&amp;Ucirc;,&amp;Uuml;,&amp;Yacute;,&amp;THORN;,&amp;szlig;,&amp;agrave;,&amp;aacute;,&amp;acirc;,&amp;atilde;,&amp;auml;,&amp;aring;,&amp;aelig;,&amp;ccedil;,&amp;egrave;,&amp;eacute;,&amp;ecirc;,&amp;euml;,&amp;igrave;,&amp;iacute;,&amp;icirc;,&amp;iuml;,&amp;eth;,&amp;ntilde;,&amp;ograve;,&amp;oacute;,&amp;ocirc;,&amp;otilde;,&amp;ouml;,&amp;divide;,&amp;oslash;,&amp;ugrave;,&amp;uacute;,&amp;ucirc;,&amp;uuml;,&amp;yacute;,&amp;thorn;,&amp;yuml;,&amp;Alpha;,&amp;alpha;,&amp;Beta;,&amp;beta;,&amp;Gamma;,&amp;gamma;,&amp;Delta;,&amp;delta;,&amp;Epsilon;,&amp;epsilon;,&amp;Zeta;,&amp;zeta;,&amp;Eta;,&amp;eta;,&amp;Theta;,&amp;theta;,&amp;Iota;,&amp;iota;,&amp;Kappa;,&amp;kappa;,&amp;Lambda;,&amp;lambda;,&amp;Mu;,&amp;mu;,&amp;Nu;,&amp;nu;,&amp;Xi;,&amp;xi;,&amp;Omicron;,&amp;omicron;,&amp;Pi;,&amp;pi;,&amp;Rho;,&amp;rho;,&amp;Sigma;,&amp;sigmaf;,&amp;sigma;,&amp;Tau;,&amp;tau;,&amp;Upsilon;,&amp;upsilon;,&amp;Phi;,&amp;phi;,&amp;Chi;,&amp;chi;,&amp;Psi;,&amp;psi;,&amp;Omega;,&amp;omega;,&amp;thetasym;,&amp;upsih;,&amp;piv;,&amp;forall;,&amp;part;,&amp;exist;,&amp;empty;,&amp;nabla;,&amp;isin;,&amp;notin;,&amp;ni;,&amp;prod;,&amp;sum;,&amp;minus;,&amp;lowast;,&amp;radic;,&amp;prop;,&amp;infin;,&amp;ang;,&amp;and;,&amp;or;,&amp;cap;,&amp;cup;,&amp;int;,&amp;there4;,&amp;sim;,&amp;cong;,&amp;asymp;,&amp;ne;,&amp;equiv;,&amp;le;,&amp;ge;,&amp;sub;,&amp;sup;,&amp;nsub;,&amp;sube;,&amp;supe;,&amp;oplus;,&amp;otimes;,&amp;perp;,&amp;sdot;,&amp;loz;,&amp;lceil;,&amp;rceil;,&amp;lfloor;,&amp;rfloor;,&amp;lang;,&amp;rang;,&amp;larr;,&amp;uarr;,&amp;rarr;,&amp;darr;,&amp;harr;,&amp;crarr;,&amp;lArr;,&amp;uArr;,&amp;rArr;,&amp;dArr;,&amp;hArr;,&amp;bull;,&amp;hellip;,&amp;prime;,&amp;oline;,&amp;frasl;,&amp;weierp;,&amp;image;,&amp;real;,&amp;trade;,&amp;euro;,&amp;alefsym;,&amp;spades;,&amp;clubs;,&amp;hearts;,&amp;diams;,&amp;ensp;,&amp;emsp;,&amp;thinsp;,&amp;zwnj;,&amp;zwj;,&amp;lrm;,&amp;rlm;,&amp;ndash;,&amp;mdash;,&amp;lsquo;,&amp;rsquo;,&amp;sbquo;,&amp;ldquo;,&amp;rdquo;,&amp;bdquo;,&amp;dagger;,&amp;Dagger;,&amp;permil;,&amp;lsaquo;,&amp;rsaquo;"

block "default"
  lineend stay

  onstring "&lt;!--" "comment" "comment"
  onstring "&lt;!" "specialtag" "tag"
  onstring "&lt;/" "ctag" "tag"
  onstring "&lt;" "tag" "tag"
  onstringlist "entities" highlight "entity"
  onregexp "^&amp;#[0-9]+;" highlight "entity"
  onregexp "^&amp;#x[0-9A-Fa-f]+;" highlight "entity"
  onregexp "^&amp;[A-Za-z0-9]{1,8};" highlight "entityinvalid"
  onstring "&amp;" highlight "entityinvalid"
end

block "tag"
  lineend stay
  onregexp_start "^[A-Za-z][A-Za-z_0-9]+" highlight "name"
  onregexp "^[A-Za-z]" "tagattr" "attribute"
  onstring "&gt;" pop
end

block "ctag"
  onregexp_start "^[A-Za-z][A-Za-z_0-9-]+" highlight "name"
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
  onstringlist "entities" highlight "entity"
  onregexp "^&amp;#[0-9]+;" highlight "entity"
  onregexp "^&amp;#x[0-9A-Fa-f]+;" highlight "entity"
  onregexp "^&amp;[A-Za-z0-9]{1,8};" highlight "entityinvalid"
  onstring "&amp;" highlight "entityinvalid"
end

block "tagqattrvalue"
  lineend stay
  onstringlist "entities" highlight "entity"
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

#
# Pattern file for CSS
# Author: Christian Seiler <self@christian-seiler.de>
# Some ideas and regular expressions were taken from the katepart syntax highlighting file for CSS.
#

start = "default"

list "mediatypes" = "all,aural,braille,embossed,handheld,print,projection,screen,tty,tv"
list "properties" = "azimuth,background-attachment,background-color,background-image,background-position,background-repeat,background,border-collapse,border-color,border-spacing,border-style,border-top,border-right,border-bottom,border-left,border-top-color,border-right-color,border-bottom-color,border-left-color,border-top-style,border-right-style,border-bottom-style,border-left-style,border-top-width,border-right-width,border-bottom-width,border-left-width,border-width,border,bottom,caption-side,clear,clip,color,content,counter-increment,counter-reset,cue-after,cue-before,cue,cursor,direction,display,elevation,empty-cells,float,font-family,font-size,font-style,font-variant,font-weight,font,line-height,height,left,letter-spacing,list-style-image,list-style-position,list-style-type,list-style,margin-right,margin-left,margin-top,margin-bottom,margin,max-height,max-width,min-height,min-width,orphans,outline-color,outline-style,outline-width,outline,overflow,padding-top,padding-right,padding-bottom,padding-left,padding,page-break-after,page-break-before,page-break-inside,pause-after,pause-before,pause,pitch-range,pitch,play-during,position,quotes,richness,right,speaker-header,speaker-numeral,speaker-punctuation,speak,speech-rate,stress,table-layout,text-align,text-decoration,text-indent,text-transform,top,unicode-bidi,vertical-align,visibility,voice-family,volume,white-space,windows,width,word-spacing,z-index"

block "default"
  lineend stay

  onregexp "^@media\\b" "media" "at-media-rule"
  onregexp "^@import\\b" "import" "at-rule"
  onregexp "^@font-face\\b" highlight "at-rule"
  onregexp "^@charset\\b" highlight "at-rule"
  onregexp "^@page\\b" highlight "at-rule"
  onstring "{" "declarations" "declarations"
  onstring "[" "attrsel" "attribute-selector"
  onregexp "^#\\w[\\w_-]*" highlight "id-selector"
  onregexp "^\\.\\w[\\w_-]*" highlight "class-selector"
  onregexp "^:lang\\([\\w_-]+\\)" highlight "pseudo-class-selector"
  onregexp "^:(first-child|link|visited|active|hover|focus|first|last|left|right)" highlight "pseudo-class-selector"
  onregexp "^:(first-line|first-letter|before|after)" highlight "pseudo-element-selector"
  onregexp "^:[\w_-]+" highlight "invalid-pseudo-selector"
  onregexp "^\\w[\\w_-]*" highlight "element-selector"

  onregexp "^(&gt;|\\*|\\+|,)" highlight "symbol"

  onstring "/*" "comment" "comment"
  onstring "&quot;" "string" "string"
  onstring "'" "sqstring" "string"
end

block "attrsel"
  lineend stay
  onstring "&quot;" "string" "string"
  onstring "'" "sqstring" "string"
  onstring "]" pop
end

block "declarations"
  lineend stay
  
  onstring "}" pop
  onstringlist "properties" "declaration" "property"
  onregexp "^-?[A-Za-z_-]+(?=\s*:)" "declaration" "unknown-property"
  onstring "/*" "comment" "comment"
end

block "declaration"
  lineend stay
  
  onstring ":" "declaration2" "property-value"
end

block "declaration2"
  lineend stay

  onstring "&quot;" "string" "string"
  onstring "'" "sqstring" "string"

  onstring ";" pop 2
  onstring "}" pop 3
  onregexp "^!important\b" highlight "important-rule"
  onstring "/*" "comment" "comment"

  onregexp "^#([0-9A-Fa-f]{3}){1,2}\\b" highlight "value"
  onregexp "^[-+]?[0-9]\\.?[0-9]*(em|ex|px|in|cm|mm|pt|pc|deg|rad|grad|ms|s|Hz|kHz)\\b" highlight "value"
  onregexp "^[-+]?[0-9]\\.?[0-9]*%?" highlight "value"
  onregexp "^(url|attr|rect|rgb|counters?)" "parvalue" "value"
end

block "parvalue"
  lineend stay
  
  onstring "(" "parvalue2" "parenthesis-value"
  onstring "/*" "comment" "comment"
end

block "parvalue2"
  lineend stay
  
  onstring ")" pop 2

  onstring "/*" "comment" "comment"
  onstring "&quot;" "string" "string"
  onstring "'" "sqstring" "string"

  onregexp "^\\\\[()'\",]" highlight "escaped-value"

  onregexp "^[-+]?[0-9]\\.?[0-9]*(em|ex|px|in|cm|mm|pt|pc|deg|rad|grad|ms|s|Hz|kHz)\\b" highlight "value"
  onregexp "^[-+]?[0-9]\\.?[0-9]*%?" highlight "value"
end

block "media"
  lineend stay
  
  onstring "{" "media2" "at-media-block"
  onstringlist "mediatypes" highlight "mediatype"
  onstring "," highlight "symbol"
end

block "media2"
  lineend stay
  
  onregexp "^@media\\b" "media" "at-media-rule"
  onregexp "^@import\\b" "import" "at-rule"
  onregexp "^@font-face\\b" highlight "at-rule"
  onregexp "^@charset\\b" highlight "at-rule"
  onregexp "^@page\\b" highlight "at-rule"
  onstring "{" "declarations" "declarations"
  onstring "[" "attrsel" "attribute-selector"
  onregexp "^#\\w[\\w_-]*" highlight "id-selector"
  onregexp "^\\.\\w[\\w_-]*" highlight "class-selector"
  onregexp "^:lang\\([\\w_-]+\\)" highlight "pseudo-class-selector"
  onregexp "^:(first-child|link|visited|active|hover|focus|first|last|left|right)" highlight "pseudo-class-selector"
  onregexp "^:(first-line|first-letter|before|after)" highlight "pseudo-element-selector"
  onregexp "^:[\w_-]+" highlight "invalid-pseudo-selector"
  onregexp "^\\w[\\w_-]*" highlight "element-selector"
  onstring "/*" "comment" "comment"
  onstring "&quot;" "string" "string"
  onstring "'" "sqstring" "string"
  
  onstring "}" pop 2
end

block "import"
  lineend stay

  onstring "url" "parvalue" "value"

  onstringlist "mediatypes" highlight "mediatype"
  onstring "/*" "comment" "comment"
  onstring "&quot;" "string" "string"
  onstring "'" "sqstring" "string"
  onregexp "^[-+]?[0-9]\\.?[0-9]*(em|ex|px|in|cm|mm|pt|pc|deg|rad|grad|ms|s|Hz|kHz)\\b" highlight "value"
  onregexp "^[-+]?[0-9]\\.?[0-9]*%?" highlight "value"
  onstring ";" pop
end

block "string"
  lineend stay
  
  onstring "\\&quot;" stay
  onstring "&quot;" pop
end

block "sqstring"
  lineend stay
  
  onstring "\\'" stay
  onstring "'" pop
end

block "comment"
  lineend stay
  onstring "*/" pop
end

# eof
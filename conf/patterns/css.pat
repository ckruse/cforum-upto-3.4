#
# Pattern file for CSS
# Author: Christian Seiler <self@christian-seiler.de>
# Some ideas and regular expressions were taken from the katepart syntax highlighting file for CSS.
#

start = "default"

list "mediatypes" = "all,aural,braille,embossed,handheld,print,projection,screen,tty,tv"
list "properties" = "additive-symbols,align-content,align-items,align-self,alignment-adjust,alignment-baseline,animation,animation-delay,animation-direction,animation-duration,animation-iteration-count,animation-play-state,backface-visibility,background,background-attachment,background-clip,background-color,background-image,background-origin,background-position,background-repeat,background-size,baseline-shift,binding,bleed,bookmark-label,bookmark-level,bookmark-state,bookmark-target,border,border-bottom,border-bottom-color,border-bottom-left-radius,border-bottom-right-radius,border-bottom-style,border-bottom-width,border-collapse,border-color,border-image,border-image-outset,border-image-repeat,border-image-slice,border-image-source,border-image-width,border-left,border-left-color,border-left-style,border-left-width,border-radius,border-right,border-right-color,border-right-style,border-right-width,border-spacing,border-style,border-top,border-top-color,border-top-left-radius,border-top-right-radius,border-top-style,border-top-width,border-width,bottom,box-decoration-break,box-shadow,box-sizing,break-after,break-before,break-inside,caption-side,clear,clip,color,column-count,column-fill,column-gap,column-rule,column-rule-color,column-rule-style,column-rule-width,column-span,column-width,columns,content,counter-increment,counter-reset,crop,cue,cue-after,cue-before,cursor,direction,display,dominant-baseline,drop-initial-after-adjust,drop-initial-before-adjust,drop-initial-before-align,drop-initial-size,drop-initial-value,empty-cells,fallback,fit,fit-position,flex,flex-basis,flex-direction,flex-flow,flex-grow,flex-shrink,flex-wrap,float,float-offset,font,font-family,font-feature-settings,font-kerning,font-language-override,font-size,font-size-adjust,font-stretch,font-style,font-synthesis,font-variant,font-variant-alternates,font-variant-caps,font-variant-east-asian,font-variant-ligatures,font-variant-numeric,font-variant-position,font-weight,grid-columns,grid-rows,hanging-punctuation,height,hyphens,icon,image-orientation,image-resolution,inline-box-align,justify-content,left,letter-spacing,line-break,line-height,line-stacking,line-stacking-ruby,line-stacking-shift,line-stacking-strategy,list-style,list-style-image,list-style-position,list-style-type,margin,margin-bottom,margin-left,margin-right,margin-top,marks,marquee-direction,marquee-play-count,marquee-speed,marquee-style,max-height,max-width,max-zoom,min-height,min-width,min-zoom,move-to,nav-down,nav-index,nav-left,nav-right,nav-up,negative,object-fit,object-position,opacity,order,orientation,orphans,outline,outline-color,outline-offset,outline-style,outline-width,overflow,overflow-style,overflow-wrap,overflow-x,overflow-y,padding,padding-bottom,padding-left,padding-right,padding-top,page,page-break-after,page-break-before,page-break-inside,page-policy,pause,pause-after,pause-before,perspective,perspective-origin,position,prefix,presentation-level,quotes,range,resize,resolution,rest,rest-after,rest-before,right,rotation,rotation-point,ruby-align,ruby-overhang,ruby-position,ruby-span,shape-image-threshold,shape-inside,shape-outside,size,speak,speak-as,src,string-set,suffix,symbols,system,tab-size,table-layout,target,target-name,target-new,target-position,text-align,text-align-last,text-decoration,text-decoration-color,text-decoration-line,text-decoration-skip,text-decoration-style,text-emphasis,text-emphasis-color,text-emphasis-position,text-emphasis-style,text-height,text-indent,text-justify,text-overflow,text-shadow,text-transform,text-underline-position,top,transform,transform-origin,transform-style,transition,transition-delay,transition-duration,transition-property,transition-timing-function,unicode-bidi,unicode-range,user-zoom,vertical-align,visibility,voice-balance,voice-duration,voice-family,voice-pitch,voice-range,voice-rate,voice-stress,voice-volume,white-space,widows,width,word-break,word-spacing,word-wrap,wrap,wrap-flow,wrap-margin,wrap-padding,wrap-through,z-index,zoom"

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
  onregexp "^:(nth-child|nth-last-child|nth-of-type|nth-last-of-type)\\((\\w|\\+|-|\\*|\\/)+\\)" highlight "pseudo-class-selector"
  onregexp "^:not\\(([\\w_-]|:|#|\\.|\\[|\\]|\\+|\\~|>|\\s)+\\)" highlight "pseudo-class-selector"
  onregexp "^::(first-line|first-letter|before|after)" highlight "pseudo-element-selector"
  onregexp "^:(link|visited|hover|active|focus|target|lang|enabled|disabled|checked|indeterminate|root|first-child|last-child|first-of-type|last-of-type|only-child|only-of-type|empty)" highlight "pseudo-class-selector"
  onregexp "^:[\\w_-]+" highlight "invalid-pseudo-selector"
  onregexp "^(\\w[\\w_-]*|\\*)" highlight "element-selector"

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
  onregexp "^-(?:moz|o|webkit|ms)-[A-Za-z_-]+(?=\s*:)" "declaration" "vendor"
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
  onregexp "^(url|attr|rect|rgb|counters?|calc|linear-gradient|radial-gradient)" "parvalue" "value"
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
  onregexp "^:(nth-child|nth-last-child|nth-of-type|nth-last-of-type)\\((\\w|\\+|-|\\*|\\/)+\\)" highlight "pseudo-class-selector"
  onregexp "^:not\\(([\\w_-]|:|#|\\.|\\[|\\]|\\+|\\~|>|\\s)+\\)" highlight "pseudo-class-selector"
  onregexp "^::(first-line|first-letter|before|after)" highlight "pseudo-element-selector"
  onregexp "^:(link|visited|hover|active|focus|target|lang|enabled|disabled|checked|indeterminate|root|first-child|last-child|first-of-type|last-of-type|only-child|only-of-type|empty)" highlight "pseudo-class-selector"
  onregexp "^:[\\w_-]+" highlight "invalid-pseudo-selector"
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

  onstring "\\&quot;" highlight "escaped"
  onstring "&quot;" pop
end

block "sqstring"
  lineend stay

  onstring "\\'" highlight "escaped"
  onstring "'" pop
end

block "comment"
  lineend stay
  onstring "*/" pop
end

# eof
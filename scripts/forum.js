/*
 * Forum JS utilities
 */

var MODIFIER_SHIFT = 1;
var MODIFIER_ALT   = 2;
var MODIFIER_CTRL  = 4;

var wiki_uri = 'http://de.wikipedia.org/wiki/';

var active_thread = null;
var wiki_window   = null;

/* get XmlHttpRequest instance */
xmlhttp = false;

/*@cc_on @*/
/*@if (@_jscript_version >= 5)
// JScript gives us Conditional compilation, we can cope with old IE versions.
// and security blocked creation of the objects.
try {
  xmlhttp = new ActiveXObject("Msxml2.XMLHTTP");
}
catch (e) {
  try {
    xmlhttp = new ActiveXObject("Microsoft.XMLHTTP");
  }
  catch (E) {
    xmlhttp = false;
  }
}
@end @*/
if (!xmlhttp && typeof XMLHttpRequest != 'undefined') xmlhttp = new XMLHttpRequest();

/* {{{ validation functions */

/* {{{ helpers */

/* {{{ toUint16 */
function toUint16(val) {
  return val & ((1<<16)-1);
}
/* }}} */

/* {{{ toUint32
 * JS internally converts numbers to signed int 32 with bit
 * operations, so we have to use the much, much slower modulo
 * operation instead of some bit shifting
 */
var MAX32 = 0xFFFFFFFF;
function toUint32(val) {
  if(val < 0) val = MAX32 + val;
  return val % (MAX32 + 1);
}
/* }}} */

/* {{{ isalnum */
function isalnum(c) {
  var x = c.charCodeAt(0);
  return (toUint16((x | 0x20) - 'a'.charCodeAt(0)) < 26) || (toUint16(x - '0'.charCodeAt(0)) < 10);
}
/* }}} */

/* {{{ isascii */
function isascii(c) {
  return c.charCodeAt(0) < 128;
}
/* }}} */

/* {{{ isdigit */
function isdigit(c) {
  return !isNaN(parseInt(c.charAt(0),10));
}
/* }}} */

/* {{{ isxdigit */
function isxdigit(c) {
  return !isNaN(parseInt(c.charAt(0),16));
}
/* }}} */

/* {{{ isextra */
function isextra(c) {
  switch(c) {
    case '!':
    case '*':
    case '\'':
    case '(':
    case ')':
    case ',':
      return 1;
  }

  return 0;
}
/* }}} */

/* {{{ issafe */
function issafe(c) {
  switch(c) {
    case '$':
    case '-':
    case '_':
    case '.':
    case '+':
      return 1;
  }

  return 0;
}
/* }}} */

/* {{{ isuchar_wo_escape */
function isuchar_wo_escape(c) {
  /* escape is not checked! */
  if((!isalnum(c) || !isascii(c)) && !issafe(c) && !isextra(c)) return 0;
  return 1;
}
/* }}} */

/* }}} */

/* {{{ is_valid_hostname */
function is_valid_hostname(hostname) {
  var idna  = new IDNA(hostname);
  var host  = idna.toASCII();

  if(!host) return false;
  if(host.charAt(0) == '-' || host.charAt(host.length-1) == '-') return false;

  for(var i=0;i<host.length;++i) {
    var c = host.charAt(i);

    if((!isalnum(c) && c != '-') || !isascii(c)) {
      if(c == '.') {
        /*
         * two dots after another are not allowed, neither a dot at the beginning
         * A dash at the beginning or the end of a label is not allowed
         */
        if(i == 0 || host.charAt(i-1) == '.' || host.charAt(i-1) == '-' || host.charAt(i+1) == '-') return false;
        continue;
      }

      return false;
    }
  }

  return true;
}
/* }}} */

/* {{{ is_valid_http_link_check */
function is_valid_http_link_check(link) {
  /* we allow anchors by default */
  return is_valid_http_link(link,0);
}
/* }}} */

/* {{{ is_valid_http_link */
function is_valid_http_link(link,strict) {
  var end = 0;

  /* first we check if the scheme is valid */
  if(link.substr(0,7) != "http://") {
    if(link.substr(0,8) != "https://") return false;

    link = link.substring(8,link.length);
  }
  else link = link.substring(7,link.length);

  /* ok, it seems as if scheme is valid -- get hostname */
  for(var i=0;i<link.length;++i) {
    var c = link.charAt(i);

    if(c == ':' || c == '/' || c == '#' || c == '?') {
      end = i - 1;
      break;
    }
  }

  /* URL consists only of a hostname if end is NULL */
  if(end == 0) end = i - 1;

  var host = link.substring(0,i);
  if(!is_valid_hostname(host)) return false;

  /* hostname is valid; follows a port? */
  if(link.charAt(i) == ':') {
    /* port has to be digit+ */
    for(var begin=++i;i<link.length;++i) {
      var c = link.charAt(i);

      /* port is valid (at least one digit followed by a slash) */
      if(c == '/' && i > begin) break;

      /* hu? port must be digits... bad boy! */
      if(!isdigit(c)) return false;
    }
  }

  /* follows a host path? */
  if(link.charAt(i) == '/') {
    for(var begin=i;i<link.length;++i) {
      var c = link.charAt(i);

      /* escape sequenz */
      if(c == '%') {
        if(!isxdigit(link.charAt(i+1)) || !isxdigit(link.charAt(i+2))) return false;
        i += 2;
        continue;
      }

      /* anchor means end of host path */
      if(c == '#') break;

      if(!isuchar_wo_escape(c)) {
        switch(c) {
          case '/':
          case ';':
          case ':':
          case '@':
          case '&':
          case '=':
          case '~':
            continue;
          default:
            if(c != '?') return false;
        }

        /* could only be a question mark (end of host path) */
        break;
      }
    }
  }

  /* follows a search path? */
  if(link.charAt(i) == '?') {
    for(++i;i<link.length;++i) {
      var c = link.charAt(i);
  
      /* escaped character */
      if(c == '%') {
        if(!isxdigit(link.charAt(i+1)) || !isxdigit(link.charAt(i+2))) return false;
        i += 2;
        continue;
      }

      /* anchor means end of search path */
      if(c == '#') break;

      /* we checked for escaped before */
      if(!isuchar_wo_escape(c)) {
        switch(c) {
          case ';':
          case ':':
          case '@':
          case '&':
          case '=':
          case '/':
            break;
          default:
            /* no anchor in strict mode */
            return false;
        }
      }
    }
  }

  if(link.charAt(i) == '#') {
    if(strict) return false;

    for(++i;i<link.length;++i) {
      var c = link.charAt(i);

      if(c == '%') {
        if(!isxdigit(link.charAt(i+1)) || !isxdigit(link.charAt(i+2))) return -1;
        i += 2;
        continue;
      }

      if(!isuchar_wo_escape(c)) {
        switch(c) {
          case ';':
          case '/':
          case '?':
          case ':':
          case '@':
          case '&':
          case '=':
            break;
          default:
            return false;
        }
      }
    }
  }

  return true;
}
/* }}} */

/* {{{ is_valid_mailto_link */
function is_valid_mailto_link(addr) {
  /* first, check if mail address begins with mailto: */
  if(addr.substr(0,7) == "mailto:") {
    /* let is_valid_mailaddress() do the checks... */
    return is_valid_mailaddress(addr.substring(7,addr.length));
  }

  /* not a mailto:-scheme -- bad guy! invalid! */
  return false;
}
/* }}} */

/* {{{ is_valid_mailaddress */
/*
 * Algorithm is in parts from 'Secure Cooking With C and C++'
 */
function is_valid_mailaddress(address) {
  var rfc822_specials = "()<>@,;:\\\"[]";
  var i = 0;

  /* first we validate the name portion (name@domain) */
  for(i=0;i<address.length;++i) {
    var c = address.charAt(i);

    if(c == '\"' && (i == 0 || address.charAt(i-1) == '.' || address.charAt(i-1) == '\"')) {
      while (++i < address.length) {
        var c = address.charAt(i);

        if (c == '\"') break;
        if (c == '\\' && (address.charAt(++i) == ' ')) continue;
        if (address.charCodeAt(0) <= 0x20 || address.charCodeAt(i) >= 127) return false;
      }

      if(i++>=address.length) return false;
      if(address.charAt(i) == '@') break;
      if(address.charAt(i) != '.') return false;
      continue;
    }

    if(address.charAt(i) == '@') break;
    if(address.charCodeAt(i) <= 0x20 || address.charCodeAt(i) >= 127) return false;
    if(rfc822_specials.indexOf(address.charAt(i)) > -1) return false;
  }

  if(i == 0 || address.charAt(i-1) == '.') return false;

  /* next we validate the domain portion (name@domain) */
  if(address.length < ++i) return false;

  /*
   * we also have to accept Umlauts domains, which means we have
   * to check domain name more complicated than the secure cooking
   * does
   */
  return is_valid_hostname(address.substring(i,address.length));
}
/* }}} */

/* {{{ is_valid_file_link */
function is_valid_file_link(link) {
  var slash = 0;
  var dp    = 0;
  var i     = 0;

  if(link.substr(0,7) != "file://") return false;

  if(link.substr(7,9) == "localhost") {
    if(link.charAt(16) != '/') return false;
    slash = 16;
  }
  else {
    /* ok, no file path follows; this is not allowed */
    if((slash = link.indexOf("/",7)) == -1) return false;

    /* we got a hostname without port number */
    if((dp = link.indexOf(":",7)) == -1) {
      hostname = link.substring(7,slash);
      if(!is_valid_hostname(hostname)) return false;
    }
    /* we got a hostname with port number */
    else {
      hostname = link.substring(7,dp);
      if(!is_valid_hostname(hostname)) return false;

      for(i=dp+1;i<link.length;++i) {
        if(!isdigit(link.charAt(i))) return false;
      }
    }
  }

  /* ok, now fpath segments follow */
  for(i=slash+1;i<link.length;++i) {
    var c = link.charAt(i);

    if(!isuchar_wo_escape(c)) {
      if(c == '%') {
        if(!isxdigit(link.charAt(i+1)) || !isxdigit(link.charAt(i+2))) return false;
        i += 2;
        continue;
      }

      if(c == ';') break;

      switch(c) {
        case '?':
        case ':':
        case '@':
        case '&':
        case '=':
        case '/':
          break;
        default:
          return false;
      }
    }
  }

  return true;
}
/* }}} */

var validation_schemes = new Object();
validation_schemes.http   = is_valid_http_link_check;
validation_schemes.https  = is_valid_http_link_check;
validation_schemes.mailto = is_valid_mailto_link;
validation_schemes.file   = is_valid_file_link;

/* {{{ is_valid_link */
function is_valid_link(link) {
  var x = link.indexOf("://");
  var scheme,vs = 0;

  /*
   * no scheme found, but mailto-links are mailto:<address> and news-links are
   * news:<address>, not mailto://<address> and not news://<address> -- so we
   * have to do an extra check
   */
  if(x == -1) {
    if(link.substr(0,7) == "mailto:") scheme = "mailto";
    else if(link.substr(0,5) == "news:") scheme = "news";
    else return false;
  }
  else {
    if(link.substr(0,12) == "view-source:") {
      link = link.substring(12,link.length);
      vs = 1;
    }

    x = link.indexOf("://");
    scheme = link.substring(0,x);

    /* view-source may only be used with http and https */
    if(vs && scheme != "http" && scheme != "https") return false;
  }

  if(vs) return is_valid_http_link_check(link);
  else {
    if(validation_schemes[scheme]) return validation_schemes[scheme](link);
  }

  return false;
}
/* }}} */


/* }}} */


/* {{{ stringlist handling */
function xmlhttp_get_contents(xml,uri,uname,pass) {
  if(uname && pass) xmlhttp.open("GET",uri,false,uname,pass);
  else xmlhttp.open("GET", uri, false);

  xmlhttp.send(null);
  return xmlhttp.responseText;
}

function set_stringlist_value(prompttxt,base,directive,param) {
  if(!xmlhttp) return true;

  var val = '';

  if(window.getSelection) {
    var obj = window.getSelection();
    val = obj.toString();
  }
  else if(document.selection) {
    var range = document.selection.createRange();
    val = range.text;
  }

  if(!val) {
    val = prompt(prompttxt);
    if(!val) return true;
  }

  var date = new Date();
  var uri  = '';

  if(base.indexOf(window.location.protocol) < 0) uri = window.location.protocol + '//' + window.location.hostname;
  uri += base + '?a=setvalue&directive='+directive+'&'+param+'='+encodeURIComponent(val)+'&type=stringlist&unique=' + date.getTime();

  xmlhttp_get_contents(xmlhttp,uri,null,null);

  window.location.reload();

  return false;
}

function remove_stringlist_value(prompttxt,base,directive,param) {
  if(!xmlhttp) return true;

  var val = '';

  if(window.getSelection) {
    var obj = window.getSelection();
    val = obj.toString();
  }
  else if(document.selection) {
    var range = document.selection.createRange();
    val = range.text;
  }

  if(!val) {
    val = prompt(prompttxt);
    if(!val) return true;
  }

  var date = new Date();
  var uri  = '';

  if(base.indexOf(window.location.protocol) < 0) uri = window.location.protocol + '//' + window.location.hostname;
  uri  += base + '?a=removevalue&directive='+directive+'&'+param+'='+encodeURIComponent(val)+'&type=stringlist&unique=' + date.getTime();

  xmlhttp_get_contents(xmlhttp,uri,null,null);

  window.location.reload();

  return false;
}
/* }}} */


/* {{{ keybinding handling */
function register_keybinding(table,key,modifiers,action) {
  if(key.toLowerCase() != key) {
    key = key.toLowerCase();
    modifiers |= MODIFIER_SHIFT;
  }

  code = key.toUpperCase().charCodeAt(0);
  if(!table[code]) table[code] = new Array();

  var entry          = new Array();
  entry['key']       = key;
  entry['modifiers'] = modifiers;
  entry['action']    = action;

  for(var i=0;i<table[code].length;++i) {
    if(table[code][i].modifiers == modifiers) {
      table[code][i] = entry;
      return;
    }
  }

  table[code].push(entry);
}

function handle_keyevent(e,table) {
  var code      = 0;
  var modifiers = 0;
  var node      = null;

  if(!e) e = window.event;

  if(e.target) node = e.target;
  else if (e.srcElement) node = e.srcElement;
  if(node.nodeName.toLowerCase() == "textarea" || node.nodeName.toLowerCase() == "input") return true;


  if(e.which) code = e.which;
  else if(e.keyCode) code = e.keyCode;
  else return true;

  if(e.shiftKey) modifiers |= MODIFIER_SHIFT;
  if(e.altKey) modifiers |= MODIFIER_ALT;
  if(e.ctrlKey) modifiers |= MODIFIER_CTRL;

  if(table[code]) {
    for(var i=0;i<table[code].length;++i) {
      if(table[code][i].modifiers == modifiers) return table[code][i].action();
    }
  }

  return true;
}
/* }}} */

/* {{{ posting functions */
function next_posting() {
  if(!document.getElementsByTagName) return true;

  var elems = document.getElementsByTagName('link');
  for(var i=0;i<elems.length;++i) {
    if(elems[i].getAttribute('rel') == 'next') {
      window.location.href = elems[i].getAttribute('href');
      return false;
    }
  }

  return true;
}

function prev_posting() {
  if(!document.getElementsByTagName) return true;

  var elems = document.getElementsByTagName('link');
  for(var i=0;i<elems.length;++i) {
    if(elems[i].getAttribute('rel') == 'prev') {
      window.location.href = elems[i].getAttribute('href');
      return false;
    }
  }

  return true;
}

function back_to_threadlist() {
  if(forum_uri) window.location.href = forum_uri;
  return false;
}

function focus_reply() {
  if(!document.getElementById) return true;

  var elem = document.getElementById('body');
  if(!elem) return true;

  elem.focus();
  if(elem.scrollIntoView) elem.scrollIntoView(true);
  else {
    if(window.location.href.indexOf("#body") < 0) window.location.href += '#body';
    else window.location.href = window.location.href;
  }

  return false;
}

function vote_good() {
  if(!window.vote) {
    alert("Um diese Funktion zu nutzen müssen Sie die Option 'Dynamisches Javascript zum Abstimmen nutzen' aktiviert haben!");
    return true;
  }

  vote(tid,mid,'good');
  return false;
}

function vote_bad() {
  if(!window.vote) {
    alert("Um diese Funktion zu nutzen müssen Sie die Option 'Dynamisches Javascript zum Abstimmen nutzen' aktiviert haben!");
    return true;
  }

  vote(tid,mid,'bad');
  return false;
}

function focus_active() {
  if(!document.getElementsByTagName) return true;

  var act = document.getElementById('active-post');
  var lis = document.getElementsByTagName('li');

  if(!act) return true;

  for(var i=0;i<lis.length;++i) {
    /* check if this is the active */
    if(lis[i] == act) {
      if(++i == lis.length) return true;

      var a = lis[i].getElementsByTagName('a')[0];
      if(a.focus) {
        a.focus();
        if(a.scrollIntoView) a.scrollIntoView(true);
        return false;
      }
    }
  }

  return true;
}

function kill_post() {
  window.location.href = forum_uri + '?dt=' + tid + '&a=d';
}

function mark_visited() {
  window.location.href = forum_uri + '?mv=' + tid;
}

/* }}} */

/* {{{ threadlist functions */
function add_to_blacklist() {
  return set_stringlist_value("Name, der auf die Blacklist soll",userconf_uri,'BlackList','blacklist');
}

function remove_from_blacklist() {
  return remove_stringlist_value("Name, der von der Blacklist entfernt werden soll",userconf_uri,"BlackList","blacklist")
}

function add_to_whitelist() {
  return set_stringlist_value("Name, der auf die Whitelist soll",userconf_uri,'WhiteList','whitelst');
}

function remove_from_whitelist() {
  return remove_stringlist_value("Name, der von der Whitelist entfernt werden soll",userconf_uri,"WhiteList","whitelst")
}

function add_to_highlightcats() {
  return set_stringlist_value("Kategorie, die von nun an hervorgehoben werden soll",userconf_uri,'HighlightCategories','highlightcats');
}

function remove_from_highlightcats() {
  return remove_stringlist_value("Kategorie, die von nun an nicht mehr hervorgehoben werden soll",userconf_uri,'HighlightCategories','highlightcats');
}

function mark_all_visited() {
  window.location.href = forum_base_url + '?mav=1';
}

function wikipedia_lookup() {
  if(!xmlhttp) return true;

  var val = '';

  if(window.getSelection) {
    var obj = window.getSelection();
    val = obj.toString();
  }
  else if(document.selection) {
    var range = document.selection.createRange();
    val = range.text;
  }

  if(!val) {
    val = prompt("Suchbegriff:");
    if(!val) return true;
  }

  try {
    wiki_window = window.open(wiki_uri + val,'wiki','');
  }
  catch(e) {
    location.href = wiki_uri + val;
  }
}
/* }}} */


/* eof */

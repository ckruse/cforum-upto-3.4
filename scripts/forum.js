/*
 * Forum JS utilities
 */

var MODIFIER_SHIFT = 1;
var MODIFIER_ALT   = 2;
var MODIFIER_CTRL  = 4;

var active_thread = null;

function recurseVars(v) {
  var txt = "";
  switch (typeof(v)) {
    case "boolean": return "[bool]"+(v?'yes':'no');
    case "string": return "[string]"+v;
    case "number": return "[number]"+v;
    case "undefined": return "[undefined]";
    case "function": return "[function]"+v;
    case "object":
      txt += "[object]\n";
      for (var item in v) {
        txt += "  propertyname="+item+ " value="+v[item]+"\n";
      }
      return txt;
  }
}
function log(msg,hint) {
  if (window.console) {
    window.console.log(hint+":\n"+recurseVars(msg));
  }
  else {
  //  alert(hint+":\n"+recurseVars(msg));
  }

  return msg;
}



/* {{{ XmlHttpRequest */

var xmlhttp = false;

/*@cc_on @*/
/*@if (@_jscript_version >= 5)
// JScript gives us Conditional compilation, we can cope with old IE versions
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
if(!xmlhttp && typeof XMLHttpRequest != 'undefined') xmlhttp = new XMLHttpRequest();

function xmlhttp_get_contents(xml,uri,uname,pass) {
  xmlhttp.open("GET", uri, false);
  xmlhttp.send(null);
  return xmlhttp.responseText;
}
/* }}} */



/* {{{ stringlist handling */

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

  if(csrftoken) uri += "&csrftoken=" + encodeURIComponent(csrftoken);

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

  if(csrftoken) uri += "&csrftoken=" + encodeURIComponent(csrftoken);

  xmlhttp_get_contents(xmlhttp,uri,null,null);

  window.location.reload();

  return false;
}

/* Tastatur-Shortcuts */

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
  else if(e.srcElement) node = e.srcElement;
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



/* {{{ navigation */

/* Tastatur-Shortcuts */

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

function focus_active() {
  if(!document.getElementsByTagName) return true;

  var act = document.getElementById('active-post');
  var lis = document.getElementsByTagName('li');

  if(!act) return true;

  for(var i=0;i<lis.length;++i) {
    // check if this is the active
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

/* }}} */

/* {{{ Voting */

function vote(tid,mid,action) {
  if(!xmlhttp) return true;

  var date    = new Date();
  var uri     = base_uri + "cgi-bin/user/fo_vote?t=" + tid + "&m=" + mid + "&a=" + action + "&mode=xmlhttp&unique="+date.getTime();
  if(csrftoken) uri += "&csrftoken=" + encodeURIComponent(csrftoken);

  var content = xmlhttp_get_contents(xmlhttp,uri,null,null);

  if(content) {
    var elem = document.getElementById('votes-'+action+'-num-m'+mid);
    if(elem) {
      elem.innerHTML = content;
      return false;
    }
  }

  return true;
}

/* Tastatur-Shortcuts */

function vote_good() {
  if(!window.vote) {
    alert("Um diese Funktion zu nutzen mssen Sie die Option 'Dynamisches Javascript zum Abstimmen nutzen' aktiviert haben!");
    return true;
  }

  vote(tid,mid,'good');
  return false;
}

function vote_bad() {
  if(!window.vote) {
    alert("Um diese Funktion zu nutzen mssen Sie die Option 'Dynamisches Javascript zum Abstimmen nutzen' aktiviert haben!");
    return true;
  }

  vote(tid,mid,'bad');
  return false;
}

/* }}} */

/* {{{ Auf- und Zuklappen */
function snap(tid,action) {
  if(!xmlhttp) return true;

  var date  = new Date();
  var litem = document.getElementById('t'+tid);
  var list  = litem.parentNode;
  var uri = forum_base_url+'?oc_t='+tid+'&t='+tid+'&a='+action+'&mode=xmlhttp&unique='+date.getTime();
  if(csrftoken) uri += "&csrftoken=" + encodeURIComponent(csrftoken);

  var cnt = xmlhttp_get_contents(xmlhttp,uri,null,null);

  var el = document.createElement('div');
  el.innerHTML = cnt;

  list.replaceChild(el.getElementsByTagName('li')[0],litem);
  return false;
}

/* }}} */

/* {{{ Threads ausblenden */
function deleted(tid) {
  if(!xmlhttp) return true;

  var li   = document.getElementById('t' + tid);
  var uri  = forum_base_url+'?dt='+tid+'&t='+tid+'&mode=xmlhttp&a=d';
  if(csrftoken) uri += "&csrftoken=" + encodeURIComponent(csrftoken);

  xmlhttp_get_contents(xmlhttp,uri,null,null);
  li.style.display = 'none';
  return false;
}

/* Tastatur-Shortcuts */
function kill_post() {
  var uri = forum_uri + '?dt=' + tid + '&a=d';
  if(csrftoken) uri += "&csrftoken=" + encodeURIComponent(csrftoken);
  window.location.href = uri;
}

/* }}} */

/* {{{ Threads als gelesen markieren */
function visited(tid) {
  if(!xmlhttp) return true;

  var date = new Date();
  var li   = document.getElementById('t'+tid);
  var list = li.parentNode;
  var uri  = forum_base_url+'?mv='+tid+'&t='+tid+'&mode=xmlhttp&unique='+date.getTime();
  if(csrftoken) uri += "&csrftoken=" + encodeURIComponent(csrftoken);

  var cnt  = xmlhttp_get_contents(xmlhttp,uri,null,null);

  var el   = document.createElement('div');
  el.innerHTML = cnt;

  list.replaceChild(el.getElementsByTagName('li')[0],li);
  return false;
}

/* Tastatur-Shortcuts */

function mark_visited() {
  var uri = forum_uri + '?mv=' + tid;
  if(csrftoken) uri += "&csrftoken=" + encodeURIComponent(csrftoken);
  window.location.href = uri;
}

function mark_all_visited() {
  uri = forum_base_url + '?mav=1';
  if(csrftoken) uri += "&csrftoken=" + encodeURIComponent(csrftoken);
  window.location.href = uri;
}
/* }}} */

/* {{{ Admin-Funktionen */
function admin(tid,mid,action) {
  if(!xmlhttp) return true;

  if (action == 'del') {
    var result = window.confirm('Wollen Sie den Beitrag samt Anworten wirklich löschen?');
    if(!result) return false;
  }
  else if (action == 'archive') {
    var result = window.confirm('Wollen Sie den Thread wirklich von der Forumsseite ins Archiv verschieben?');
    if(!result) return false;
  }

  var date = new Date();
  var li   = document.getElementById('t'+tid);
  var list = li.parentNode;

  var uri = forum_base_url + '?t=' + tid + '&m=' + mid + '&mode=xmlhttp&unique=' + date.getTime() + '&aaf=1';
  if(action == 'remove-na' || action == 'set-na' || action == 'set-noarchive' || action == 'remove-noarchive') uri += '&a='+action;
  else uri += '&faa='+action;
  if(csrftoken) uri += "&csrftoken=" + encodeURIComponent(csrftoken);

  var cnt = xmlhttp_get_contents(xmlhttp,uri,null,null);

  if(action == "archive") li.style.display = 'none';
  else {
    var el = document.createElement('div');
    el.innerHTML = cnt;

    list.replaceChild(el.getElementsByTagName('li')[0],li);
  }

  return false;
}
/* }}} */

/* {{{ In Wikipedia nachschlagen */

/* Tastatur-Shortcuts */

var wiki_uri = 'http://de.wikipedia.org/wiki/';
var wiki_window   = null;

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

/* {{{ Chat- und News-Fenster */
function Chat() {
  window.open("http://aktuell.de.selfhtml.org/live/chat.htm", "popup", "width=600,height=450");
}

/*

function News() {
  window.open("http://aktuell.de.selfhtml.org/news.htm", "popup", "width=520,height=400,scrollbars=yes,resizable=yes,status=yes");
}

*/

/* }}} */

/* eof */

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
    if(lis[i] == act.parentNode) {
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

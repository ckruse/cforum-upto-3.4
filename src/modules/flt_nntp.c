/**
 * \file flt_nntp.c
 * \author Christian Kruse
 * Implementation of the NNTP stack for the selfforum
 *
 * \todo Implement good message id parsing
 * \todo Define posting handling
 * \todo Implement XOVER
 */

/* {{{ Initial comments */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

/* {{{ Includes */
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"
#include "fo_server.h"
#include "charconvert.h"
#include "serverlib.h"

/* }}} */

/** I am lazy... */
#define flt_nntp_syntax_error() writen(sock,"501 command syntax error\015\012",26)

/** free tokens and line */
#define flt_nntp_cleanup_req()  do { \
  while(tnum--) free(tokens[tnum]); \
  free(tokens); \
  free(line); \
} while(0)

/** same as atoi() just for a u_char, not for a u_char * */
#define CtoI(n) (n-'0')

/** calculate how many days the year has */
#define days_in_year(y) ((y) % 4 ? 365 : (y) % 100 ? 366 : (y) % 400 ? 365 : 366)

/** check if character is an delimiter */
#define flt_nntp_is_delim(c) (isspace(c) || (c) == ':' || (c) == '=')


static u_char       *NNTPInterface = NULL; /**< The interface(s) to listen on */
static u_char       *NNTPHost      = NULL; /**< The hostname to identify */
static u_char       *NNTPGroupName = NULL; /**< The Newsgroup name */
static unsigned int NNTPPort      = 0;    /**< The port to listen on */
static int          NNTPMayPost   = 0;    /**< May the user create new postings? (not yet implemented) */

struct sockaddr_in *NNTP_Addr     = NULL; /**< The address structure the server listens on */

u_char *html_decode(const u_char *str) {
  t_name_value *cs = cfg_get_first_value(&fo_default_conf,"ExternCharset");
  u_char *new_str;
  register u_char *ptr;
  t_string ns;

  str_init(&ns);

  if(cs) {
    if(cf_strcmp(cs->values[0],"UTF-8")) {
      if((new_str = charset_convert(str,strlen(str),"UTF-8",cs->values[0],NULL)) == NULL) {
        new_str = strdup(str);
      }
    }
    else {
      new_str = strdup(str);
    }
  }
  else {
    new_str = strdup(str);
  }

  for(ptr = new_str;*ptr;ptr++) {
    if(*ptr == '&') {
      if(cf_strncmp(ptr,"&gt;",4) == 0) {
        str_char_append(&ns,'>');
      }
      else if(cf_strncmp(ptr,"&lt;",4) == 0) {
        str_char_append(&ns,'<');
      }
      else if(cf_strncmp(ptr,"&amp;",5) == 0) {
        str_char_append(&ns,'&');
      }
      else if(cf_strncmp(ptr,"&quot;",6) == 0) {
        str_char_append(&ns,'"');
      }
      else if(cf_strncmp(ptr,"&nbsp;",6) == 0) {
        str_char_append(&ns,'\240');
      }
    }
  }

  free(new_str);
  return ns.content;
}

/**
 * This function tokenizes a NNTP command line read from the client
 * \param line The line read
 * \param tokens A reference to a u_char **; this function will allocated a vector containing all tokens
 * \return The number of tokens
 */
int flt_nntp_tokenize(u_char *line,u_char ***tokens) {
  int n = 0,reser = 5;
  register u_char *ptr,*prev;

  *tokens = fo_alloc(NULL,5,sizeof(u_char **),FO_ALLOC_MALLOC);
  if(!*tokens) return 0;

  for(prev=ptr=line;*ptr;ptr++) {
    if(n >= reser) {
      reser += 5;
      *tokens = fo_alloc(*tokens,reser,sizeof(u_char **),FO_ALLOC_REALLOC);
    }

    if(flt_nntp_is_delim(*ptr)) {
      if((ptr - 1 == prev && flt_nntp_is_delim(*(ptr-1))) || ptr == prev) {
        prev = ptr;
        continue;
      }

      *ptr = 0;
      (*tokens)[n++] = strdup(prev);
      prev = ptr+1;
    }
  }

  if(prev != ptr && (prev-1 != ptr || !flt_nntp_is_delim(*(ptr-1))) && *ptr) (*tokens)[n++] = strdup(prev);

  return n;
}

/**
 * This function parses a RFC977 date
 * \param t1 The day-month-year piece
 * \param t2 The hour-min-sec piece
 * \return (time_t)-1 on failure, the date on success
 */
time_t flt_nntp_parse_date(u_char *t1,u_char *t2) {
  static int days_in_month[12] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30 };
  register u_char *p;
  int century, year, month, day, hour, mins, secs, datelen;
  register int i;
  long seconds;
  u_char buff[8 + 6 + 1];
  struct tm current;
  struct timeval now;

  datelen = strlen(t1);
  if((datelen < 6 || datelen > 8) || strlen(t2) != 6) return -1;

  sprintf(buff,"%s%s",t1,t2);

  for(p=buff;*p;p++) {
    if(!isdigit(*p)) return -1;
  }

  p = buff + datelen - 6;

  year  = CtoI(p[0]) * 10 + CtoI(p[1]);
  month = CtoI(p[2]) * 10 + CtoI(p[3]);
  day   = CtoI(p[4]) * 10 + CtoI(p[5]);
  hour  = CtoI(p[6]) * 10 + CtoI(p[7]);
  mins  = CtoI(p[8]) * 10 + CtoI(p[9]);
  secs  = CtoI(p[10]) * 10 + CtoI(p[11]);

  if(datelen == 6) {
    if(gettimeofday(&now,NULL) == -1 || gmtime_r(&now.tv_sec,&current) == NULL) return -1;

    century = current.tm_year / 100;
    if(current.tm_year >= 100) current.tm_year -= century * 100;

    if(year <= current.tm_year) year += (century + 19) * 100;
    else year += (century + 18) * 100;
  }
  else {
    year += CtoI(*--p) * 100;
    if(datelen == 7) year += 1900; /* YYYMMDD */
    else year += CtoI(*--p) * 1000; /* YYYYMMDD */
  }

  if(month < 1 || month > 12 || day < 1 || day > 31 || mins < 0 || mins > 59 || secs < 0 || secs > 59) return -1;

  if(hour == 24) {
    hour = 0;
    day++;
  }
  else {
    if(hour < 0 || hour > 23) return -1;
  }

  for(seconds = 0, i = 1970; i < year; i++) seconds += days_in_year(i);
  if(days_in_year(year) == 366 && month > 2) seconds++;

  while(--month > 0) seconds += days_in_month[month];

  seconds += day - 1;
  seconds = 24 * seconds + hour;
  seconds = 60 * seconds + mins;
  seconds = 60 * seconds + secs;

  return seconds;
}

/**
 * This function creates the socket the server is listening to
 * \param addr A pointer to the socket address structure
 * \return -1 on failure, the socket on success
 */
int flt_nntp_set_us_up_the_socket(struct sockaddr_in *addr) {
  int sock,ret,one = 1;

  if((sock = socket(AF_INET,SOCK_STREAM,0)) == -1) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_nntp: socket: %s\n",sock,strerror(errno));
    return -1;
  }

  if((setsockopt(sock, SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one))) == -1) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_nntp: setsockopt(SO_REUSEADDR): %s\n",strerror(errno));
    close(sock);
    return -1;
  }

  memset(addr,0,sizeof(struct sockaddr_in));
  addr->sin_family = AF_INET;
  addr->sin_port   = htons(NNTPPort);

  if(NNTPInterface) {
    if((ret = inet_aton(NNTPInterface,&(addr->sin_addr))) != 0) {
      cf_log(LOG_ERR,__FILE__,__LINE__,"flt_nntp: inet_aton(\"%s\"): %s\n",NNTPInterface,strerror(ret));
      close(sock);
      return -1;
    }
  }
  else {
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
  }

  if(bind(sock,(struct sockaddr *)addr,sizeof(struct sockaddr_in)) < 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_nntp: bind: %s\n",strerror(errno));
    close(sock);
    return -1;
  }

  if(listen(sock,LISTENQ) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_nntp: listen: %s\n",strerror(errno));
    close(sock);
    return -1;
  }

  return sock;
}

/**
 * This function gets the article pointer specified by the article number
 * \param t A pointer to the actual thread structure
 * \param p A pointer to the actual posting structure
 * \param anum The actual article number
 * \param num The wanted article number
 * \return 0 on success, -1 on failure
 */
int flt_nntp_get_article_pointer(t_thread **t,t_posting **p,long *anum,long num) {
  t_thread *t1;

  *t = NULL;
  *p = NULL;

  /* ah :) first call */
  /* due to archiver runs it alwas has to be the first run... */
  CF_RW_RD(&head.lock);
  *t = head.thread;
  CF_RW_UN(&head.lock);

  CF_RW_RD(&((*t)->lock));
  *p = (*t)->postings;
  CF_RW_UN(&((*t)->lock));

  *anum = 0;

  if(*anum == num) return 0;


  /* since we're not yet at 'num', we have to go _forwards_ */
  for(t1=NULL;*t;*t=t1) {
    CF_RW_RD(&((*t)->lock));

    /* we jumped to the next thread */
    if(t1) *p = (*t)->postings;

    /* count the posts :) */
    for(;*p && *anum < num;*p=(*p)->next,*anum += 1);

    /* found? */
    if(*anum == num) {
      if(!*p) {
        t1 = (*t)->next;
        CF_RW_UN(&((*t)->lock));
        *t = t1;

        if(*t) {
          CF_RW_RD(&((*t))->lock);
          *p = (*t)->postings;
        }
        else {
          return -1;
        }
      }

      break;
    }

    t1 = (*t)->next;
    CF_RW_UN(&((*t)->lock));
    *t = t1;
  }

  if(*t && *p) return 0;
  else         return -1;

  /* hu? what happened? Compiler bug? */
  return -1;
}

/**
 * This function counts the existing articles
 * \return The number of existing articles
 */
int flt_nntp_count(void) {
  int cnt = 0;
  t_thread *t,*t1;
  t_posting *p;

  CF_RW_RD(&head.lock);
  t = head.thread;
  CF_RW_UN(&head.lock);

  while(t) {
    CF_RW_RD(&t->lock);

    for(p=t->postings;p;p=p->next) cnt++;

    t1 = t->next;
    CF_RW_UN(&t->lock);
    t = t1;
  }

  return cnt;
}

/**
 * This function counts the newlines in a posting text
 * \param p1 A pointer to the posting in which we shall count
 * \return The number of newlines
 */
long flt_nntp_count_newlines(const t_posting *p1) {
  u_char *tmp  = p1->content;
  long count = 0;

  while((tmp = strstr(tmp+1,"<br />")) != 0) ++count;

  return count;
}

/**
 * This function prepares and sends a posting body
 * \param t The thread structure of the posting
 * \param p The posting structure of the posting which we shall send
 * \param sock The client socket
 * \param dot Shall we terminate with a '.\015\012'? If yes, 1, if no, 0
 */
void flt_nntp_send_body(const t_thread *t,const t_posting *p,int sock,int dot) {
  t_string str;
  u_char *ptr,*content;

  str_init(&str);

  for(ptr=p->content;*ptr;ptr++) {
    if(cf_strncmp(ptr,"<br />",6) == 0) {
      str_chars_append(&str,"\015\012",2);
      ptr += 5;
    }
    else if(cf_strncmp(ptr,"<a href=\"",9) == 0) {
      u_char *tmp;

      ptr    += 9;
      tmp     = strstr(ptr,"\"");

      if(tmp) {
  str_char_append(&str,'<');
  str_chars_append(&str,ptr,tmp-ptr);
        str_char_append(&str,'>');

  for(;cf_strncmp(ptr+1,"</a>",4) != 0;ptr++);
        ptr += 4;
      }
      else {
  tmp -= 9;
      }
    }
    else if(cf_strncmp(ptr,"<img src=\"",10) == 0) {
      u_char *tmp;

      ptr    += 10;
      tmp     = strstr(ptr,"\"");

      if(tmp) {
  str_char_append(&str,'<');
  str_chars_append(&str,ptr,tmp-ptr);
  str_char_append(&str,'>');

        ptr = strstr(tmp,">");
      }
      else {
        ptr -= 10;
      }
    }
    else if(cf_strncmp(ptr,"[pref:",6) == 0) {
      u_char *tmp;
      u_char buff[512];
      long i;
      u_int64_t tid,mid;

      ptr += 6;
      tid = strtoull(ptr,(char **)&tmp,10);
      mid = strtoull(tmp+5,(char **)&tmp,10);

      i = snprintf(buff,512,"<t%lldm%lld@%s>",tid,mid,NNTPHost);

      ptr = tmp;
    }
    else if(*ptr == (u_char)127) {
      str_char_append(&str,'>');
    }
    else if(cf_strncmp(ptr,"_/_SIG_/_",9) == 0) {
      str_chars_append(&str,"-- \015\012",9);
      ptr += 8;
    }
    else {
      str_char_append(&str,*ptr);
    }
  }

  str_chars_append(&str,"\015\012",2);

  if(dot) str_chars_append(&str,".\015\012",3);

  content = html_decode(str.content);

  writen(sock,content,strlen(content));

  free(str.content);
  free(content);
}

/**
 * This function sends the headers of an article
 * \param t The thread structure of the article to send
 * \param p1 The posting structure of the article to send
 * \param sock The client socket
 * \param anum The number of the article
 * \param newline Shall we terminate the header with an empty line?
 */
void flt_nntp_send_headers(const t_thread *t,const t_posting *p1,int sock,long anum,int newline) {
  t_string str;
  u_char buff[250];
  struct tm tm;
  int i;

  str_init(&str);

  if(p1->user.email) {
    str_chars_append(&str,"From: \"",7);
    str_chars_append(&str,p1->user.name,p1->user.name_len);
    str_chars_append(&str,"\" ",2);

    str_char_append(&str,'<');
    str_chars_append(&str,p1->user.email,p1->user.email_len);
    str_char_append(&str,'>');
  }
  else {
    str_chars_append(&str,"From: ",6);
    str_chars_append(&str,p1->user.name,p1->user.name_len);
  }

  str_chars_append(&str,"\015\012Newsgroups: ",14);
  str_chars_append(&str,NNTPGroupName,strlen(NNTPGroupName));
  str_chars_append(&str,"\015\012Subject: ",11);
  str_chars_append(&str,p1->subject,p1->subject_len);
  str_chars_append(&str,"\015\012Message-ID: ",14);

  i = snprintf(buff,250,"<t%lldm%lld@%s>",t->tid,p1->mid,NNTPHost);
  str_chars_append(&str,buff,i);
  if(p1->category) {
    str_chars_append(&str,"\015\012X-Category: ",14);
    str_chars_append(&str,p1->category,p1->category_len);
  }

  if(p1->user.hp) {
    str_chars_append(&str,"\015\012X-Homepage: ",14);
    str_chars_append(&str,p1->user.hp,p1->user.hp_len);
  }

  if(p1->user.img) {
    str_chars_append(&str,"\015\012X-Image: ",11);
    str_chars_append(&str,p1->user.img,p1->user.img_len);
  }

  gmtime_r(&p1->date,&tm);
  str_chars_append(&str,"\015\012Date: ",8);
  i = strftime(buff,250,"%a, %d %b %Y %T %z (GMT)",&tm);
  str_chars_append(&str,buff,i);

  if(p1->prev) {
    t_posting *p2 = (t_posting *)p1;

    while(p2->prev && p2->level >= p1->level) p2 = p2->prev;

    if(p2) {
      str_chars_append(&str,"\015\012In-Reply-To: ",14);
      i = snprintf(buff,512,"<t%lldm%lld@%s>",t->tid,p2->mid,NNTPHost);
      str_chars_append(&str,buff,i);
    }
  }

  str_chars_append(&str,"\015\012Xref: ",8);
  i = snprintf(buff,512,"%s %s:%ld\015\012",NNTPHost,NNTPGroupName,anum);
  str_chars_append(&str,buff,i);


  str_chars_append(&str,"Lines: ",7);
  i = snprintf(buff,512,"%ld",flt_nntp_count_newlines(p1)+1);
  str_chars_append(&str,buff,i);
  str_chars_append(&str,"\015\012",2);

  if(newline) str_chars_append(&str,"\015\012",2);

  writen(sock,str.content,str.len);
  free(str.content);
}

/**
 * This function sends the news specified by a NEWNEWS command
 * \param sock The client socket
 * \param date The date of the last received posting
 */
void send_new_news(int sock,time_t date) {
  t_string str;
  size_t w;
  t_thread *t,*t1;
  t_posting *p;
  u_char buff[100];

  str_init(&str);

  CF_RW_RD(&head.lock);
  t = head.thread;
  CF_RW_UN(&head.lock);

  for(;t;t=t1) {
    CF_RW_RD(&t->lock);

    for(p=t->postings;p;p=p->next) {
      if(p->date >= date) {
        w = snprintf(buff,100,"<t%lldm%lld@%s>\015\012",t->tid,p->mid,NNTPHost);
        str_chars_append(&str,buff,w);
      }
    }

    t1 = t->next;
    CF_RW_UN(&t->lock);
  }

  str_chars_append(&str,".\015\012",3);
  writen(sock,str.content,str.len);

  str_cleanup(&str);
}

/**
 * This function handles a session
 * \param sock The client socket
 */
void flt_nntp_handle_request(int sock) {
  rline_t tsd;
  u_char *line;
  u_char **tokens;
  int ShallRun = 1;
  long anum = 0,tnum = 0;
  t_thread *t  = NULL;
  t_posting *p = NULL;

  memset(&tsd,0,sizeof(rline_t));

  writen(sock,"200 Classic Forum NNTP plugin ready - posting not yet implemented\015\012",67);

  while(ShallRun) {
    line = readline(sock,&tsd);

    #ifdef DEBUG
    cf_log(LOG_DBG,__FILE__,__LINE__,"flt_nntp: line: '%s'\n",line);
    #endif

    if(line) {
      tnum = flt_nntp_tokenize(line,&tokens);

      if(tnum) {
        if(cf_strcasecmp(tokens[0],"ARTICLE") == 0) {
          u_char *tmp;

          if(tnum < 2) {
            flt_nntp_syntax_error();
            flt_nntp_cleanup_req();
            continue;
          }

          if(*tokens[1] == '<') { /* article selection by message id; pointer will not be modified */
            t_thread *t1;
            t_posting *p1;

            if((t1 = cf_get_thread(strtoull(tokens[1]+1,(char **)&tmp,10))) != NULL) {
              if((p1 = cf_get_posting(t1,strtoull(tmp+1,NULL,10))) != NULL) {
                u_char buff[512];
                int i;

                i = snprintf(buff,512,"220 %ld <t%lldm%lld@%s> Article retrieved, everything follows\015\012",anum+1,t->tid,p->mid,NNTPHost);

                writen(sock,buff,i);
                flt_nntp_send_headers(t1,p1,sock,anum+1,1);
                flt_nntp_send_body(t1,p1,sock,1);
              }
            }
          }
          else { /* article selection by number; modifies pointer! */
            long i,num = strtol(tokens[1],NULL,10);

            if(flt_nntp_get_article_pointer(&t,&p,&anum,num?num-1:0) == 0) {
              u_char buff[512];

              i = snprintf(buff,512,"220 %ld <t%lldm%lld@%s> Article retrieved, text follows\015\012",num,t->tid,p->mid,NNTPHost);
              writen(sock,buff,i);
              flt_nntp_send_headers(t,p,sock,num,1);
              flt_nntp_send_body(t,p,sock,1);

              CF_RW_UN(&t->lock);
            }
            else {
              writen(sock,"430 no such article found\015\012",27);
            }
          }
        }

        else if(cf_strcasecmp(tokens[0],"BODY") == 0) {
          if(*tokens[1] == '<') {
          }
          else {
            u_char buff[512];
            int i;
            long num = strtol(tokens[1],NULL,10);

            if(flt_nntp_get_article_pointer(&t,&p,&anum,num?num-1:0) == 0) {
              CF_RW_RD(&t->lock);

              i = snprintf(buff,512,"222 %ld <t%lldm%lld@%s> article retrieved - body follows\015\012",anum+1,t->tid,p->mid,NNTPHost);
              writen(sock,buff,i);
              flt_nntp_send_body(t,p,sock,1);

              CF_RW_UN(&t->lock);
            }
            else {
              writen(sock,"430 no such article found\015\012",27);
            }
          }
        }

        else if(cf_strcasecmp(tokens[0],"MODE") == 0) {
          if(tnum != 2) {
            flt_nntp_syntax_error();
            flt_nntp_cleanup_req();
            continue;
          }

          if(cf_strcasecmp(tokens[1],"reader") == 0) {
            writen(sock,"200 Ok, go and read\015\012",21);
          }
          else {
            writen(sock,"500 hey, no streaming mode allowed!\015\012",37);
          }
        }

        else if(cf_strcasecmp(tokens[0],"HEAD") == 0) {
          if(*tokens[1] == '<') {
          }
          else {
            long x = strtol(tokens[1],NULL,10);
            int i;
            u_char buff[512];

            if(flt_nntp_get_article_pointer(&t,&p,&anum,x?x-1:0) == 0) {
              i = snprintf(buff,512,"221 %ld <t%lldm%lld@%s> head follows\015\012",anum+1,t->tid,p->mid,NNTPHost);
              writen(sock,buff,i);
              flt_nntp_send_headers(t,p,sock,anum+1,0);
              writen(sock,".\015\012",3);
            }
            else {
              writen(sock,"430 no such article found\015\012",27);
            }
          }
        }

        else if(cf_strncmp(line,"STAT",4) == 0) {
          if(tnum < 2) {
            flt_nntp_syntax_error();
            flt_nntp_cleanup_req();
            continue;
          }

          CF_RW_RD(&head.lock);
          if(1) {
            long i = 0,num = strtol(tokens[1],NULL,10);

            if(flt_nntp_get_article_pointer(&t,&p,&anum,num?num-1:0) == 0) {
              u_char buff[512];

              i = snprintf(buff,512,"230 %ld <t%lldm%lld@%s> Article retrieved - statistics only (article %ld selecetd, its message-id is <t%lldm%lld@%s>)\015\012",num,t->tid,p->mid,NNTPHost,num,t->tid,p->mid,NNTPHost);
              writen(sock,buff,i);
              CF_RW_UN(&t->lock);
            }
            else {
              writen(sock,"430 no such article found\015\012",27);
            }

            CF_RW_UN(&head.lock);
          }
        }

        else if(cf_strcasecmp(tokens[0],"GROUP") == 0) {
          if(tnum < 2) {
            flt_nntp_syntax_error();
            flt_nntp_cleanup_req();
            continue;
          }

          if(cf_strcmp(tokens[1],NNTPGroupName) == 0) {
            int n = flt_nntp_count();
            u_char buff[512];

            n = snprintf(buff,512,"211 %d 1 %d %s group selected\015\012",n,n,NNTPGroupName);
            writen(sock,buff,n);

            CF_RW_RD(&head.lock);
            t = head.thread;
            CF_RW_UN(&head.lock);

            if(t) {
              CF_RW_RD(&t->lock);
              p = head.thread->postings;
              CF_RW_UN(&t->lock);
            }
          }
          else {
            writen(sock,"411 I don't know this group.\015\012",30);
          }
        }

        else if(cf_strcasecmp(tokens[0],"LAST") == 0) {
          int i = 0;
          u_char buff[512];

          if(!t || !p) {
            writen(sock,"421 no last article in this group\015\012",35);
            flt_nntp_cleanup_req();
            continue;
          }

          CF_RW_RD(&t->lock);

          if(p->prev) {
            p = p->prev;
          }
          else {
            if(t->prev) {
              t_thread *t1;

              CF_RW_RD(&t->prev->lock);
              t1 = t->prev;
              CF_RW_UN(&t->lock);
              t  = t1;
              p  = t->last;
            }
            else {
              writen(sock,"421 no last article in this group\015\012",35);
              flt_nntp_cleanup_req();
              CF_RW_UN(&t->lock);
              continue;
            }
          }

          i = snprintf(buff,512,"223 %ld <t%lldm%lld@%s> article retrieved - request text seperately\015\012",--anum,t->tid,p->mid,NNTPHost);
          writen(sock,buff,i);

          CF_RW_UN(&t->lock);
        }

        else if(cf_strcasecmp(tokens[0],"NEXT") == 0) {
          u_char buff[512];
          int i;

          if(!p) {
            if(!t) {
              CF_RW_RD(&head.lock);
              t = head.thread;
              CF_RW_UN(&head.lock);
            }

            CF_RW_RD(&t->lock);
            p = t->postings;
            CF_RW_UN(&t->lock);
          }

          CF_RW_RD(&t->lock);

          if(!p->next) {
            if(!t->next) {
              writen(sock,"421 No next article in group\015\012",30);
              CF_RW_UN(&t->lock);
              flt_nntp_cleanup_req();
              continue;
            }
            else {
              t_thread *t2 = t->next;
              CF_RW_UN(&t->lock);
              t = t2;

              CF_RW_RD(&t->lock);
              p = t->postings;
            }
          }
          else {
            p = p->next;
          }

          i = snprintf(buff,512,"223 %ld <t%lldm%lld@%s> article retrieved - request text seperately\015\012",++anum,t->tid,p->mid,NNTPHost);
          writen(sock,buff,i);

          CF_RW_UN(&t->lock);

        }

        else if(cf_strcasecmp(tokens[0],"LIST") == 0) {
          int cnt = 0;
          u_char buff[250];

          cnt = flt_nntp_count();
          cnt = snprintf(buff,250,"%s %d 1 n\015\012.\015\012",NNTPGroupName,cnt);
          writen(sock,"215 I'm pleased to serve you\015\012",30);
          writen(sock,buff,cnt);
        }

        else if(cf_strcasecmp(tokens[0],"NEWGROUPS") == 0) {
          /* easy going ;) */
          writen(sock,"231 List of new newsgroups follows\015\012.\015\012",39);
        }

        else if(cf_strncmp(line,"NEWNEWS",7) == 0) {
          u_char *group;
          time_t date;

          if(tnum < 4) {
            flt_nntp_syntax_error();
            flt_nntp_cleanup_req();
            continue;
          }

          group = tokens[1];

          if(*group == '*' || cf_strncmp(group,NNTPGroupName,strlen(NNTPGroupName)) == 0) {
            /* search for the date */
            if((date = flt_nntp_parse_date(tokens[2],tokens[3])) < 0) {
              writen(sock,"501 command syntax error\015\012",26);
            }
            else {
              if(tnum < 5) {
                struct tm local,gmt;
                time_t now;
                long tz;

                if(gmtime_r(&now,&gmt) == NULL || localtime_r(&now,&local) == NULL) {
                  writen(sock,"501 command syntax error\015\012",26);
                }
                else {
                  tz = (gmt.tm_hour - local.tm_hour) * 60 + gmt.tm_min - local.tm_min;
                  date += tz;

                  writen(sock,"230 list of new articles by message-id follows\015\012",48);
                  send_new_news(sock,date);
                }
              }
            }
          }
          else {
            writen(sock,"411 no such newsgroup\015\012",23);
          }

        }

        else if(cf_strcasecmp(tokens[0],"POST") == 0) {
        }

        else if(cf_strcasecmp(tokens[0],"QUIT") == 0) {
          writen(sock,"205 Go away!\015\012",14);
          ShallRun = 0;
        }
        else {
          writen(sock,"500 Hu? Go and read RFC 977\015\012",29);
        }


        flt_nntp_cleanup_req();
      }
    }
    else {
      ShallRun = 0;
    }
  }

  close(sock);
}

/**
 * This function creates the server socket and initializes the plugin
 * \param main_sock The main socket of the server (not needed)
 * \return FLT_OK on success, FLT_EXIT on failure
 */
int flt_nntp_run(int main_sock) {
  int sock;

  if(!NNTPPort) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_nntp: we need a port to bind on!\n");
    return FLT_EXIT;
  }
  if(!NNTPHost) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_nntp: we need a hostname!\n");
    return FLT_EXIT;
  }

  if(!NNTPGroupName) NNTPGroupName = strdup("alt.cforum");


  NNTP_Addr = calloc(1,sizeof(struct sockaddr_in));
  sock      = flt_nntp_set_us_up_the_socket(NNTP_Addr);

  if(sock < 0) return FLT_EXIT;

  if(cf_push_server(sock,(struct sockaddr *)NNTP_Addr,sizeof(struct sockaddr_in),flt_nntp_handle_request) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"cf_push_server returned not 0!\n");
    return FLT_EXIT;
  }

  return FLT_OK;
}

/**
 * This function handles a configuration command
 * \param cf The configfile structure
 * \param opt The configuration option entry
 * \param arg1 The first command argument
 * \param arg2 The second command argument
 * \param arg3 The third command argument
 * \return != 0 on failure, 0 on success
 */
int flt_nntp_handle_command(t_configfile *cf,t_conf_opt *opt,u_char **args,int argnum) {
  if(argnum == 1) {
    if(cf_strcmp(opt->name,"NNTPInterface") == 0) {
      if(NNTPInterface) free(NNTPInterface);
      NNTPInterface = strdup(args[0]);
    }
    else if(cf_strcmp(opt->name,"NNTPHost") == 0) {
      if(NNTPHost) free(NNTPHost);
      NNTPHost = strdup(args[0]);
    }
    else if(cf_strcmp(opt->name,"NNTPMayPost") == 0) {
      if(cf_strcasecmp(args[0],"YES") == 0) NNTPMayPost = 1;
    }
    else if(cf_strcmp(opt->name,"NNTPGroupName") == 0) {
      if(NNTPGroupName) free(NNTPGroupName);
      NNTPGroupName = strdup(args[0]);
    }
    else {
      if((NNTPPort = (unsigned int)strtol(args[0],NULL,10)) <= 1024) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"flt_nntp: Sorry, ports <= 1024 are not supported. Reasonable would be something\nlike 2048 or 4096.\n");
        return 1;
      }
    }
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_nntp: Directive %s expects one argument!\n",opt->name);
    return 1;
  }

  return 0;
}

/**
 * This function cleans up the plugin (e.g. it frees allocated memory, it closes the server socket, etc)
 */
void flt_nntp_cleanup(void) {
  if(NNTPInterface) free(NNTPInterface);
  if(NNTPHost)      free(NNTPHost);
  if(NNTPGroupName) free(NNTPGroupName);
  if(NNTP_Addr)     free(NNTP_Addr);
}

/**
 * The configuration options provided by this plugin
 */
t_conf_opt flt_nntp_config[] = {
  { "NNTPPort",      flt_nntp_handle_command, NULL },
  { "NNTPInterface", flt_nntp_handle_command, NULL },
  { "NNTPHost",      flt_nntp_handle_command, NULL },
  { "NNTPMayPost",   flt_nntp_handle_command, NULL },
  { "NNTPGroupName", flt_nntp_handle_command, NULL },
  { NULL, NULL, NULL }
};

/**
 * The handler hooks defined by this plugin
 */
t_handler_config flt_nntp_handlers[] = {
  { INIT_HANDLER, flt_nntp_run },
  { 0, NULL }
};

/**
 * Module configuration
 */
t_module_config flt_nntp = {
  flt_nntp_config,
  flt_nntp_handlers,
  NULL,
  NULL,
  NULL,
  flt_nntp_cleanup
};

/* eof */

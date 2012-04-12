/*
    Copyright (C) 2005-2007 Cockos Incorporated

    Wahjam is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Wahjam is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Wahjam; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*

  This file does the setup and configuration file management for the server.
  Note that the kernel of the server is in Server.cpp/.h and usercon.cpp/.h,
  which includes a Server class (high-level server object), User_Connection
  class (manages a user), and a User_Group class (manages a jam).

*/



#ifdef _WIN32
#include <ctype.h>
#include <windows.h>
#include <conio.h>
#else
#include <stdlib.h>
#include <string.h>
#include <time.h>
#endif
#include <signal.h>
#include <stdarg.h>

#include <QCoreApplication>

#include "../../WDL/jnetlib/jnetlib.h"
#include "../../WDL/jnetlib/httpget.h"
#include "../netmsg.h"
#include "../mpb.h"
#include "usercon.h"

#include "../../WDL/rng.h"
#include "../../WDL/sha.h"
#include "../../WDL/lineparse.h"
#include "../../WDL/string.h"

#include "Server.h"
#include "ninjamsrv.h"

#define VERSION "v0.06"

static const char *startupmessage = "Wahjam Server " VERSION " built on " __DATE__ " at " __TIME__ " starting up...\n" "Copyright (C) 2005-2007, Cockos, Inc.\n";

static FILE *g_logfp;
static ServerConfig g_config;
static Server *g_server;

class localUserInfoLookup : public IUserInfoLookup
{
public:
  localUserInfoLookup(char *name)
  {
    username.Set(name);
  }
  ~localUserInfoLookup()
  {
  }

  int Run()
  {
    // perform lookup here

    user_valid=0;

    if (!strncmp(username.Get(),"anonymous",9) && (!username.Get()[9] || username.Get()[9] == ':'))
    {
      logText("got anonymous request (%s)\n",g_config.allowAnonymous?"allowing":"denying");
      if (!g_config.allowAnonymous) return 1;

      user_valid=1;
      reqpass=0;

      WDL_String tmp(username.Get());

      if (tmp.Get()[9] == ':' && tmp.Get()[10])
      {
        username.Set(tmp.Get()+10);

        int cnt=16;
        char *p=username.Get();
        while (*p)
        {
          if (!cnt--)
          {
            *p=0;
            break;
          }
          if (*p == '@' || *p == '.') *p='_';
          p++;
        }
      }
      else username.Set("anon");

      username.Append("@");
      username.Append(hostmask.Get());

      if (g_config.anonymousMaskIP)
      {
        char *p=username.Get();
        while (*p) p++;
        while (p > username.Get() && *p != '.' && *p != '@') p--;
        if (*p == '.' && p[1])
        {
          p[1]='x';
          p[2]=0;
        }
      }

      privs=(g_config.allowAnonChat?PRIV_CHATSEND:0) | (g_config.allowAnonymousMulti?PRIV_ALLOWMULTI:0) | PRIV_VOTE;
      max_channels=g_config.maxchAnon;
    }
    else
    {
      int x;
      logText("got login request for '%s'\n",username.Get());
      if (g_config.statusUser.Get()[0] && !strcmp(username.Get(), g_config.statusUser.Get()))
      {
        user_valid=1;
        reqpass=1;
        is_status=1;
        privs=0; 
        max_channels=0;

        WDL_SHA1 shatmp;
        shatmp.add(username.Get(),strlen(username.Get()));
        shatmp.add(":",1);
        shatmp.add(g_config.statusPass.Get(), strlen(g_config.statusPass.Get()));

        shatmp.result(sha1buf_user);
      }
      else for (x = 0; x < g_config.userlist.GetSize(); x ++)
      {
        if (!strcmp(username.Get(), g_config.userlist.Get(x)->name.Get()))
        {
          user_valid=1;
          reqpass=1;

          char *pass = g_config.userlist.Get(x)->pass.Get();
          WDL_SHA1 shatmp;
          shatmp.add(username.Get(),strlen(username.Get()));
          shatmp.add(":",1);
          shatmp.add(pass,strlen(pass));

          shatmp.result(sha1buf_user);

          privs = g_config.userlist.Get(x)->priv_flag;
          max_channels=g_config.maxchUser;
          break;
        }
      }
    }

    return 1;
  }

};


static IUserInfoLookup *myCreateUserLookup(char *username)
{
  return new localUserInfoLookup(username);
}




static int ConfigOnToken(ServerConfig *config, LineParser *lp)
{
  QString token = QString(lp->gettoken_str(0)).toLower();
  if (token == QString("Port").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;
    int p=lp->gettoken_int(1);
    if (!p) return -2;
    config->port=p;
  }
  else if (token == QString("StatusUserPass").toLower())
  {
    if (lp->getnumtokens() != 3) return -1;
    config->statusUser.Set(lp->gettoken_str(1));
    config->statusPass.Set(lp->gettoken_str(2));
  }
  else if (token == QString("MaxUsers").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;
    int p=lp->gettoken_int(1);
    config->maxUsers=p;
  }
  else if (token == QString("PIDFile").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;
    config->pidFilename.Set(lp->gettoken_str(1));
  }
  else if (token == QString("LogFile").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;
    config->logFilename.Set(lp->gettoken_str(1));
  }
  else if (token == QString("SessionArchive").toLower())
  {
    if (lp->getnumtokens() != 3) return -1;
    config->logPath.Set(lp->gettoken_str(1));
    config->logSessionLen = lp->gettoken_int(2);
  }
  else if (token == QString("SetUID").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;
    config->setuid = lp->gettoken_int(1);
  }
  else if (token == QString("DefaultBPI").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;
    int p = lp->gettoken_int(1);
    if (p < MIN_BPI) {
      p = MIN_BPI;
    } else if (p > MAX_BPI) {
      p = MAX_BPI;
    }
    config->defaultBPI=lp->gettoken_int(1);
  }
  else if (token == QString("DefaultBPM").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;
    int p = lp->gettoken_int(1);
    if (p < MIN_BPM) {
      p = MIN_BPM;
    } else if (p > MAX_BPM) {
      p = MAX_BPM;
    }
    config->defaultBPM = p;
  }
  else if (token == QString("DefaultTopic").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;
    config->defaultTopic.Set(lp->gettoken_str(1));
  }
  else if (token == QString("MaxChannels").toLower())
  {
    if (lp->getnumtokens() != 2 && lp->getnumtokens() != 3) return -1;

    config->maxchUser=lp->gettoken_int(1);
    config->maxchAnon=lp->gettoken_int(lp->getnumtokens()>2?2:1);
  }
  else if (token == QString("SetKeepAlive").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;
    int p = lp->gettoken_int(1);
    if (p < 0 || p > 255) {
      p = 0;
    }
    config->keepAlive = p;
  }
  else if (token == QString("SetVotingThreshold").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;
    config->votingThreshold = lp->gettoken_int(1);
  }
  else if (token == QString("SetVotingVoteTimeout").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;
    config->votingTimeout = lp->gettoken_int(1);
  }
  else if (token == QString("ServerLicense").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;
    FILE *fp=fopen(lp->gettoken_str(1),"rt");
    if (!fp) 
    {
      printf("Error opening license file %s\n",lp->gettoken_str(1));
      if (g_logfp)
        logText("Error opening license file %s\n",lp->gettoken_str(1));
      return -2;
    }
    config->license.Set("");
    for (;;)
    {
      char buf[1024];
      buf[0]=0;
      fgets(buf,sizeof(buf),fp);
      if (!buf[0]) break;
      config->license.Append(buf);
    }

    fclose(fp);
    
  }
  else if (token == QString("ACL").toLower())
  {
    if (lp->getnumtokens() != 3) return -1;
    int suc=0;
    char *v=lp->gettoken_str(1);
    char *t=strstr(v,"/");
    if (t)
    {
      *t++=0;
      QHostAddress hostaddr(v);
      if (hostaddr != QHostAddress::Null)
      {
        int maskbits=atoi(t);
        if (maskbits >= 0 && maskbits <= 32)
        {
          int flag=lp->gettoken_enum(2,"allow\0deny\0reserve\0");
          if (flag >= 0)
          {
            suc=1;
            unsigned long mask = 0xffffffff << maskbits;
            config->acl.add(hostaddr.toIPv4Address(), mask, flag);
          }
        }
      }
    }

    if (!suc)
    {
      if (g_logfp)
        logText("Usage: ACL xx.xx.xx.xx/X [ban|allow|reserve]\n");
      printf("Usage: ACL xx.xx.xx.xx/X [ban|allow|reserve]\n");
      return -2;
    }
  }
  else if (token == QString("User").toLower())
  {
    if (lp->getnumtokens() != 3 && lp->getnumtokens() != 4) return -1;
    UserPassEntry *p=new UserPassEntry;
    p->name.Set(lp->gettoken_str(1));
    p->pass.Set(lp->gettoken_str(2));
    if (lp->getnumtokens()>3)
    {
      char *ptr=lp->gettoken_str(3);
      while (*ptr)
      {
        if (*ptr == '*') p->priv_flag|=~PRIV_HIDDEN; // everything but hidden if * used
        else if (*ptr == 'T' || *ptr == 't') p->priv_flag |= PRIV_TOPIC;
        else if (*ptr == 'B' || *ptr == 'b') p->priv_flag |= PRIV_BPM;
        else if (*ptr == 'C' || *ptr == 'c') p->priv_flag |= PRIV_CHATSEND;
        else if (*ptr == 'K' || *ptr == 'k') p->priv_flag |= PRIV_KICK;        
        else if (*ptr == 'R' || *ptr == 'r') p->priv_flag |= PRIV_RESERVE;        
        else if (*ptr == 'M' || *ptr == 'm') p->priv_flag |= PRIV_ALLOWMULTI;
        else if (*ptr == 'H' || *ptr == 'h') p->priv_flag |= PRIV_HIDDEN;       
        else if (*ptr == 'V' || *ptr == 'v') p->priv_flag |= PRIV_VOTE;               
        else 
        {
          if (g_logfp)
            logText("Warning: Unknown user priviledge flag '%c'\n",*ptr);
          printf("Warning: Unknown user priviledge flag '%c'\n",*ptr);
        }
        ptr++;
      }
    }
    else p->priv_flag=PRIV_CHATSEND|PRIV_VOTE;// default privs
    config->userlist.Add(p);
  }
  else if (token == QString("AllowHiddenUsers").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;

    int x=lp->gettoken_enum(1,"no\0yes\0");
    if (x <0)
    {
      return -2;
    }
    config->allowHiddenUsers = x;
  }
  else if (token == QString("AnonymousUsers").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;

    int x=lp->gettoken_enum(1,"no\0yes\0multi\0");
    if (x <0)
    {
      return -2;
    }
    config->allowAnonymous = x;
    config->allowAnonymousMulti = x == 2;
  }
  else if (token == QString("AnonymousMaskIP").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;

    int x=lp->gettoken_enum(1,"no\0yes\0");
    if (x <0)
    {
      return -2;
    }
    config->anonymousMaskIP = x;
  }
  else if (token == QString("AnonymousUsers").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;

    int x=lp->gettoken_enum(1,"no\0yes\0");
    if (x <0)
    {
      return -2;
    }
    config->allowAnonymous = x;
  }
  else if (token == QString("AnonymousUsersCanChat").toLower())
  {
    if (lp->getnumtokens() != 2) return -1;

    int x=lp->gettoken_enum(1,"no\0yes\0");
    if (x <0)
    {
      return -2;
    }
    config->allowAnonChat = x;
  }
  else return -3;
  return 0;
}


static int ReadConfig(ServerConfig *config, char *configfile)
{
  bool comment_state=0;
  int linecnt=0;
  WDL_String linebuild;
  if (g_logfp) logText("[config] reloading configuration file\n"); // TODO move this elsewhere
  FILE *fp=strcmp(configfile,"-")?fopen(configfile,"rt"):stdin; 
  if (!fp)
  {
    printf("[config] error opening configfile '%s'\n",configfile);
    if (g_logfp) logText("[config] error opening config file (console request)\n"); // TODO move this elsewhere
    return -1;
  }

  config->allowAnonChat = true;
  config->allowAnonymous = false;
  config->allowAnonymousMulti = false;
  config->anonymousMaskIP = false;
  config->allowHiddenUsers = false;
  config->setuid = -1;
  config->defaultBPM = 120;
  config->defaultBPI = 8;
  config->port = 2049;
  config->keepAlive = 0;
  config->maxUsers = 0; // unlimited users
  config->maxchAnon = 2;
  config->maxchUser = 32;
  config->logSessionLen = 10; // ten minute default, tho the user will need to specify the path anyway
  config->votingThreshold = 110;
  config->votingTimeout = 120;
  config->logPath.Set("");
  config->pidFilename.Set("");
  config->logFilename.Set("");
  config->statusPass.Set("");
  config->statusUser.Set("");
  config->license.Set("");
  config->defaultTopic.Set("");
  config->acl.clear();

  int x;
  for(x=0; x < config->userlist.GetSize(); x++)
  {
    delete config->userlist.Get(x);
  }
  config->userlist.Empty();

  for (;;)
  {
    char buf[8192];
    buf[0]=0;
    fgets(buf,sizeof(buf),fp);
    linecnt++;
    if (!buf[0]) break;
    if (buf[strlen(buf)-1]=='\n') buf[strlen(buf)-1]=0;

    LineParser lp(comment_state);

    if (buf[0] && buf[strlen(buf)-1]=='\\')
    {
      buf[strlen(buf)-1]=0;
      linebuild.Append(buf);
      continue;
    }
    linebuild.Append(buf);

    int res=lp.parse(linebuild.Get());

    linebuild.Set("");

    if (res)
    {
      if (res==-2) 
      {
        if (g_logfp) logText("[config] warning: unterminated string parsing line %d of %s\n",linecnt,configfile);
        printf("[config] warning: unterminated string parsing line %d of %s\n",linecnt,configfile);
      }
      else 
      {
        if (g_logfp) logText("[config] warning: error parsing line %d of %s\n",linecnt,configfile);
        printf("[config] warning: error parsing line %d of %s\n",linecnt,configfile);
      }
    }
    else
    {
      comment_state = lp.InCommentBlock();

      if (lp.getnumtokens()>0)
      {
        int err = ConfigOnToken(config, &lp);
        if (err)
        {
          if (err == -1)
          {
            if (g_logfp) logText("[config] warning: wrong number of tokens on line %d of %s\n",linecnt,configfile);
            printf("[config] warning: wrong number of tokens on line %d of %s\n",linecnt,configfile);
          }
          if (err == -2)
          {
            if (g_logfp) logText("[config] warning: invalid parameter on line %d of %s\n",linecnt,configfile);
            printf("[config] warning: invalid parameter on line %d of %s\n",linecnt,configfile);
          }
          if (err == -3)
          {
            if (g_logfp) logText("[config] warning: invalid config command \"%s\" on line %d of %s\n",lp.gettoken_str(0),linecnt,configfile);
            printf("[config] warning: invalid config command \"%s\" on line %d of %s\n",lp.gettoken_str(0),linecnt,configfile);
          }
        }
      }
    }
  }

  if (g_logfp) logText("[config] reload complete\n");

  if (fp != stdin) fclose(fp);
  return 0;
}

static int g_reloadconfig;
static int g_done;


void sighandler(int sig)
{
  if (sig == SIGINT)
  {
    g_done=1;
  }
#ifndef _WIN32
  if (sig == SIGHUP)
  {
    g_reloadconfig=1;
  }
#endif
}

void usage(const char *progname)
{
    printf("Usage: %s config.cfg [options]\n"
           "Options (override config file):\n"
#ifndef _WIN32
           "  -pidfile <filename.pid>\n"
#endif
           "  -logfile <filename.log>\n"
           "  -archive <path_to_archive>\n"
           "  -port <port>\n"
#ifndef _WIN32
           "  -setuid <uid>\n"
#endif
      , progname);
    exit(1);
}

void logText(const char *s, ...)
{
    if (g_logfp) 
    {      
      time_t tv;
      time(&tv);
      struct tm *t=localtime(&tv);
      fprintf(g_logfp,"[%04d/%02d/%02d %02d:%02d:%02d] ",t->tm_year+1900,t->tm_mon,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec);
    }


    va_list ap;
    va_start(ap,s);

    vfprintf(g_logfp?g_logfp:stdout,s,ap);    

    if (g_logfp) fflush(g_logfp);

    va_end(ap);
}

bool reloadConfig(int argc, char **argv, bool firstTime)
{
  if (!firstTime) {
    logText("reloading config...\n");
  }

  if (ReadConfig(&g_config, argv[1]) != 0) {
    return false;
  }

  int p;
  for (p = 2; p < argc; p ++)
  {
      if (!strcmp(argv[p],"-pidfile"))
      {
        if (++p >= argc) usage(argv[0]);
        g_config.pidFilename.Set(argv[p]);
      }
      else if (!strcmp(argv[p],"-logfile"))
      {
        if (++p >= argc) usage(argv[0]);
        g_config.logFilename.Set(argv[p]);
      }
      else if (!strcmp(argv[p],"-archive"))
      {
        if (++p >= argc) usage(argv[0]);
        g_config.logPath.Set(argv[p]);
      }
      else if (!strcmp(argv[p],"-setuid"))
      {
        if (++p >= argc) usage(argv[0]);
        g_config.setuid=atoi(argv[p]);
      }
      else if (!strcmp(argv[p],"-port"))
      {
        if (++p >= argc) usage(argv[0]);
        g_config.port=atoi(argv[p]);
      }
      else usage(argv[0]);
  }

  return g_server->setConfig(&g_config);
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  if (argc < 2)
  {
    usage(argv[0]);
  }

  JNL::open_socketlib();

  User_Group *group = new User_Group;
  group->CreateUserLookup=myCreateUserLookup;

  g_server = new Server(group);

  printf("%s", startupmessage);

  if (!reloadConfig(argc, argv, true)) {
    printf("Error loading config file!\n");
    exit(1);
  }

#ifdef _WIN32
  DWORD v=GetTickCount();
  WDL_RNG_addentropy(&v,sizeof(v));
  v=(DWORD)time(NULL);
  WDL_RNG_addentropy(&v,sizeof(v));
#else
  time_t v=time(NULL);
  WDL_RNG_addentropy(&v,sizeof(v));
  int pid=getpid();
  WDL_RNG_addentropy(&pid,sizeof(pid));

  if (g_config.setuid != -1) setuid(g_config.setuid);

  if (g_config.pidFilename.Get()[0])
  {
    FILE *fp=fopen(g_config.pidFilename.Get(),"w");
    if (fp)
    {
      fprintf(fp,"%d\n",pid);
      fclose(fp);
    }
    else printf("Error opening PID file '%s'\n", g_config.pidFilename.Get());
  }



  signal(SIGPIPE,sighandler);
  signal(SIGHUP,sighandler);
#endif
  signal(SIGINT,sighandler);


  if (g_config.logFilename.Get()[0])
  {
    g_logfp=fopen(g_config.logFilename.Get(),"at");
    if (!g_logfp)
      printf("Error opening log file '%s'\n",g_config.logFilename.Get());
    else
      logText("Opened log. Wahjam Server %s built on %s at %s\n",VERSION,__DATE__,__TIME__);

  }

  logText("Server starting up...\n");

  while (!g_done)
  {
    if (g_server->run())
    {
      app.processEvents(QEventLoop::AllEvents, 1 /* milliseconds */);

      if (g_reloadconfig && strcmp(argv[1],"-"))
      {
        g_reloadconfig=0;
        reloadConfig(argc, argv, false);
      }
    }
  }

  logText("Shutting down server\n");

  delete g_server;

  if (g_logfp)
  {
    fclose(g_logfp);
    g_logfp=0;
  }

  JNL::close_socketlib();
	return 0;
}

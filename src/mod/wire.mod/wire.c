/* 
 * wire.c   - An encrypted partyline communication.
 *            Compatible with wire.tcl.
 *
 *            by ButchBub - Scott G. Taylor (staylor@mrynet.com) 
 *
 *      REQUIRED: Eggdrop Modules (see dependancies below).  
 *                Wire will only compile and work on the latest Required
 *                Eggdrop version listed below.
 *
 *    Version   Date            Req'd Eggver    Notes                    Who
 *    .......   ..........      ............    ....................     ......
 *      1.0     1997-07-17      1.2.0           Initial.                 BB
 *      1.1     1997-07-28      1.2.0           Release version.         BB
 *      1.2     1997-08-19      1.2.1           Update and bugfixes.     BB
 *      1.3     1997-09-24      1.2.2.0         Reprogrammed for 1.2.2   BB
 *      1.4     1997-11-25      1.2.2.0         Added language addition  Kirk
 *      1.5     1998-07-12      1.3.0.0         Fixed ;me and updated    BB
 *
 *   This module does not support wire usage in the files area.
 *
 */

#define MAKING_WIRE
#define MODULE_NAME "wire"
#include "../module.h"
#include "../../users.h"
#include "../../chan.h"
#include <time.h>
#include "wire.h"

#undef global
static Function *global = NULL, *blowfish_funcs = NULL;

typedef struct wire_t {
  int sock;
  char *crypt;
  char *key;
  struct wire_t *next;
} wire_list;

static wire_list *wirelist;

static cmd_t wire_bot[] =
{
  {0, 0, 0, 0},			/* Saves having to malloc :P */
};

static void wire_leave();
static void wire_join();
static void wire_display();

static int wire_expmem()
{
  wire_list *w = wirelist;
  int size = 0;

  context;
  while (w) {
    size += sizeof(wirelist);
    size += strlen(w->crypt);
    size += strlen(w->key);
    w = w->next;
  }
  return size;
}

static void nsplit(char *to, char *from)
{
  char *x, *y = from;

  x = newsplit(&y);
  strcpy(to, x);
  strcpy(from, y);
}

static void wire_filter(char *from, char *cmd, char *param)
{
  char wirecrypt[512];
  char wirewho[512];
  char wiretmp2[512];
  char wiretmp[512];
  char wirereq[512];
  wire_list *w = wirelist;
  char reqsock;
  time_t now2 = now;
  char idle[20];
  char *enctmp;

  context;
  strcpy(wirecrypt, &cmd[5]);
  strcpy(wiretmp, param);
  nsplit(wirereq, param);

/*
 * !wire<crypt"wire"> !wirereq <destbotsock> <crypt"destbotnick">
 * -----  wirecrypt    wirereq    wirewho         param
 */

  if (!strcmp(wirereq, "!wirereq")) {
    context;
    nsplit(wirewho, param);
    while (w) {
      if (!strcmp(w->crypt, wirecrypt)) {
	reqsock = atoi(wirewho);
	if (now2 - dcc[findanyidx(w->sock)].timeval > 300) {
	  unsigned long Days, hrs, mins;

	  Days = (now2 - dcc[findanyidx(w->sock)].timeval) / 86400;
	  hrs = ((now2 - dcc[findanyidx(w->sock)].timeval) -
		 (Days * 86400)) / 3600;
	  mins = ((now2 - dcc[findanyidx(w->sock)].timeval) -
		  (hrs * 3600)) / 60;
	  if (Days > 0)
	    sprintf(idle, " \[%s %lud%luh]", WIRE_IDLE, Days, hrs);
	  else if (hrs > 0)
	    sprintf(idle, " \[%s %luh%lum]", WIRE_IDLE, hrs, mins);
	  else
	    sprintf(idle, " \[%s %lum]", WIRE_IDLE, mins);
	} else
	  idle[0] = 0;
	sprintf(wirereq, "----- %c%-9s %-9s  %s%s",
		geticon(findanyidx(w->sock)),
		dcc[findanyidx(w->sock)].nick,
		botnetnick,
		dcc[findanyidx(w->sock)].host, idle);
	enctmp = encrypt_string(w->key, wirereq);
	strcpy(wiretmp, enctmp);
	nfree(enctmp);
	sprintf(wirereq, "zapf %s %s !wire%s !wireresp %s %s %s",
		botnetnick, from, wirecrypt, wirewho, param, wiretmp);
	dprintf(nextbot(from), "%s\n", wirereq);
	if (dcc[findanyidx(w->sock)].u.chat->away) {
	  sprintf(wirereq, "-----    %s: %s\n", WIRE_AWAY,
		  dcc[findanyidx(w->sock)].u.chat->away);
	  enctmp = encrypt_string(w->key, wirereq);
	  strcpy(wiretmp, enctmp);
	  nfree(enctmp);
	  sprintf(wirereq, "zapf %s %s !wire%s !wireresp %s %s %s",
		  botnetnick, from, wirecrypt, wirewho, param, wiretmp);
	  dprintf(nextbot(from), "%s\n", wirereq);
	}
      }
      w = w->next;
    }
    return;
  }
  if (!strcmp(wirereq, "!wireresp")) {
    context;
    nsplit(wirewho, param);
    reqsock = atoi(wirewho);
    w = wirelist;
    nsplit(wiretmp2, param);
    while (w) {
      if (w->sock == reqsock) {
	enctmp = decrypt_string(w->key, wiretmp2);
	strcpy(wirewho, enctmp);
	nfree(enctmp);
	if (!strcmp(dcc[findanyidx(reqsock)].nick, wirewho)) {
	  enctmp = decrypt_string(w->key, param);
	  dprintf(findanyidx(reqsock), "%s\n", enctmp);
	  nfree(enctmp);
	  return;
	}
      }
      w = w->next;
    }
    return;
  }
  context;
  while (w) {
    if (!strcmp(wirecrypt, w->crypt))
      wire_display(findanyidx(w->sock), w->key, wirereq, param);
    w = w->next;
  }
}

static void wire_display(int idx, char *key, char *from, char *message)
{
  char *enctmp;

  context;
  enctmp = decrypt_string(key, message);
  if (from[0] == '!')
    dprintf(idx, "----- > %s %s\n", &from[1], enctmp + 1);
  else
    dprintf(idx, "----- <%s> %s\n", from, enctmp);
  nfree(enctmp);
}

static int cmd_wirelist(struct userrec *u, int idx, char *par)
{
  wire_list *w = wirelist;
  int entry = 0;

  context;
  dprintf(idx, "Current Wire table:  (Base table address = %U)\n", w);
  while (w) {
    dprintf(idx, "entry %d: w=%U  idx=%d  sock=%d  next=%U\n",
	    ++entry, w, findanyidx(w->sock), w->sock, w->next);
    w = w->next;
  }
  return 0;
}

static int cmd_onwire(struct userrec *u, int idx, char *par)
{
  wire_list *w, *w2;
  char wiretmp[512], wirecmd[512], idxtmp[512];
  char idle[20], *enctmp;
  time_t now2 = now;

  context;
  w = wirelist;
  while (w) {
    if (w->sock == dcc[idx].sock)
      break;
    w = w->next;
  }
  if (!w) {
    dprintf(idx, "%s\n", WIRE_NOTONWIRE);
    return 0;
  }
  dprintf(idx, "----- %s '%s':\n", WIRE_CURRENTLYON, w->key);
  dprintf(idx, "----- Nick       Bot        Host\n");
  dprintf(idx, "----- ---------- ---------- ------------------------------\n");
  enctmp = encrypt_string(w->key, "wire");
  sprintf(wirecmd, "!wire%s", enctmp);
  nfree(enctmp);
  enctmp = encrypt_string(w->key, dcc[idx].nick);
  strcpy(wiretmp, enctmp);
  nfree(enctmp);
  simple_sprintf(idxtmp, "!wirereq %d %s", dcc[idx].sock, wiretmp);
  botnet_send_zapf_broad(-1, botnetnick, wirecmd, idxtmp);
  w2 = wirelist;
  while (w2) {
    if (!strcmp(w2->key, w->key)) {
      if (now2 - dcc[findanyidx(w2->sock)].timeval > 300) {
	unsigned long Days, hrs, mins;

	Days = (now2 - dcc[findanyidx(w2->sock)].timeval) / 86400;
	hrs = ((now2 - dcc[findanyidx(w2->sock)].timeval) - (Days * 86400)) / 3600;
	mins = ((now2 - dcc[findanyidx(w2->sock)].timeval) - (hrs * 3600)) / 60;
	if (Days > 0)
	  sprintf(idle, " \[%s %lud%luh]", WIRE_IDLE, Days, hrs);
	else if (hrs > 0)
	  sprintf(idle, " \[%s %luh%lum]", WIRE_IDLE, hrs, mins);
	else
	  sprintf(idle, " \[%s %lum]", WIRE_IDLE, mins);
      } else
	idle[0] = 0;
      dprintf(idx, "----- %c%-9s %-9s  %s%s\n",
	      geticon(findanyidx(w2->sock)),
	      dcc[findanyidx(w2->sock)].nick,
	      botnetnick, dcc[findanyidx(w2->sock)].host, idle);
      if (dcc[findanyidx(w2->sock)].u.chat->away)
	dprintf(idx, "-----    %s: %s\n", WIRE_AWAY,
		dcc[findanyidx(w2->sock)].u.chat->away);
    }
    w2 = w2->next;
  }
  return 0;
}

static int cmd_wire(struct userrec *u, int idx, char *par)
{
  wire_list *w = wirelist;

  context;
  if (!par[0]) {
    dprintf(idx, "%s: .wire [<encrypt-key>|OFF|info]\n", USAGE);
    return 0;
  }
  while (w) {
    if (w->sock == dcc[idx].sock)
      break;
    w = w->next;
  }
  if (!strcasecmp(par, "off")) {
    if (w) {
      wire_leave(w->sock);
      dprintf(idx, "%s\n", WIRE_NOLONGERWIRED);
      return 0;
    }
    dprintf(idx, "%s\n", WIRE_NOTONWIRE);
    return 0;
  }
  if (!strcasecmp(par, "info")) {
    if (w)
      dprintf(idx, "%s '%s'.\n", WIRE_CURRENTLYON, w->key);
    else
      dprintf(idx, "%s\n", WIRE_NOTONWIRE);
    return 0;
  }
  if (w) {
    dprintf(idx, "%s %s...\n", WIRE_CHANGINGKEY, par);
    wire_leave(w->sock);
  } else {
    dprintf(idx, "----- %s\n", WIRE_INFO1);
    dprintf(idx, "----- %s\n", WIRE_INFO2);
    dprintf(idx, "----- %s\n", WIRE_INFO3);
  }
  wire_join(idx, par);
  cmd_onwire((struct userrec *) 0, idx, "");
  return 0;
}

static char *chof_wire(char *from, int idx)
{
  wire_leave(dcc[idx].sock);
  return NULL;
}

static void wire_join(int idx, char *key)
{
  char wirecmd[512];
  char wiremsg[512];
  char wiretmp[512];
  char *enctmp;
  wire_list *w = wirelist, *w2;

  context;
  while (w) {
    if (w->next == 0)
      break;
    w = w->next;
  }
  if (!wirelist) {
    wirelist = (wire_list *) nmalloc(sizeof(wire_list));
    w = wirelist;
  } else {
    w->next = (wire_list *) nmalloc(sizeof(wire_list));
    w = w->next;
  }
  w->sock = dcc[idx].sock;
  w->key = (char *) nmalloc(strlen(key) + 1);
  strcpy(w->key, key);
  w->next = 0;
  context;
  enctmp = encrypt_string(w->key, "wire");
  strcpy(wiretmp, enctmp);
  nfree(enctmp);
  w->crypt = (char *) nmalloc(strlen(wiretmp) + 1);
  strcpy(w->crypt, wiretmp);
  sprintf(wirecmd, "!wire%s", wiretmp);
  sprintf(wiremsg, "%s joined wire '%s'", dcc[idx].nick, key);
  enctmp = encrypt_string(w->key, wiremsg);
  strcpy(wiretmp, enctmp);
  nfree(enctmp);
  {
    char x[1024];

    simple_sprintf(x, "%s %s", botnetnick, wiretmp);
    botnet_send_zapf_broad(-1, botnetnick, wirecmd, x);
  }
  w2 = wirelist;
  while (w2) {
    if (!strcmp(w2->key, w->key))
      dprintf(findanyidx(w2->sock), "----- %s %s '%s'.\n",
	      dcc[findanyidx(w->sock)].nick, WIRE_JOINED, w2->key);
    w2 = w2->next;
  }
  w2 = wirelist;
  while (w2) {			/* Is someone using this key here already? */
    if (w2 != w)
      if (!strcmp(w2->key, w->key))
	break;
    w2 = w2->next;
  }
  if (!w2) {			/* Someone else is NOT using this key, so
				 * we add a bind */
    wire_bot[0].name = wirecmd;
    wire_bot[0].flags = "";
    wire_bot[0].func = (Function) wire_filter;
    add_builtins(H_bot, wire_bot, 1);
  }
}

static void wire_leave(int sock)
{
  char wirecmd[513];
  char wiremsg[513];
  char wiretmp[513];
  char *enctmp;
  wire_list *w = wirelist;
  wire_list *w2 = wirelist;
  wire_list *wlast = wirelist;

  context;
  while (w) {
    if (w->sock == sock)
      break;
    w = w->next;
  }
  if (!w)
    return;
  enctmp = encrypt_string(w->key, "wire");
  strcpy(wirecmd, enctmp);
  nfree(enctmp);
  sprintf(wiretmp, "%s left the wire.", dcc[findanyidx(w->sock)].nick);
  enctmp = encrypt_string(w->key, wiretmp);
  strcpy(wiremsg, enctmp);
  nfree(enctmp);
  {
    char x[1024];

    simple_sprintf(x, "!wire%s %s", wirecmd, botnetnick);
    botnet_send_zapf_broad(-1, botnetnick, x, wiremsg);
  }
  w2 = wirelist;
  while (w2) {
    if (w2->sock != sock && !strcmp(w2->key, w->key)) {
      dprintf(findanyidx(w2->sock), "----- %s %s\n",
	      dcc[findanyidx(w2->sock)].nick, WIRE_LEFT);
    }
    w2 = w2->next;
  }
  /* Check to see if someone else is using this wire key.
   * If so, then don't remove the wire filter binding */
  w2 = wirelist;
  while (w2) {
    if (w2 != w && !strcmp(w2->key, w->key))
      break;
    w2 = w2->next;
  }
  if (!w2) {			/* Someone else is NOT using this key */
    wire_bot[0].name = wirecmd;
    wire_bot[0].flags = "";
    wire_bot[0].func = (Function) wire_filter;
    rem_builtins(H_bot, wire_bot, 1);
  }
  w2 = wirelist;
  wlast = 0;
  while (w2) {
    if (w2 == w)
      break;
    wlast = w2;
    w2 = w2->next;
  }
  if (wlast) {
    if (w->next)
      wlast->next = w->next;
    else
      wlast->next = 0;
  } else if (!w->next)
    wirelist = 0;
  else
    wirelist = w->next;
  nfree(w->crypt);
  nfree(w->key);
  nfree(w);
}

static char *cmd_putwire(int idx, char *message)
{
  wire_list *w = wirelist;
  wire_list *w2 = wirelist;
  int wiretype;
  char wirecmd[512];
  char wiremsg[512];
  char wiretmp[512];
  char wiretmp2[512];
  char *enctmp;

  context;
  while (w) {
    if (w->sock == dcc[idx].sock)
      break;
    w = w->next;
  }
  if (!w)
    return "";
  if (!message[1])
    return "";
  if ((strlen(message) > 3) && !strncmp(&message[1], "me", 2)
      && (message[3] == ' ')) {
    sprintf(wiretmp2, "!%s@%s", dcc[idx].nick, botnetnick);
    enctmp = encrypt_string(w->key, &message[3]);
    wiretype = 1;
  } else {
    sprintf(wiretmp2, "%s@%s", dcc[idx].nick, botnetnick);
    enctmp = encrypt_string(w->key, &message[1]);
    wiretype = 0;
  }
  strcpy(wiremsg, enctmp);
  nfree(enctmp);
  enctmp = encrypt_string(w->key, "wire");
  strcpy(wiretmp, enctmp);
  nfree(enctmp);
  sprintf(wirecmd, "!wire%s", wiretmp);
  sprintf(wiretmp, "%s %s", wiretmp2, wiremsg);
  botnet_send_zapf_broad(-1, botnetnick, wirecmd, wiretmp);
  sprintf(wiretmp, "%s%s", wiretype ? "!" : "", dcc[findanyidx(w->sock)].nick);
  while (w2) {
    if (!strcmp(w2->key, w->key))
      wire_display(findanyidx(w2->sock), w2->key, wiretmp, wiremsg);
    w2 = w2->next;
  }
  return "";
}

/* a report on the module status */
static void wire_report(int idx, int details)
{
  int wiretot = 0;
  wire_list *w = wirelist;

  context;
  while (w) {
    wiretot++;
    w = w->next;
  }
  if (details)
    dprintf(idx, "   %d wire%s using %d bytes\n", wiretot,
	    wiretot == 1 ? "" : "s", wire_expmem());
}

static cmd_t wire_dcc[] =
{
  {"wire", "", cmd_wire, 0},
  {"onwire", "", cmd_onwire, 0},
  {"wirelist", "n", cmd_wirelist, 0},
};

static cmd_t wire_chof[] =
{
  {"*", "", (Function) chof_wire, "wire:chof"},
};

static cmd_t wire_filt[] =
{
  {";*", "", (Function) cmd_putwire, "wire:filt"},
};

static char *wire_close()
{
  wire_list *w = wirelist;
  char wiretmp[512];
  char *enctmp;
  p_tcl_bind_list H_temp;

  /* Remove any current wire encrypt bindings
   * for now, don't worry about duplicate unbinds */
  context;
  while (w) {
    enctmp = encrypt_string(w->key, "wire");
    sprintf(wiretmp, "!wire%s", enctmp);
    nfree(enctmp);
    wire_bot[0].name = wiretmp;
    wire_bot[0].flags = "";
    wire_bot[0].func = (Function) wire_filter;
    rem_builtins(H_bot, wire_bot, 1);
    w = w->next;
  }
  w = wirelist;
  while (w && w->sock) {
    dprintf(findanyidx(w->sock), "----- %s\n", WIRE_UNLOAD);
    dprintf(findanyidx(w->sock), "----- %s\n", WIRE_NOLONGERWIRED);
    wire_leave(w->sock);
    w = wirelist;
  }
  rem_builtins(H_dcc, wire_dcc, 3);
  H_temp = find_bind_table("filt");
  rem_builtins(H_temp, wire_filt, 1);
  H_temp = find_bind_table("chof");
  rem_builtins(H_temp, wire_chof, 1);
  module_undepend(MODULE_NAME);
  return NULL;
}

char *wire_start();

static Function wire_table[] =
{
  (Function) wire_start,
  (Function) wire_close,
  (Function) wire_expmem,
  (Function) wire_report,
};

char *wire_start(Function * global_funcs)
{
  module_entry *me;
  p_tcl_bind_list H_temp;

  global = global_funcs;

  context;
  module_register(MODULE_NAME, wire_table, 2, 0);
  if (!module_depend(MODULE_NAME, "eggdrop", 103, 0)) {
    module_undepend(MODULE_NAME);
    return WIRE_VERSIONERROR;
  }
  me = module_find("encryption", 2, 0);
  blowfish_funcs = me->funcs;
  add_builtins(H_dcc, wire_dcc, 3);
  H_temp = find_bind_table("filt");
  add_builtins(H_filt, wire_filt, 1);
  H_temp = find_bind_table("chof");
  add_builtins(H_chof, wire_chof, 1);
  wirelist = 0;
  /* Absolutely HUGE! change to this module
   * Ah well, at least the language file gets loaded 'on demand' - Kirk */
  add_lang_section("wire");
  /* Even better thing is, all the modules can be lang'ed now :)
   * and you just need this one line at the bottom of the code - Kirk */
  return NULL;
}
